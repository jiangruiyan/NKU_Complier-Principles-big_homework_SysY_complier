#include <middleend/pass/sccp.h>
#include <middleend/module/ir_module.h>
#include <middleend/module/ir_function.h>
#include <middleend/module/ir_block.h>
#include <middleend/module/ir_instruction.h>
#include <middleend/module/ir_operand.h>

#include <vector>
#include <queue>
#include <unordered_map>

namespace ME
{
    using LV = SCCPPass::LatticeVal;
    using ValState = SCCPPass::ValState;

    void SCCPPass::runOnModule(Module& module)
    {
        for (auto* func : module.functions)
            runOnFunction(*func);
    }

    // 简化的 SCCP：基于迭代的格传播 + 把最终常量替换成立即数
    void SCCPPass::runOnFunction(Function& function)
    {
        std::unordered_map<size_t, LV> lattice; // regNum -> lattice
        bool changed = true;

        // 初始化：所有定义的寄存器为 Unknown
        for (auto& [bid, block] : function.blocks)
        {
            for (auto* inst : block->insts)
            {
                Operand* def = nullptr;
                switch (inst->opcode)
                {
                    case Operator::LOAD: def = static_cast<LoadInst*>(inst)->res; break;
                    case Operator::ADD: case Operator::SUB: case Operator::MUL: case Operator::DIV:
                    case Operator::FADD: case Operator::FSUB: case Operator::FMUL: case Operator::FDIV:
                    case Operator::MOD: case Operator::BITXOR: case Operator::BITAND:
                    case Operator::SHL: case Operator::ASHR: case Operator::LSHR:
                        def = static_cast<ArithmeticInst*>(inst)->res; break;
                    case Operator::ICMP: case Operator::FCMP:
                        def = static_cast<IcmpInst*>(inst)->res; break;
                    case Operator::PHI:
                        def = static_cast<PhiInst*>(inst)->res; break;
                    case Operator::CALL:
                        def = static_cast<CallInst*>(inst)->res; break;
                    case Operator::GETELEMENTPTR:
                        def = static_cast<GEPInst*>(inst)->res; break;
                    case Operator::SITOFP:
                        def = static_cast<SI2FPInst*>(inst)->dest; break;
                    case Operator::FPTOSI:
                        def = static_cast<FP2SIInst*>(inst)->dest; break;
                    case Operator::ZEXT:
                        def = static_cast<ZextInst*>(inst)->dest; break;
                    default:
                        break;
                }

                if (def && def->getType() == OperandType::REG)
                {
                    lattice[def->getRegNum()] = LV();
                }
            }
        }

        // 迭代传播直到不再变化
        while (changed)
        {
            changed = false;

            for (auto& [bid, block] : function.blocks)
            {
                for (auto* inst : block->insts)
                {
                    LV out;
                    if (!evaluateInstructionConst(inst, out))
                    {
                        // 无法得到常量，若定义寄存器则设置为 Overdefined
                        Operand* def = nullptr;
                        switch (inst->opcode)
                        {
                            case Operator::LOAD:
                                def = static_cast<LoadInst*>(inst)->res; break;
                            case Operator::ADD: case Operator::SUB: case Operator::MUL: case Operator::DIV:
                            case Operator::FADD: case Operator::FSUB: case Operator::FMUL: case Operator::FDIV:
                            case Operator::MOD: case Operator::BITXOR: case Operator::BITAND:
                            case Operator::SHL: case Operator::ASHR: case Operator::LSHR:
                                def = static_cast<ArithmeticInst*>(inst)->res; break;
                            case Operator::ICMP: case Operator::FCMP:
                                def = static_cast<IcmpInst*>(inst)->res; break;
                            case Operator::PHI:
                                def = static_cast<PhiInst*>(inst)->res; break;
                            case Operator::CALL:
                                def = static_cast<CallInst*>(inst)->res; break;
                            case Operator::GETELEMENTPTR:
                                def = static_cast<GEPInst*>(inst)->res; break;
                            case Operator::SITOFP:
                                def = static_cast<SI2FPInst*>(inst)->dest; break;
                            case Operator::FPTOSI:
                                def = static_cast<FP2SIInst*>(inst)->dest; break;
                            case Operator::ZEXT:
                                def = static_cast<ZextInst*>(inst)->dest; break;
                            default:
                                break;
                        }

                        if (def && def->getType() == OperandType::REG)
                        {
                            size_t r = def->getRegNum();
                            auto& cur = lattice[r];
                            if (cur.state != ValState::Overdefined)
                            {
                                cur.state = ValState::Overdefined;
                                changed = true;
                            }
                        }
                    }
                    else
                    {
                        // 得到常量 -> 写入 lattice
                        Operand* def = nullptr;
                        switch (inst->opcode)
                        {
                            case Operator::LOAD: def = static_cast<LoadInst*>(inst)->res; break;
                            case Operator::ADD: case Operator::SUB: case Operator::MUL: case Operator::DIV:
                            case Operator::FADD: case Operator::FSUB: case Operator::FMUL: case Operator::FDIV:
                            case Operator::MOD: case Operator::BITXOR: case Operator::BITAND:
                            case Operator::SHL: case Operator::ASHR: case Operator::LSHR:
                                def = static_cast<ArithmeticInst*>(inst)->res; break;
                            case Operator::ICMP: case Operator::FCMP:
                                def = static_cast<IcmpInst*>(inst)->res; break;
                            case Operator::PHI:
                                def = static_cast<PhiInst*>(inst)->res; break;
                            case Operator::CALL:
                                def = static_cast<CallInst*>(inst)->res; break;
                            case Operator::GETELEMENTPTR:
                                def = static_cast<GEPInst*>(inst)->res; break;
                            case Operator::SITOFP:
                                def = static_cast<SI2FPInst*>(inst)->dest; break;
                            case Operator::FPTOSI:
                                def = static_cast<FP2SIInst*>(inst)->dest; break;
                            case Operator::ZEXT:
                                def = static_cast<ZextInst*>(inst)->dest; break;
                            default:
                                break;
                        }

                        if (def && def->getType() == OperandType::REG)
                        {
                            size_t r = def->getRegNum();
                            auto& cur = lattice[r];
                            if (cur.state != out.state || cur.i32 != out.i32 || cur.f32 != out.f32)
                            {
                                cur = out;
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        // 替换寄存器操作数为立即数（仅 ConstI32/ConstF32）
        for (auto& [bid, block] : function.blocks)
        {
            for (auto* inst : block->insts)
            {
                // 遍历并替换常见的操作数字段
                auto tryReplace = [&](Operand*& op) {
                    if (!op) return;
                    if (op->getType() != OperandType::REG) return;
                    size_t r = op->getRegNum();
                    auto it = lattice.find(r);
                    if (it == lattice.end()) return;
                    const LV& v = it->second;
                    if (v.state == ValState::ConstI32)
                    {
                        // use OperandFactory helpers (Imme operands are managed by factory)
                        op = getImmeI32Operand(v.i32);
                    }
                    else if (v.state == ValState::ConstF32)
                    {
                        op = getImmeF32Operand(v.f32);
                    }
                };

                switch (inst->opcode)
                {
                    case Operator::ADD: case Operator::SUB: case Operator::MUL: case Operator::DIV:
                    case Operator::MOD: case Operator::BITXOR: case Operator::BITAND:
                    case Operator::SHL: case Operator::ASHR: case Operator::LSHR:
                    case Operator::FADD: case Operator::FSUB: case Operator::FMUL: case Operator::FDIV: {
                        auto* ai = static_cast<ArithmeticInst*>(inst);
                        tryReplace(ai->lhs);
                        tryReplace(ai->rhs);
                        break;
                    }
                    case Operator::ICMP: case Operator::FCMP: {
                        auto* ci = static_cast<IcmpInst*>(inst);
                        tryReplace(ci->lhs);
                        tryReplace(ci->rhs);
                        break;
                    }
                    case Operator::STORE: {
                        auto* si = static_cast<StoreInst*>(inst);
                        tryReplace(si->val);
                        tryReplace(si->ptr);
                        break;
                    }
                    case Operator::LOAD: {
                        auto* li = static_cast<LoadInst*>(inst);
                        tryReplace(li->ptr);
                        break;
                    }
                    case Operator::BR_COND: {
                        auto* br = static_cast<BrCondInst*>(inst);
                        tryReplace(br->cond);
                        break;
                    }
                    case Operator::PHI: {
                        auto* phi = static_cast<PhiInst*>(inst);
                        for (auto& p : phi->incomingVals)
                        {
                            tryReplace(p.second);
                        }
                        break;
                    }
                    case Operator::CALL: {
                        auto* call = static_cast<CallInst*>(inst);
                        for (auto& a : call->args) tryReplace(a.second);
                        break;
                    }
                    case Operator::GETELEMENTPTR: {
                        auto* gep = static_cast<GEPInst*>(inst);
                        tryReplace(gep->basePtr);
                        for (auto*& idx : gep->idxs) tryReplace(idx);
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    // 尝试对单条指令进行常量求值，能求出常量则返回 true 并填充 out
    bool SCCPPass::evaluateInstructionConst(Instruction* inst, LV& out)
    {
        // 只处理常见的算术/比较指令与 PHI
        switch (inst->opcode)
        {
            case Operator::ADD: case Operator::SUB: case Operator::MUL: case Operator::DIV:
            case Operator::MOD: case Operator::BITXOR: case Operator::BITAND:
            case Operator::SHL: case Operator::ASHR: case Operator::LSHR: {
                auto* ai = static_cast<ArithmeticInst*>(inst);
                if (!ai->lhs || !ai->rhs) return false;
                if (ai->lhs->getType() == OperandType::IMMEI32 && ai->rhs->getType() == OperandType::IMMEI32)
                {
                    int a = static_cast<ImmeI32Operand*>(ai->lhs)->value;
                    int b = static_cast<ImmeI32Operand*>(ai->rhs)->value;
                    long long res = 0;
                    switch (inst->opcode)
                    {
                        case Operator::ADD: res = (long long)a + b; break;
                        case Operator::SUB: res = (long long)a - b; break;
                        case Operator::MUL: res = (long long)a * b; break;
                        case Operator::DIV: if (b==0) return false; res = a / b; break;
                        case Operator::MOD: if (b==0) return false; res = a % b; break;
                        case Operator::BITXOR: res = a ^ b; break;
                        case Operator::BITAND: res = a & b; break;
                        case Operator::SHL: res = a << b; break;
                        case Operator::ASHR: res = a >> b; break;
                        case Operator::LSHR: res = static_cast<unsigned int>(a) >> b; break;
                        default: return false;
                    }
                    out.state = ValState::ConstI32;
                    out.i32 = static_cast<int>(res);
                    return true;
                }
                return false;
            }
            case Operator::FADD: case Operator::FSUB: case Operator::FMUL: case Operator::FDIV: {
                auto* ai = static_cast<ArithmeticInst*>(inst);
                if (!ai->lhs || !ai->rhs) return false;
                if (ai->lhs->getType() == OperandType::IMMEF32 && ai->rhs->getType() == OperandType::IMMEF32)
                {
                    float a = static_cast<ImmeF32Operand*>(ai->lhs)->value;
                    float b = static_cast<ImmeF32Operand*>(ai->rhs)->value;
                    float res = 0.0f;
                    switch (inst->opcode)
                    {
                        case Operator::FADD: res = a + b; break;
                        case Operator::FSUB: res = a - b; break;
                        case Operator::FMUL: res = a * b; break;
                        case Operator::FDIV: if (b==0.0f) return false; res = a / b; break;
                        default: return false;
                    }
                    out.state = ValState::ConstF32;
                    out.f32 = res;
                    return true;
                }
                return false;
            }
            case Operator::ICMP: {
                auto* ci = static_cast<IcmpInst*>(inst);
                if (!ci->lhs || !ci->rhs) return false;
                if (ci->lhs->getType() == OperandType::IMMEI32 && ci->rhs->getType() == OperandType::IMMEI32)
                {
                    int a = static_cast<ImmeI32Operand*>(ci->lhs)->value;
                    int b = static_cast<ImmeI32Operand*>(ci->rhs)->value;
                    bool res = false;
                    switch (ci->cond)
                    {
                        case ICmpOp::EQ: res = (a==b); break;
                        case ICmpOp::NE: res = (a!=b); break;
                        case ICmpOp::SGT: res = (a>b); break;
                        case ICmpOp::SGE: res = (a>=b); break;
                        case ICmpOp::SLT: res = (a<b); break;
                        case ICmpOp::SLE: res = (a<=b); break;
                        default: return false;
                    }
                    out.state = ValState::ConstI32;
                    out.i32 = res ? 1 : 0;
                    return true;
                }
                return false;
            }
            case Operator::PHI: {
                auto* phi = static_cast<PhiInst*>(inst);
                bool first = true;
                LV tmp;
                for (auto& p : phi->incomingVals)
                {
                    Operand* v = p.second;
                    if (!v) return false;
                    if (v->getType() == OperandType::IMMEI32)
                    {
                        LV cur; cur.state = ValState::ConstI32; cur.i32 = static_cast<ImmeI32Operand*>(v)->value;
                        if (first) { tmp = cur; first = false; }
                        else if (tmp.state != cur.state || tmp.i32 != cur.i32) return false;
                    }
                    else if (v->getType() == OperandType::IMMEF32)
                    {
                        LV cur; cur.state = ValState::ConstF32; cur.f32 = static_cast<ImmeF32Operand*>(v)->value;
                        if (first) { tmp = cur; first = false; }
                        else if (tmp.state != cur.state || tmp.f32 != cur.f32) return false;
                    }
                    else return false;
                }

                if (!first)
                {
                    out = tmp;
                    return true;
                }
                return false;
            }
            default:
                break;
        }

        return false;
    }

} // namespace ME
