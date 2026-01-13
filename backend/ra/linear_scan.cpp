#include <backend/ra/linear_scan.h>
#include <backend/mir/m_function.h>
#include <backend/mir/m_instruction.h>
#include <backend/mir/m_block.h>
#include <backend/mir/m_defs.h>
#include <backend/target/target_reg_info.h>
#include <backend/target/target_instr_adapter.h>
#include <backend/common/cfg.h>
#include <backend/common/cfg_builder.h>
#include <utils/dynamic_bitset.h>
#include <debug.h>

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <algorithm>

namespace BE::RA
{
    /*
     * 线性扫描寄存器分配（Linear Scan）教学版说明
     *
     * 目标：将每个虚拟寄存器（vreg）的活跃区间映射到目标机的物理寄存器或栈槽（溢出）。
     *
     * 核心步骤（整数/浮点分开执行，流程相同）：
     * 1) 指令线性化与编号：为函数内所有指令分配全局顺序号，记录每个基本块的 [start, end) 区间，
     *    同时收集调用点（callPoints），用于偏好分配被调用者保存寄存器（callee-saved）。
     * 2) 构建 USE/DEF：枚举每条指令的使用与定义寄存器，聚合到基本块级的 USE/DEF 集合。
     * 3) 活跃性分析：在 CFG 上迭代 IN/OUT，满足 IN = USE ∪ (OUT − DEF) 直至收敛。
     * 4) 活跃区间构建：按基本块从后向前，根据 IN/OUT 与指令次序，累积每个 vreg 的若干 [start, end) 段并合并。
     * 5) 标记跨调用：若区间与任意调用点重叠（交叉），标记 crossesCall=true，以便后续优先使用被调用者保存寄存器。
     * 6) 线性扫描分配：将区间按起点排序，维护活动集合 active；到达新区间时先移除已过期区间，然后
     *    尝试选择空闲物理寄存器；若无空闲则选择一个区间溢出（常见启发：溢出“结束点更远”的区间）。
     * 7) 重写 MIR：对未分配物理寄存器的 use/def，在指令前/后插入 reload/spill，并用临时物理寄存器替换操作数。
     *
     * 提示：
     * - 通过 TargetInstrAdapter 提供的接口完成目标无关的指令读写。
     * - TargetRegInfo 提供了可分配寄存器集合、被调用者保存寄存器、保留寄存器等信息。
     */
    namespace
    {
        struct Segment
        {
            int start;
            int end;
            Segment(int s = 0, int e = 0) : start(s), end(e) {}
        };
        struct Interval
        {
            BE::Register         vreg;
            std::vector<Segment> segs;
            bool                 crossesCall = false;

            void addSegment(int s, int e)
            {
                if (s >= e) return;
                segs.emplace_back(s, e);
            }
            void merge() {
                if (segs.size() <= 1) return;
                std::sort(segs.begin(), segs.end(),
                          [](const Segment& a, const Segment& b) {
                              return (a.start < b.start) || (a.start == b.start && a.end < b.end);
                          });
                size_t out = 0;
                for (size_t i = 0; i < segs.size(); ++i)
                {
                    if (out == 0 || segs[i].start > segs[out - 1].end)
                    {
                        segs[out++] = segs[i];
                    }
                    else if (segs[i].end > segs[out - 1].end)
                    {
                        segs[out - 1].end = segs[i].end;
                    }
                }
                segs.resize(out);
            }
        };

        struct IntervalOrder
        {
            bool operator()(const Interval* a, const Interval* b) const {
                const Segment& as = a->segs.front();
                const Segment& bs = b->segs.front();
                if (as.start != bs.start) return as.start < bs.start;
                if (as.end != bs.end) return as.end < bs.end;
                return a->vreg < b->vreg;
            }
        };
    }  // namespace

    static std::vector<int> buildAllocatableInt(const BE::Targeting::TargetRegInfo& ri)
    {
        const auto& all = ri.intRegs();
        const auto& reserved = ri.reservedRegs();
        if (reserved.empty()) return all;

        std::vector<int> sortedReserved = reserved;
        std::sort(sortedReserved.begin(), sortedReserved.end());

        std::vector<int> out;
        out.reserve(all.size());
        for (int r : all)
        {
            if (!std::binary_search(sortedReserved.begin(), sortedReserved.end(), r))
                out.push_back(r);
        }
        return out;
    }
    static std::vector<int> buildAllocatableFloat(const BE::Targeting::TargetRegInfo& ri)
    {
        const auto& all = ri.floatRegs();
        const auto& reserved = ri.reservedRegs();
        if (reserved.empty()) return all;

        std::vector<int> sortedReserved = reserved;
        std::sort(sortedReserved.begin(), sortedReserved.end());

        std::vector<int> out;
        out.reserve(all.size());
        for (int r : all)
        {
            if (!std::binary_search(sortedReserved.begin(), sortedReserved.end(), r))
                out.push_back(r);
        }
        return out;
    }

    void LinearScanRA::allocateFunction(BE::Function& func, const BE::Targeting::TargetRegInfo& regInfo)
    {
        ASSERT(BE::Targeting::g_adapter && "TargetInstrAdapter is not set");

        std::map<BE::Block*, std::pair<int, int>>                                   blockRange;
        std::vector<std::pair<BE::Block*, std::deque<BE::MInstruction*>::iterator>> id2iter;
        std::set<int>                                                               callPoints;
        int                                                                         ins_id = 0;
        for (auto& [bid, block] : func.blocks)
        {
            int start = ins_id;
            for (auto it = block->insts.begin(); it != block->insts.end(); ++it, ++ins_id)
            {
                id2iter.emplace_back(block, it);
                if (BE::Targeting::g_adapter->isCall(*it)) callPoints.insert(ins_id);
            }
            blockRange[block] = {start, ins_id};
        }

        std::map<BE::Block*, std::set<BE::Register>> USE, DEF;
        for (auto& [bid, block] : func.blocks)
        {
            std::set<BE::Register> use, def;
            for (auto it = block->insts.begin(); it != block->insts.end(); ++it)
            {
                std::vector<BE::Register> uses, defs;
                BE::Targeting::g_adapter->enumUses(*it, uses);
                BE::Targeting::g_adapter->enumDefs(*it, defs);
                for (auto& d : defs)
                    if (!def.count(d)) def.insert(d);
                for (auto& u : uses)
                    if (!def.count(u)) use.insert(u);
            }
            USE[block] = std::move(use);
            DEF[block] = std::move(def);
        }

        // ============================================================================
        // 构建 CFG 后继关系
        // ============================================================================
        // 作用：搭建活跃性数据流的图结构。
        // 如何做：可直接用 MIR::CFGBuilder 生成 CFG，再转换为 succs 映射。
        BE::MIR::CFG*                                 cfg = nullptr;
        std::map<BE::Block*, std::vector<BE::Block*>> succs;
        {
            BE::MIR::CFGBuilder cfgBuilder(BE::Targeting::g_adapter);
            cfg = cfgBuilder.buildCFGForFunction(&func);
            for (auto& [bid, block] : func.blocks)
            {
                if (!cfg || block->blockId >= cfg->graph.size())
                {
                    succs[block] = {};
                    continue;
                }
                succs[block] = cfg->graph[block->blockId];
            }
        }

        // ============================================================================
        // 活跃性分析（IN/OUT）
        // ============================================================================
        // IN[b] = USE[b] ∪ (OUT[b] − DEF[b])，OUT[b] = ⋃ IN[s]，其中 s ∈ succs[b]
        // 迭代执行上述操作直到不变为止
        std::map<BE::Block*, std::set<BE::Register>> IN, OUT;
        bool                                         changed = true;
        while (changed)
        {
            changed = false;
            for (auto& [bid, block] : func.blocks)
            {
                std::set<BE::Register> newOUT;
                for (auto* s : succs[block])
                {
                    auto it = IN.find(s);
                    if (it != IN.end()) newOUT.insert(it->second.begin(), it->second.end());
                }
                std::set<BE::Register> newIN = USE[block];

                for (auto& r : newOUT)
                    if (!DEF[block].count(r)) newIN.insert(r);

                if (!(newOUT != OUT[block] || newIN != IN[block])) continue;

                OUT[block] = std::move(newOUT);
                IN[block]  = std::move(newIN);
                changed    = true;
            }
        }

        delete cfg;

        // ============================================================================
        // 构建活跃区间（Intervals）
        // ============================================================================
        // 作用：得到每个 vreg 的若干 [start,end) 段并合并（interval.merge()）。
        // 如何做：对每个基本块，反向遍历其指令序列，根据 IN/OUT/uses/defs 更新段的开始/结束。
        std::map<BE::Register, Interval> intervals;
        for (auto& [bid, block] : func.blocks)
        {
            const auto rangeIt = blockRange.find(block);
            if (rangeIt == blockRange.end()) continue;
            const int blockStart = rangeIt->second.first;
            const int blockEnd   = rangeIt->second.second;

            std::set<BE::Register> live;
            for (const auto& r : OUT[block])
                if (r.isVreg) live.insert(r);

            std::map<BE::Register, size_t> openSeg;
            for (const auto& r : live)
            {
                auto& interval = intervals[r];
                interval.vreg  = r;
                interval.segs.emplace_back(blockEnd, blockEnd);
                openSeg[r] = interval.segs.size() - 1;
            }

            int pos = blockEnd - 1;
            std::vector<BE::Register> uses;
            std::vector<BE::Register> defs;
            for (auto it = block->insts.rbegin(); it != block->insts.rend(); ++it, --pos)
            {
                uses.clear();
                defs.clear();
                BE::Targeting::g_adapter->enumUses(*it, uses);
                BE::Targeting::g_adapter->enumDefs(*it, defs);

                for (const auto& d : defs)
                {
                    if (!d.isVreg) continue;
                    auto openIt = openSeg.find(d);
                    if (openIt != openSeg.end())
                    {
                        intervals[d].segs[openIt->second].start = pos;
                        openSeg.erase(openIt);
                    }
                    live.erase(d);
                }

                for (const auto& u : uses)
                {
                    if (!u.isVreg) continue;
                    auto openIt = openSeg.find(u);
                    if (openIt == openSeg.end())
                    {
                        live.insert(u);
                        auto& interval = intervals[u];
                        interval.vreg  = u;
                        interval.segs.emplace_back(pos, pos + 1);
                        openSeg[u] = interval.segs.size() - 1;
                    }
                    else
                    {
                        intervals[u].segs[openIt->second].start = pos;
                    }
                }
            }

            for (const auto& [r, idx] : openSeg)
                intervals[r].segs[idx].start = blockStart;
        }

        for (auto& [r, interval] : intervals)
            interval.merge();

        if (!callPoints.empty())
        {
            for (auto& [r, interval] : intervals)
            {
                if (interval.segs.empty()) continue;
                for (const auto& seg : interval.segs)
                {
                    auto it = callPoints.lower_bound(seg.start);
                    if (it != callPoints.end() && *it < seg.end)
                    {
                        interval.crossesCall = true;
                        break;
                    }
                }
            }
        }

        // ============================================================================
        // 线性扫描主循环
        // ============================================================================
        // 作用：按区间起点排序；进入新区间前，先从活动集合 active 移除“已结束”的区间；
        // 然后尝试分配空闲物理寄存器；若无可用，执行溢出策略（如“溢出结束点更远”的区间）。
        auto allIntRegs   = buildAllocatableInt(regInfo);
        auto allFloatRegs = buildAllocatableFloat(regInfo);
        std::map<BE::Register, int> assignedPhys;
        std::map<BE::Register, int> spillFrameIndex;

        auto intervalStart = [](const Interval* itv) { return itv->segs.front().start; };
        auto intervalEnd   = [](const Interval* itv) { return itv->segs.back().end; };

        auto ensureSpillSlot = [&](const BE::Register& r) -> int {
            auto it = spillFrameIndex.find(r);
            if (it != spillFrameIndex.end()) return it->second;
            int size = r.dt ? r.dt->getDataWidth() : 8;
            int fi   = func.frameInfo.createSpillSlot(size, size);
            spillFrameIndex[r] = fi;
            return fi;
        };

        auto buildOrder = [](const std::vector<int>& all, const std::vector<int>& callee, bool preferCallee) {
            if (callee.empty()) return all;
            std::vector<int> order;
            order.reserve(all.size());
            std::set<int> calleeSet(callee.begin(), callee.end());
            if (preferCallee)
            {
                for (int r : all)
                    if (calleeSet.count(r)) order.push_back(r);
                for (int r : all)
                    if (!calleeSet.count(r)) order.push_back(r);
            }
            else
            {
                for (int r : all)
                    if (!calleeSet.count(r)) order.push_back(r);
                for (int r : all)
                    if (calleeSet.count(r)) order.push_back(r);
            }
            return order;
        };

        auto allocateForClass = [&](std::vector<Interval*>& work, const std::vector<int>& allRegs,
                                    const std::vector<int>& calleeSaved) {
            if (work.empty()) return;
            std::sort(work.begin(), work.end(), IntervalOrder());

            const auto orderCalleeFirst = buildOrder(allRegs, calleeSaved, true);
            const auto orderCallerFirst = buildOrder(allRegs, calleeSaved, false);
            std::unordered_set<int> calleeSet(calleeSaved.begin(), calleeSaved.end());
            std::vector<int> calleeOnly;
            calleeOnly.reserve(calleeSaved.size());
            for (int r : allRegs)
            {
                if (calleeSet.count(r)) calleeOnly.push_back(r);
            }

            std::vector<Interval*> active;
            active.reserve(allRegs.size());

            auto expireOld = [&](int startPos) {
                size_t out = 0;
                for (Interval* itv : active)
                {
                    if (intervalEnd(itv) <= startPos)
                        continue;
                    active[out++] = itv;
                }
                active.resize(out);
            };

            for (Interval* current : work)
            {
                expireOld(intervalStart(current));

                const auto& order = current->crossesCall ? calleeOnly : orderCallerFirst;
                int         chosen = -1;
                for (int r : order)
                {
                    bool used = false;
                    for (Interval* itv : active)
                    {
                        auto it = assignedPhys.find(itv->vreg);
                        if (it != assignedPhys.end() && it->second == r)
                        {
                            used = true;
                            break;
                        }
                    }
                    if (!used)
                    {
                        chosen = r;
                        break;
                    }
                }

                if (chosen >= 0)
                {
                    assignedPhys[current->vreg] = chosen;
                    active.push_back(current);
                    continue;
                }

                Interval* spill = current;
                int       spillEnd = intervalEnd(current);
                for (Interval* itv : active)
                {
                    auto physIt = assignedPhys.find(itv->vreg);
                    if (physIt == assignedPhys.end()) continue;
                    if (current->crossesCall && !calleeSet.count(physIt->second)) continue;

                    int end = intervalEnd(itv);
                    if (end > spillEnd)
                    {
                        spillEnd = end;
                        spill    = itv;
                    }
                }

                if (spill != current)
                {
                    int phys = assignedPhys[spill->vreg];
                    assignedPhys.erase(spill->vreg);
                    ensureSpillSlot(spill->vreg);
                    assignedPhys[current->vreg] = phys;

                    size_t out = 0;
                    for (Interval* itv : active)
                    {
                        if (itv == spill) continue;
                        active[out++] = itv;
                    }
                    active.resize(out);
                    active.push_back(current);
                }
                else
                {
                    ensureSpillSlot(current->vreg);
                }
            }
        };

        std::vector<Interval*> intIntervals;
        std::vector<Interval*> floatIntervals;
        intIntervals.reserve(intervals.size());
        floatIntervals.reserve(intervals.size());
        for (auto& [r, interval] : intervals)
        {
            if (interval.segs.empty() || !r.isVreg || !r.dt) continue;
            if (r.dt->dt == BE::DataType::Type::FLOAT)
                floatIntervals.push_back(&interval);
            else if (r.dt->dt == BE::DataType::Type::INT)
                intIntervals.push_back(&interval);
        }

        allocateForClass(intIntervals, allIntRegs, regInfo.calleeSavedIntRegs());
        allocateForClass(floatIntervals, allFloatRegs, regInfo.calleeSavedFloatRegs());

        // ============================================================================
        // 重写 MIR（插入 reload/spill，替换 use/def）
        // ============================================================================
        // 作用：将未分配物理寄存器的 use/def 改写为使用 scratch + FILoad/FIStore（由 Adapter 注入）。
        // 如何做：
        // - 对每条指令枚举 uses：若该 vreg 分配了物理寄存器，则直接替换；
        //   否则在指令前插入 reload 到一个 scratch，然后用 scratch 替换 use。
        // - 对每条指令枚举 defs：若分配了物理寄存器则直接替换；
        //   否则先将 def 写到一个 scratch，再在指令后插入 spill 到对应 FI。
        std::unordered_map<BE::MInstruction*, int> instPos;
        instPos.reserve(id2iter.size());
        for (size_t i = 0; i < id2iter.size(); ++i)
        {
            instPos.emplace(*id2iter[i].second, static_cast<int>(i));
        }

        std::unordered_map<int, std::vector<const Interval*>> physToIntervalsInt;
        std::unordered_map<int, std::vector<const Interval*>> physToIntervalsFloat;
        physToIntervalsInt.reserve(assignedPhys.size());
        physToIntervalsFloat.reserve(assignedPhys.size());
        for (const auto& [vreg, phys] : assignedPhys)
        {
            auto it = intervals.find(vreg);
            if (it == intervals.end()) continue;
            if (vreg.dt && vreg.dt->dt == BE::DataType::Type::FLOAT)
                physToIntervalsFloat[phys].push_back(&it->second);
            else
                physToIntervalsInt[phys].push_back(&it->second);
        }

        auto isLiveAt = [](const Interval* itv, int pos) {
            for (const auto& seg : itv->segs)
            {
                if (pos < seg.start) break;
                if (pos >= seg.start && pos < seg.end) return true;
            }
            return false;
        };

        auto isPhysLive = [&](int phys, int pos, const std::unordered_map<int, std::vector<const Interval*>>& physMap) {
            auto it = physMap.find(phys);
            if (it == physMap.end()) return false;
            for (const Interval* itv : it->second)
            {
                if (isLiveAt(itv, pos)) return true;
            }
            return false;
        };

        for (auto& [bid, block] : func.blocks)
        {
            std::vector<BE::MInstruction*> original(block->insts.begin(), block->insts.end());
            for (BE::MInstruction* inst : original)
            {
                auto posIt = instPos.find(inst);
                if (posIt == instPos.end()) continue;
                const int pos = posIt->second;

                std::vector<BE::Register> uses;
                std::vector<BE::Register> defs;
                BE::Targeting::g_adapter->enumUses(inst, uses);
                BE::Targeting::g_adapter->enumDefs(inst, defs);

                std::vector<BE::Register> physRegs;
                BE::Targeting::g_adapter->enumPhysRegs(inst, physRegs);
                std::unordered_set<int> forbidden;
                forbidden.reserve(physRegs.size() + uses.size() + defs.size());
                for (const auto& pr : physRegs)
                    forbidden.insert(pr.rId);

                for (const auto& r : uses)
                {
                    if (!r.isVreg) continue;
                    auto it = assignedPhys.find(r);
                    if (it != assignedPhys.end()) forbidden.insert(it->second);
                }
                for (const auto& r : defs)
                {
                    if (!r.isVreg) continue;
                    auto it = assignedPhys.find(r);
                    if (it != assignedPhys.end()) forbidden.insert(it->second);
                }

                std::unordered_set<int> usedScratchInt;
                std::unordered_set<int> usedScratchFloat;
                usedScratchInt.reserve(allIntRegs.size());
                usedScratchFloat.reserve(allFloatRegs.size());

                auto pickScratch = [&](const BE::Register& vreg) -> BE::Register {
                    const auto& regs = (vreg.dt && vreg.dt->dt == BE::DataType::Type::FLOAT) ? allFloatRegs : allIntRegs;
                    auto& usedScratch =
                        (vreg.dt && vreg.dt->dt == BE::DataType::Type::FLOAT) ? usedScratchFloat : usedScratchInt;
                    const auto& physMap =
                        (vreg.dt && vreg.dt->dt == BE::DataType::Type::FLOAT) ? physToIntervalsFloat : physToIntervalsInt;
                    
                    // First pass: try allocatable registers
                    for (int r : regs)
                    {
                        if (forbidden.count(r) || usedScratch.count(r)) continue;
                        if (isPhysLive(r, pos, physMap)) continue;
                        usedScratch.insert(r);
                        forbidden.insert(r);
                        return BE::Register(r, vreg.dt, false);
                    }
                    
                    // Second pass: try argument registers (a0-a7/fa0-fa7) as scratch
                    // These are safe to use as scratch if they're not currently live
                    const auto& argRegs = (vreg.dt && vreg.dt->dt == BE::DataType::Type::FLOAT) 
                        ? regInfo.floatArgRegs() : regInfo.intArgRegs();
                    for (int r : argRegs)
                    {
                        if (forbidden.count(r) || usedScratch.count(r)) continue;
                        if (isPhysLive(r, pos, physMap)) continue;
                        usedScratch.insert(r);
                        forbidden.insert(r);
                        return BE::Register(r, vreg.dt, false);
                    }
                    
                    ERROR("No available scratch register for spill/reload");
                    return BE::Register();
                };

                std::map<BE::Register, BE::Register> scratchMap;
                std::set<BE::Register>               reloaded;
                std::set<BE::Register>               spilledDef;

                auto findIt = [&]() {
                    auto it = std::find(block->insts.begin(), block->insts.end(), inst);
                    ASSERT(it != block->insts.end());
                    return it;
                };

                for (const auto& u : uses)
                {
                    if (!u.isVreg) continue;
                    auto physIt = assignedPhys.find(u);
                    if (physIt != assignedPhys.end())
                    {
                        BE::Register phys(physIt->second, u.dt, false);
                        BE::Targeting::g_adapter->replaceUse(inst, u, phys);
                        continue;
                    }

                    auto scratchIt = scratchMap.find(u);
                    if (scratchIt == scratchMap.end())
                    {
                        BE::Register scratch = pickScratch(u);
                        scratchIt = scratchMap.emplace(u, scratch).first;
                    }
                    if (!reloaded.count(u))
                    {
                        auto it = findIt();
                        BE::Targeting::g_adapter->insertReloadBefore(block, it, scratchIt->second, ensureSpillSlot(u));
                        reloaded.insert(u);
                    }
                    BE::Targeting::g_adapter->replaceUse(inst, u, scratchIt->second);
                }

                for (const auto& d : defs)
                {
                    if (!d.isVreg) continue;
                    auto physIt = assignedPhys.find(d);
                    if (physIt != assignedPhys.end())
                    {
                        BE::Register phys(physIt->second, d.dt, false);
                        BE::Targeting::g_adapter->replaceDef(inst, d, phys);
                        continue;
                    }

                    auto scratchIt = scratchMap.find(d);
                    if (scratchIt == scratchMap.end())
                    {
                        BE::Register scratch = pickScratch(d);
                        scratchIt = scratchMap.emplace(d, scratch).first;
                    }
                    BE::Targeting::g_adapter->replaceDef(inst, d, scratchIt->second);
                    if (!spilledDef.count(d))
                    {
                        auto it = findIt();
                        BE::Targeting::g_adapter->insertSpillAfter(block, it, scratchIt->second, ensureSpillSlot(d));
                        spilledDef.insert(d);
                    }
                }
            }
        }
    }
}  // namespace BE::RA



