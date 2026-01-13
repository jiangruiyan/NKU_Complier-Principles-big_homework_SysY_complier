#include <backend/targets/riscv64/rv64_instr_adapter.h>
#include <backend/targets/riscv64/rv64_defs.h>
#include <algorithm>

namespace BE::Targeting::RV64
{
    using namespace BE::RV64;

    bool InstrAdapter::isCall(BE::MInstruction* inst) const { 
        if (!inst || inst->kind != BE::InstKind::TARGET) return false;
        return static_cast<Instr*>(inst)->op == Operator::CALL;
    }

    bool InstrAdapter::isReturn(BE::MInstruction* inst) const { 
        if (!inst || inst->kind != BE::InstKind::TARGET) return false;
        auto* ri = static_cast<Instr*>(inst);
        if (ri->op == Operator::RET) return true;
        return ri->op == Operator::JALR && ri->rd == PR::x0 && ri->rs1 == PR::ra && ri->imme == 0;
    }

    bool InstrAdapter::isUncondBranch(BE::MInstruction* inst) const { 
        if (!inst || inst->kind != BE::InstKind::TARGET) return false;
        auto* ri = static_cast<Instr*>(inst);
        if (ri->op == Operator::JAL) return true;
        return ri->op == Operator::JALR && ri->rd == PR::x0;
    }

    bool InstrAdapter::isCondBranch(BE::MInstruction* inst) const { 
        if (!inst || inst->kind != BE::InstKind::TARGET) return false;
        auto* ri = static_cast<Instr*>(inst);
        switch (ri->op)
        {
            case Operator::BEQ:
            case Operator::BNE:
            case Operator::BLT:
            case Operator::BGE:
            case Operator::BLTU:
            case Operator::BGEU:
            case Operator::BGT:
            case Operator::BLE:
            case Operator::BGTU:
            case Operator::BLEU:
                return true;
            default: return false;
        }
    }

    int InstrAdapter::extractBranchTarget(BE::MInstruction* inst) const {
        if (!inst || inst->kind != BE::InstKind::TARGET) return -1;
        auto* ri = static_cast<Instr*>(inst);
        if (!ri->use_label) return -1;
        switch (ri->op)
        {
            case Operator::BEQ:
            case Operator::BNE:
            case Operator::BLT:
            case Operator::BGE:
            case Operator::BLTU:
            case Operator::BGEU:
            case Operator::BGT:
            case Operator::BLE:
            case Operator::BGTU:
            case Operator::BLEU:
            case Operator::JAL:
                return ri->label.jmp_label;
            default:
                return -1;
        }
    }

    void InstrAdapter::enumUses(BE::MInstruction* inst, std::vector<BE::Register>& out) const
    {
        if (!inst) return;
        switch (inst->kind)
        {
            case BE::InstKind::TARGET:
            {
                auto* ri = static_cast<Instr*>(inst);
                if (ri->op == Operator::CALL)
                {
                    static const BE::Register intArgs[] = {PR::a0, PR::a1, PR::a2, PR::a3, PR::a4, PR::a5, PR::a6, PR::a7};
                    static const BE::Register floatArgs[] = {PR::fa0, PR::fa1, PR::fa2, PR::fa3, PR::fa4, PR::fa5, PR::fa6, PR::fa7};
                    int icnt = ri->call_ireg_cnt < 8 ? ri->call_ireg_cnt : 8;
                    int fcnt = ri->call_freg_cnt < 8 ? ri->call_freg_cnt : 8;
                    for (int i = 0; i < icnt; ++i) out.push_back(intArgs[i]);
                    for (int i = 0; i < fcnt; ++i) out.push_back(floatArgs[i]);
                    return;
                }

                OpType opType;
                switch (ri->op)
                {
#define X(name, type, _asm, latency) \
    case Operator::name: opType = OpType::type; break;
                    RV64_INSTS
#undef X
                    default: return;
                }

                switch (opType)
                {
                    case OpType::R:
                        out.push_back(ri->rs1);
                        out.push_back(ri->rs2);
                        break;
                    case OpType::I:
                        out.push_back(ri->rs1);
                        break;
                    case OpType::S:
                        out.push_back(ri->rs1);
                        out.push_back(ri->rs2);
                        break;
                    case OpType::B:
                        out.push_back(ri->rs1);
                        out.push_back(ri->rs2);
                        break;
                    case OpType::R2:
                        out.push_back(ri->rs1);
                        break;
                    case OpType::R4:
                        out.push_back(ri->rs1);
                        out.push_back(ri->rs2);
                        break;
                    case OpType::U:
                    case OpType::J:
                    case OpType::CALL:
                        break;
                    default:
                        break;
                }
                return;
            }
            case BE::InstKind::MOVE:
            {
                auto* mv = static_cast<BE::MoveInst*>(inst);
                if (mv->src && mv->src->ot == BE::Operand::Type::REG)
                    out.push_back(static_cast<BE::RegOperand*>(mv->src)->reg);
                return;
            }
            case BE::InstKind::PHI:
            {
                auto* phi = static_cast<BE::PhiInst*>(inst);
                for (auto& [_, src] : phi->incomingVals)
                {
                    if (src && src->ot == BE::Operand::Type::REG)
                        out.push_back(static_cast<BE::RegOperand*>(src)->reg);
                }
                return;
            }
            case BE::InstKind::SSLOT:
            {
                auto* ss = static_cast<BE::FIStoreInst*>(inst);
                out.push_back(ss->src);
                return;
            }
            default:
                return;
        }
    }

    void InstrAdapter::enumDefs(BE::MInstruction* inst, std::vector<BE::Register>& out) const
    {
        if (!inst) return;
        switch (inst->kind)
        {
            case BE::InstKind::TARGET:
            {
                auto* ri = static_cast<Instr*>(inst);
                if (ri->op == Operator::CALL) return;

                OpType opType;
                switch (ri->op)
                {
#define X(name, type, _asm, latency) \
    case Operator::name: opType = OpType::type; break;
                    RV64_INSTS
#undef X
                    default: return;
                }

                switch (opType)
                {
                    case OpType::R:
                    case OpType::I:
                    case OpType::U:
                    case OpType::J:
                    case OpType::R2:
                    case OpType::R4:
                        out.push_back(ri->rd);
                        break;
                    case OpType::S:
                    case OpType::B:
                    case OpType::CALL:
                    default:
                        break;
                }
                return;
            }
            case BE::InstKind::MOVE:
            {
                auto* mv = static_cast<BE::MoveInst*>(inst);
                if (mv->dest && mv->dest->ot == BE::Operand::Type::REG)
                    out.push_back(static_cast<BE::RegOperand*>(mv->dest)->reg);
                return;
            }
            case BE::InstKind::PHI:
            {
                auto* phi = static_cast<BE::PhiInst*>(inst);
                out.push_back(phi->resReg);
                return;
            }
            case BE::InstKind::LSLOT:
            {
                auto* ls = static_cast<BE::FILoadInst*>(inst);
                out.push_back(ls->dest);
                return;
            }
            default:
                return;
        }
    }

    static void replaceReg(BE::Register& slot, const BE::Register& from, const BE::Register& to)
    {
        if (slot == from) slot = to;
    }

    void InstrAdapter::replaceUse(BE::MInstruction* inst, const BE::Register& from, const BE::Register& to) const
    {
        auto* ri = dynamic_cast<Instr*>(inst);
        if (!ri) return;
        replaceReg(ri->rs1, from, to);
        replaceReg(ri->rs2, from, to);
    }

    void InstrAdapter::replaceDef(BE::MInstruction* inst, const BE::Register& from, const BE::Register& to) const
    {
        auto* ri = dynamic_cast<Instr*>(inst);
        if (!ri) return;
        replaceReg(ri->rd, from, to);
    }

    void InstrAdapter::enumPhysRegs(BE::MInstruction* inst, std::vector<BE::Register>& out) const
    {
        if (!inst) return;

        auto addPhys = [&out](const BE::Register& reg) {
            if (!reg.isVreg) out.push_back(reg);
        };

        switch (inst->kind)
        {
            case BE::InstKind::TARGET:
            {
                auto* ri = static_cast<Instr*>(inst);
                if (ri->op == Operator::CALL)
                {
                    static const BE::Register intArgs[] = {PR::a0, PR::a1, PR::a2, PR::a3, PR::a4, PR::a5, PR::a6, PR::a7};
                    static const BE::Register floatArgs[] = {PR::fa0, PR::fa1, PR::fa2, PR::fa3, PR::fa4, PR::fa5, PR::fa6, PR::fa7};
                    int icnt = ri->call_ireg_cnt < 8 ? ri->call_ireg_cnt : 8;
                    int fcnt = ri->call_freg_cnt < 8 ? ri->call_freg_cnt : 8;
                    for (int i = 0; i < icnt; ++i) out.push_back(intArgs[i]);
                    for (int i = 0; i < fcnt; ++i) out.push_back(floatArgs[i]);
                    return;
                }

                OpType opType;
                switch (ri->op)
                {
#define X(name, type, _asm, latency)     case Operator::name: opType = OpType::type; break;
                    RV64_INSTS
#undef X
                    default: return;
                }

                switch (opType)
                {
                    case OpType::R:
                        addPhys(ri->rd);
                        addPhys(ri->rs1);
                        addPhys(ri->rs2);
                        break;
                    case OpType::I:
                        addPhys(ri->rd);
                        addPhys(ri->rs1);
                        break;
                    case OpType::S:
                    case OpType::B:
                        addPhys(ri->rs1);
                        addPhys(ri->rs2);
                        break;
                    case OpType::R2:
                        addPhys(ri->rd);
                        addPhys(ri->rs1);
                        break;
                    case OpType::R4:
                        addPhys(ri->rd);
                        addPhys(ri->rs1);
                        addPhys(ri->rs2);
                        break;
                    case OpType::U:
                    case OpType::J:
                    case OpType::CALL:
                    default:
                        break;
                }
                return;
            }
            case BE::InstKind::MOVE:
            {
                auto* mv = static_cast<BE::MoveInst*>(inst);
                if (mv->src && mv->src->ot == BE::Operand::Type::REG)
                    addPhys(static_cast<BE::RegOperand*>(mv->src)->reg);
                if (mv->dest && mv->dest->ot == BE::Operand::Type::REG)
                    addPhys(static_cast<BE::RegOperand*>(mv->dest)->reg);
                return;
            }
            case BE::InstKind::PHI:
            {
                auto* phi = static_cast<BE::PhiInst*>(inst);
                addPhys(phi->resReg);
                for (auto& [_, src] : phi->incomingVals)
                {
                    if (src && src->ot == BE::Operand::Type::REG)
                        addPhys(static_cast<BE::RegOperand*>(src)->reg);
                }
                return;
            }
            case BE::InstKind::SSLOT:
            {
                auto* ss = static_cast<BE::FIStoreInst*>(inst);
                addPhys(ss->src);
                return;
            }
            case BE::InstKind::LSLOT:
            {
                auto* ls = static_cast<BE::FILoadInst*>(inst);
                addPhys(ls->dest);
                return;
            }
            default:
                return;
        }
    }

    void InstrAdapter::insertReloadBefore(
        BE::Block* block, std::deque<BE::MInstruction*>::iterator it, const BE::Register& physReg, int frameIndex) const
    {
        block->insts.insert(it, new BE::FILoadInst(physReg, frameIndex));
    }

    void InstrAdapter::insertSpillAfter(
        BE::Block* block, std::deque<BE::MInstruction*>::iterator it, const BE::Register& physReg, int frameIndex) const
    {
        block->insts.insert(std::next(it), new BE::FIStoreInst(physReg, frameIndex));
    }
}  // namespace BE::Targeting::RV64

