#include <backend/targets/riscv64/passes/lowering/stack_lowering.h>
#include <backend/mir/m_function.h>
#include <backend/mir/m_instruction.h>
#include <backend/mir/m_defs.h>
#include <backend/targets/riscv64/rv64_defs.h>
#include <algorithm>
#include <backend/targets/riscv64/rv64_reg_info.h>

namespace BE::RV64::Passes::Lowering
{
    namespace
    {
        static inline bool imm12(int v) { return v >= -2048 && v <= 2047; }

        static Operator selectLoadOp(BE::DataType* dt)
        {
            if (dt == BE::F32) return Operator::FLW;
            if (dt == BE::F64) return Operator::FLD;
            if (dt == BE::I64 || dt == BE::PTR) return Operator::LD;
            return Operator::LW;
        }

        static Operator selectStoreOp(BE::DataType* dt)
        {
            if (dt == BE::F32) return Operator::FSW;
            if (dt == BE::F64) return Operator::FSD;
            if (dt == BE::I64 || dt == BE::PTR) return Operator::SD;
            return Operator::SW;
        }

        static bool isReturnInst(const Instr* inst)
        {
            if (!inst) return false;
            if (inst->op == Operator::RET) return true;
            return inst->op == Operator::JALR && inst->rd == PR::x0 && inst->rs1 == PR::ra && inst->imme == 0;
        }

        static std::deque<BE::MInstruction*>::iterator insertSpAdjust(
            BE::Block* block, std::deque<BE::MInstruction*>::iterator it, int delta)
        {
            if (delta == 0) return it;
            if (imm12(delta))
            {
                it = block->insts.insert(it, createIInst(Operator::ADDI, PR::sp, PR::sp, delta));
                ++it;
                return it;
            }

            it = block->insts.insert(it, createUInst(Operator::LI, PR::t0, delta));
            ++it;
            it = block->insts.insert(it, createRInst(Operator::ADD, PR::sp, PR::sp, PR::t0));
            ++it;
            return it;
        }

        static std::deque<BE::MInstruction*>::iterator replaceWithLargeOffsetLoad(
            BE::Block* block, std::deque<BE::MInstruction*>::iterator it, Operator op,
            const BE::Register& dest, const BE::Register& base, int offset)
        {
            if (imm12(offset))
            {
                BE::MInstruction::delInst(*it);
                *it = createIInst(op, dest, base, offset);
                return it;
            }

            it = block->insts.insert(it, createUInst(Operator::LI, PR::t0, offset));
            ++it;
            it = block->insts.insert(it, createRInst(Operator::ADD, PR::t0, base, PR::t0));
            ++it;
            BE::MInstruction::delInst(*it);
            *it = createIInst(op, dest, PR::t0, 0);
            return it;
        }

        static std::deque<BE::MInstruction*>::iterator replaceWithLargeOffsetStore(
            BE::Block* block, std::deque<BE::MInstruction*>::iterator it, Operator op,
            const BE::Register& src, const BE::Register& base, int offset)
        {
            if (imm12(offset))
            {
                BE::MInstruction::delInst(*it);
                *it = createSInst(op, src, base, offset);
                return it;
            }

            it = block->insts.insert(it, createUInst(Operator::LI, PR::t0, offset));
            ++it;
            it = block->insts.insert(it, createRInst(Operator::ADD, PR::t0, base, PR::t0));
            ++it;
            BE::MInstruction::delInst(*it);
            *it = createSInst(op, src, PR::t0, 0);
            return it;
        }
    }  // namespace

    void StackLoweringPass::runOnModule(BE::Module& module)
    {
        for (auto* func : module.functions) lowerFunction(func);
    }

    void StackLoweringPass::lowerFunction(BE::Function* func) { 
        if (!func) return;

        int frameSize = func->frameInfo.calculateOffsets();

        BE::Targeting::RV64::RegInfo regInfo;
        std::vector<bool>            usedRegs(64, false);
        bool                         hasCall = false;

        auto markReg = [&usedRegs](const BE::Register& reg) {
            if (!reg.isVreg && reg.rId < usedRegs.size()) usedRegs[reg.rId] = true;
        };

        for (auto& [_, block] : func->blocks)
        {
            for (auto* inst : block->insts)
            {
                if (auto* ri = dynamic_cast<Instr*>(inst))
                {
                    markReg(ri->rd);
                    markReg(ri->rs1);
                    markReg(ri->rs2);
                    if (ri->op == Operator::CALL) hasCall = true;
                    continue;
                }
                if (auto* mv = dynamic_cast<BE::MoveInst*>(inst))
                {
                    if (mv->src && mv->src->ot == BE::Operand::Type::REG)
                        markReg(static_cast<BE::RegOperand*>(mv->src)->reg);
                    if (mv->dest && mv->dest->ot == BE::Operand::Type::REG)
                        markReg(static_cast<BE::RegOperand*>(mv->dest)->reg);
                    continue;
                }
                if (auto* phi = dynamic_cast<BE::PhiInst*>(inst))
                {
                    markReg(phi->resReg);
                    for (auto& [_, src] : phi->incomingVals)
                    {
                        if (src && src->ot == BE::Operand::Type::REG)
                            markReg(static_cast<BE::RegOperand*>(src)->reg);
                    }
                    continue;
                }
                if (auto* ls = dynamic_cast<BE::FILoadInst*>(inst))
                {
                    markReg(ls->dest);
                    continue;
                }
                if (auto* ss = dynamic_cast<BE::FIStoreInst*>(inst))
                {
                    markReg(ss->src);
                    continue;
                }
            }
        }

        std::vector<BE::Register> savedRegs;
        for (int r : regInfo.calleeSavedIntRegs())
            if (r >= 0 && r < static_cast<int>(usedRegs.size()) && usedRegs[r])
                savedRegs.push_back(BE::RV64::PR::getPR(static_cast<uint32_t>(r)));
        for (int r : regInfo.calleeSavedFloatRegs())
            if (r >= 0 && r < static_cast<int>(usedRegs.size()) && usedRegs[r])
                savedRegs.push_back(BE::RV64::PR::getPR(static_cast<uint32_t>(r)));
        if (hasCall) savedRegs.push_back(PR::ra);

        int savedRegSize = static_cast<int>(savedRegs.size()) * 8;
        int stackSize    = frameSize + savedRegSize;
        func->stackSize  = stackSize;

        for (auto& [_, block] : func->blocks)
        {
            for (auto it = block->insts.begin(); it != block->insts.end(); ++it)
            {
                if (auto* ls = dynamic_cast<BE::FILoadInst*>(*it))
                {
                    int offset = func->frameInfo.getSpillSlotOffset(ls->frameIndex);
                    it = replaceWithLargeOffsetLoad(block, it, selectLoadOp(ls->dest.dt), ls->dest, PR::sp, offset);
                    continue;
                }
                if (auto* ss = dynamic_cast<BE::FIStoreInst*>(*it))
                {
                    int offset = func->frameInfo.getSpillSlotOffset(ss->frameIndex);
                    it = replaceWithLargeOffsetStore(block, it, selectStoreOp(ss->src.dt), ss->src, PR::sp, offset);
                    continue;
                }
                auto* ri = dynamic_cast<Instr*>(*it);
                if (!ri || !ri->use_ops || !ri->fiop || ri->fiop->ot != BE::Operand::Type::FRAME_INDEX) continue;
                auto* fiOp = static_cast<BE::FrameIndexOperand*>(ri->fiop);
                int   offset = -1;
                
                // Negative frame index means incoming arg from caller's stack
                if (fiOp->frameIndex < 0)
                {
                    offset = func->frameInfo.getIncomingArgOffset(fiOp->frameIndex);
                    if (offset >= 0)
                    {
                        // Incoming args are at positive offset relative to sp after prologue
                        // They are at sp + stackSize + offset
                        offset += stackSize;
                    }
                }
                else
                {
                    offset = func->frameInfo.getObjectOffset(fiOp->frameIndex);
                    if (offset < 0) offset = func->frameInfo.getSpillSlotOffset(fiOp->frameIndex);
                }
                
                if (offset < 0) continue;  // Skip if offset not resolved
                
                if (imm12(offset))
                {
                    ri->imme   = offset;
                    ri->use_ops = false;
                    delete ri->fiop;
                    ri->fiop = nullptr;
                    continue;
                }

                if (ri->op == Operator::ADDI || ri->op == Operator::ADDIW)
                {
                    it = block->insts.insert(it, createUInst(Operator::LI, PR::t0, offset));
                    ++it;

                    Operator addOp = (ri->op == Operator::ADDIW) ? Operator::ADDW : Operator::ADD;
                    auto*    newInst = createRInst(addOp, ri->rd, ri->rs1, PR::t0);

                    delete ri->fiop;
                    BE::MInstruction::delInst(ri);
                    *it = newInst;
                    continue;
                }

                if (ri->op == Operator::LW || ri->op == Operator::LD || ri->op == Operator::FLW || ri->op == Operator::FLD)
                {
                    replaceWithLargeOffsetLoad(block, it, ri->op, ri->rd, ri->rs1, offset);
                    continue;
                }
                if (ri->op == Operator::SW || ri->op == Operator::SD || ri->op == Operator::FSW || ri->op == Operator::FSD)
                {
                    replaceWithLargeOffsetStore(block, it, ri->op, ri->rs1, ri->rs2, offset);
                    continue;
                }

                ri->imme   = offset;
                ri->use_ops = false;
                delete ri->fiop;
                ri->fiop = nullptr;
            }
        }

        if (func->blocks.empty()) return;

        auto* entry = func->blocks.begin()->second;
        if (entry && (!savedRegs.empty() || stackSize > 0))
        {
            auto it = entry->insts.begin();
            if (stackSize > 0) it = insertSpAdjust(entry, it, -stackSize);

            // Save callee-saved regs inside the allocated frame.
            for (size_t i = 0; i < savedRegs.size(); ++i)
            {
                const auto& reg = savedRegs[i];
                int          off = frameSize + static_cast<int>(i * 8);
                Operator     sop = (reg.dt && reg.dt->dt == BE::DataType::Type::FLOAT) ? Operator::FSD : Operator::SD;
                if (imm12(off))
                {
                    it = entry->insts.insert(it, createSInst(sop, reg, PR::sp, off));
                    ++it;
                }
                else
                {
                    it = entry->insts.insert(it, createUInst(Operator::LI, PR::t0, off));
                    ++it;
                    it = entry->insts.insert(it, createRInst(Operator::ADD, PR::t0, PR::sp, PR::t0));
                    ++it;
                    it = entry->insts.insert(it, createSInst(sop, reg, PR::t0, 0));
                    ++it;
                }
            }
        }

        if (!savedRegs.empty() || stackSize > 0)
        {
            for (auto& [_, block] : func->blocks)
            {
                for (auto it = block->insts.begin(); it != block->insts.end(); ++it)
                {
                    auto* ri = dynamic_cast<Instr*>(*it);
                    if (!isReturnInst(ri)) continue;

                    for (size_t i = 0; i < savedRegs.size(); ++i)
                    {
                        const auto& reg = savedRegs[i];
                        int          off = frameSize + static_cast<int>(i * 8);
                        Operator     lop = (reg.dt && reg.dt->dt == BE::DataType::Type::FLOAT) ? Operator::FLD : Operator::LD;
                        if (imm12(off))
                        {
                            it = block->insts.insert(it, createIInst(lop, reg, PR::sp, off));
                            ++it;
                        }
                        else
                        {
                            it = block->insts.insert(it, createUInst(Operator::LI, PR::t0, off));
                            ++it;
                            it = block->insts.insert(it, createRInst(Operator::ADD, PR::t0, PR::sp, PR::t0));
                            ++it;
                            it = block->insts.insert(it, createIInst(lop, reg, PR::t0, 0));
                            ++it;
                        }
                    }

                    if (stackSize > 0)
                    {
                        it = insertSpAdjust(block, it, stackSize);
                    }
                }
            }
        }
    }
}  // namespace BE::RV64::Passes::Lowering
