#include <middleend/pass/basic_mem2reg.h>
#include <middleend/module/ir_operand.h>

#include <utility>
#include <algorithm>

namespace ME
{
    void BasicMem2RegPass::runOnModule(Module& module)
    {
        for (auto* func : module.functions) runOnFunction(*func);
    }

    void BasicMem2RegPass::runOnFunction(Function& function)
    {
        // 收集信息（一次遍历）
        std::unordered_map<RegId, AllocaInfo> infos;
        collectFunctionAllocaInfos(function, infos);

        // 记录需要删除的指令和需要替换的寄存器
        std::unordered_set<Instruction*> delSet;
        RegMap                           renameMap; // 源寄存器 -> 目标寄存器

        // 处理每个alloca
        for (auto& [rid, info] : infos)
        {
            // Case 1：没有任何 use（load）
            if (info.loads.empty())
            {
                markDeadAllocaDeletes(info, delSet);
                continue;
            }

            // Case 2：所有 use/def 都在同一个基本块
            Block* uniqUseBlock = nullptr;
            if (info.useBlocks.size() == 1)
            {
                uniqUseBlock = *info.useBlocks.begin();
                bool defOk =
                    (info.defBlocks.empty() || (info.defBlocks.size() == 1 && *info.defBlocks.begin() == uniqUseBlock));
                if (defOk)
                {
                    // 仅当所有store的写入值均为寄存器时才进行块内替换
                    bool allStoreValsAreRegs = true;
                    for (auto* si : info.stores)
                    {
                        if (!si->val || si->val->getType() != OperandType::REG)
                        {
                            allStoreValsAreRegs = false;
                            break;
                        }
                    }
                    if (!allStoreValsAreRegs)
                    {
                        // 跳过该 alloca，避免产生无法替换为常量导致的不一致
                        continue;
                    }

                    // 遍历块、记录替换并标记删除
                    bool canDeleteAlloca = processLocalAllocaInBlock(uniqUseBlock, info, renameMap, delSet);
                    if (canDeleteAlloca)
                    {
                        // 所有相关 load 均已删除，删除该 alloca
                        delSet.insert(info.alloc);
                    }
                    continue;
                }
            }
        }

        // 常数次遍历：应用寄存器替换，删除标记指令
        applySrcRegRename(function, renameMap);
        applyBatchDelete(function, delSet);
    }

    void BasicMem2RegPass::collectFunctionAllocaInfos(Function& function, std::unordered_map<RegId, AllocaInfo>& infos)
    {
        // 一次遍历所有基本块与指令，收集标量 alloca 与其直接 load/store
        for (auto& [bid, block] : function.blocks)
        {
            for (auto* inst : block->insts)
            {
                switch (inst->opcode)
                {
                    // 遇到alloca，建立AllocaInfo：记录alloc指令、结果寄存器 regId、所在块
                    case Operator::ALLOCA:
                    {
                        auto* ai = static_cast<AllocaInst*>(inst);
                        // 仅处理标量（dims为空）
                        if (!ai->dims.empty()) break;

                        RegId rid = ai->res->getRegNum();
                        auto  it  = infos.find(rid);
                        if (it == infos.end())
                        {
                            AllocaInfo info;
                            info.alloc      = ai;
                            info.regId      = rid;
                            info.allocBlock = block;
                            infos.emplace(rid, std::move(info));
                        }
                        break;
                    }
                    // 遇到store，若ptr为寄存器且匹配某alloca的regId，则加入stores，并在defBlocks记录其所在块
                    case Operator::STORE:
                    {
                        auto* si = static_cast<StoreInst*>(inst);
                        if (si->ptr->getType() == OperandType::REG)
                        {
                            RegId rid = si->ptr->getRegNum();
                            auto  it  = infos.find(rid);
                            if (it != infos.end())
                            {
                                it->second.stores.push_back(si);
                                it->second.defBlocks.insert(block);
                            }
                        }
                        break;
                    }
                    // 遇到load，若ptr为寄存器且匹配regId，则加入loads，并在useBlocks记录其所在块
                    case Operator::LOAD:
                    {
                        auto* li = static_cast<LoadInst*>(inst);
                        if (li->ptr->getType() == OperandType::REG)
                        {
                            RegId rid = li->ptr->getRegNum();
                            auto  it  = infos.find(rid);
                            if (it != infos.end())
                            {
                                it->second.loads.push_back(li);
                                it->second.useBlocks.insert(block);
                            }
                        }
                        break;
                    }
                    default: break;
                }
            }
        }
    }

    void BasicMem2RegPass::markDeadAllocaDeletes(const AllocaInfo& info, std::unordered_set<Instruction*>& delSet)
    {
        // 删除该 alloca 以及所有对其指针的 store
        if (info.alloc) delSet.insert(info.alloc);
        for (auto* si : info.stores) delSet.insert(si);
        // 无 load，故无需寄存器替换
    }

    bool BasicMem2RegPass::processLocalAllocaInBlock(
        Block* block, const AllocaInfo& info, RegMap& renameMap, std::unordered_set<Instruction*>& delSet)
    {
        // 单次遍历该基本块，维护最近一次 store 的值（仅支持寄存器值）
        RegId currentValReg    = 0;
        bool  hasCurrentReg    = false;
        bool  allLoadsReplaced = true;  // 若遇到无法替换的 load（即在首个寄存器 store 之前），则置为 false

        for (auto* inst : block->insts)
        {
            if (inst->opcode == Operator::STORE)
            {
                auto* si = static_cast<StoreInst*>(inst);
                if (si->ptr->getType() == OperandType::REG && si->ptr->getRegNum() == info.regId)
                {
                    if (si->val && si->val->getType() == OperandType::REG)
                    {
                        currentValReg = si->val->getRegNum();
                        hasCurrentReg = true;
                        // 该 store 已被寄存器替代，标记删除
                        delSet.insert(si);
                    }
                    else
                    {
                        // 由于上层已检查 allStoreValsAreRegs，这里通常不会进入该分支
                        // 为稳妥起见，不改变当前寄存器状态
                        hasCurrentReg    = false;
                        allLoadsReplaced = false;  // 出现非寄存器写入，认为不可完全删除 alloca
                    }
                }
            }
            else if (inst->opcode == Operator::LOAD)
            {
                auto* li = static_cast<LoadInst*>(inst);
                if (li->ptr->getType() == OperandType::REG && li->ptr->getRegNum() == info.regId)
                {
                    // 若已有当前值，则替换所有使用该 load 结果的源寄存器，并删除该 load
                    if (hasCurrentReg)
                    {
                        RegId loadResId      = li->res->getRegNum();
                        renameMap[loadResId] = currentValReg;
                        delSet.insert(li);
                    }
                    else
                    {
                        // 在首个寄存器 store 之前的 load 无法替换为 undef，保留该 load
                        allLoadsReplaced = false;
                    }
                }
            }
        }

        // 返回是否可删除 alloca（仅当该块内所有相关 load 均被删除时）
        return allLoadsReplaced;
    }

    void BasicMem2RegPass::applySrcRegRename(Function& function, RegMap& renameMap)
    {
        if (renameMap.empty()) return;

        // 消除链式重命名：将 k->v（若 v 也在映射中）折叠为 k->final
        flattenRegRenameMap(renameMap);

        SrcRegRename renamer;

        for (auto& [bid, block] : function.blocks)
        {
            for (auto* inst : block->insts) { apply(renamer, *inst, renameMap); }
        }
    }

    void BasicMem2RegPass::applyBatchDelete(Function& function, const std::unordered_set<Instruction*>& delSet)
    {
        if (delSet.empty()) return;

        for (auto& [bid, block] : function.blocks)
        {
            std::deque<Instruction*> newInsts;
            newInsts.resize(0);
            newInsts.clear();

            for (auto* inst : block->insts)
            {
                if (delSet.find(inst) == delSet.end()) { newInsts.push_back(inst); }
                else
                {
                    delete inst;
                }
            }

            block->insts.swap(newInsts);
        }
    }

    void BasicMem2RegPass::flattenRegRenameMap(RegMap& renameMap)
    {
        if (renameMap.empty()) return;

        auto find_final = [&](RegId r) -> RegId {
            // 路径压缩查找最终目标寄存器
            RegId cur = r;
            // 防止异常环
            std::unordered_set<RegId> visiting;
            while (true)
            {
                auto it = renameMap.find(cur);
                if (it == renameMap.end()) break;
                RegId nxt = it->second;
                if (nxt == cur) break;
                if (!visiting.insert(cur).second) break;  // 检测到环，退出
                cur = nxt;
            }
            return cur;
        };

        // 折叠每个映射的目标
        for (auto& kv : renameMap)
        {
            RegId final = find_final(kv.second);
            kv.second   = final;
        }
    }

}  // namespace ME
