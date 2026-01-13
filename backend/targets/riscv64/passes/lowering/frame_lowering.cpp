#include "backend/targets/riscv64/rv64_defs.h"
#include <backend/targets/riscv64/passes/lowering/frame_lowering.h>
#include <backend/mir/m_block.h>
#include <backend/mir/m_defs.h>
#include <backend/mir/m_function.h>
#include <debug.h>

namespace BE::RV64::Passes::Lowering
{
    namespace
    {
        static inline bool imm12(int v) { return v >= -2048 && v <= 2047; }

        static inline bool isLoadOp(Operator op)
        {
            return op == Operator::LW || op == Operator::LD || op == Operator::FLW || op == Operator::FLD;
        }

        static inline bool isStoreOp(Operator op)
        {
            return op == Operator::SW || op == Operator::SD || op == Operator::FSW || op == Operator::FSD;
        }
    }  // namespace

    void FrameLoweringPass::runOnModule(BE::Module& module)
    {
        for (auto* func : module.functions) runOnFunction(func);
    }

    void FrameLoweringPass::runOnFunction(BE::Function* func) { 
        if (!func) return;

        if (func->paramSize > 0) func->frameInfo.setParamAreaSize(func->paramSize);
        func->frameInfo.calculateOffsets();

        for (auto& [_, block] : func->blocks)
        {
            for (auto it = block->insts.begin(); it != block->insts.end(); ++it)
            {
                auto* ri = dynamic_cast<Instr*>(*it);
                if (!ri || !ri->use_ops || !ri->fiop || ri->fiop->ot != BE::Operand::Type::FRAME_INDEX) continue;

                auto* fiOp  = static_cast<BE::FrameIndexOperand*>(ri->fiop);
                int   offset = func->frameInfo.getObjectOffset(fiOp->frameIndex);
                if (offset < 0) offset = func->frameInfo.getSpillSlotOffset(fiOp->frameIndex);
                if (offset < 0) continue;

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
                    BE::DataType* offType = ri->rd.dt ? ri->rd.dt : BE::I64;
                    Register      offReg  = BE::getVReg(offType);
                    it = block->insts.insert(it, BE::createMove(new BE::RegOperand(offReg), offset));
                    ++it;

                    Operator addOp = (ri->op == Operator::ADDIW) ? Operator::ADDW : Operator::ADD;
                    auto*    newInst = createRInst(addOp, ri->rd, ri->rs1, offReg);

                    delete ri->fiop;
                    BE::MInstruction::delInst(ri);
                    *it = newInst;
                    continue;
                }

                if (isLoadOp(ri->op) || isStoreOp(ri->op))
                {
                    Register baseReg = isStoreOp(ri->op) ? ri->rs2 : ri->rs1;
                    Register offReg  = BE::getVReg(BE::I64);
                    Register addrReg = BE::getVReg(BE::I64);

                    it = block->insts.insert(it, BE::createMove(new BE::RegOperand(offReg), offset));
                    ++it;
                    it = block->insts.insert(it, createRInst(Operator::ADD, addrReg, baseReg, offReg));
                    ++it;

                    if (isStoreOp(ri->op))
                        ri->rs2 = addrReg;
                    else
                        ri->rs1 = addrReg;

                    ri->imme   = 0;
                    ri->use_ops = false;
                    delete ri->fiop;
                    ri->fiop = nullptr;
                    continue;
                }

                ri->imme   = offset;
                ri->use_ops = false;
                delete ri->fiop;
                ri->fiop = nullptr;
            }
        }
    }

}  // namespace BE::RV64::Passes::Lowering
