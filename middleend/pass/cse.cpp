#include <middleend/pass/cse.h>
#include <middleend/module/ir_module.h>
#include <middleend/module/ir_function.h>
#include <middleend/module/ir_block.h>
#include <middleend/module/ir_instruction.h>
#include <middleend/module/ir_operand.h>
#include <middleend/visitor/utils/rename_visitor.h>
#include <interfaces/middleend/ir_defs.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ME
{
    void CSEPass::runOnModule(Module& module)
    {
        for (auto* func : module.functions)
        {
            runOnFunction(*func);
        }
    }

    void CSEPass::runOnFunction(Function& function)
    {
        // 记录每个基本块内的表达式
        std::unordered_map<size_t, Instruction*> exprMap;
        RegMap renameMap;
        std::unordered_set<Instruction*> toDelete;

        for (auto& [bid, block] : function.blocks)
        {
            // 每个基本块独立处理（局部CSE）
            exprMap.clear();

            for (auto* inst : block->insts)
            {
                // 只处理纯计算指令（无副作用）
                bool isPureComputation = false;
                switch (inst->opcode)
                {
                    case Operator::ADD:
                    case Operator::SUB:
                    case Operator::MUL:
                    case Operator::DIV:
                    case Operator::FADD:
                    case Operator::FSUB:
                    case Operator::FMUL:
                    case Operator::FDIV:
                    case Operator::MOD:
                    case Operator::BITXOR:
                    case Operator::BITAND:
                    case Operator::SHL:
                    case Operator::ASHR:
                    case Operator::LSHR:
                    case Operator::ICMP:
                    case Operator::FCMP:
                        isPureComputation = true;
                        break;
                    default:
                        break;
                }

                if (!isPureComputation)
                    continue;

                size_t hash = hashInstruction(inst);
                auto it = exprMap.find(hash);

                if (it != exprMap.end())
                {
                    // 找到了相同的表达式，检查是否真的等价
                    Instruction* prevInst = it->second;
                    if (areInstructionsEquivalent(inst, prevInst))
                    {
                        // 记录重命名：当前指令的结果 -> 之前指令的结果
                        Operand* currRes = nullptr;
                        Operand* prevRes = nullptr;

                        if (inst->opcode >= Operator::ADD && inst->opcode <= Operator::LSHR)
                        {
                            currRes = static_cast<ArithmeticInst*>(inst)->res;
                            prevRes = static_cast<ArithmeticInst*>(prevInst)->res;
                        }
                        else if (inst->opcode == Operator::ICMP || inst->opcode == Operator::FCMP)
                        {
                            currRes = static_cast<IcmpInst*>(inst)->res;
                            prevRes = static_cast<IcmpInst*>(prevInst)->res;
                        }

                        if (currRes && prevRes && 
                            currRes->getType() == OperandType::REG && 
                            prevRes->getType() == OperandType::REG)
                        {
                            renameMap[currRes->getRegNum()] = prevRes->getRegNum();
                            toDelete.insert(inst);
                        }
                    }
                }
                else
                {
                    // 记录这个表达式
                    exprMap[hash] = inst;
                }
            }
        }

        // 应用重命名
        if (!renameMap.empty())
        {
            RegRename renamer;
            for (auto& [bid, block] : function.blocks)
            {
                for (auto* inst : block->insts)
                {
                    apply(renamer, *inst, renameMap);
                }
            }
        }

        // 删除冗余指令
        if (!toDelete.empty())
        {
            for (auto& [bid, block] : function.blocks)
            {
                std::deque<Instruction*> newInsts;
                for (auto* inst : block->insts)
                {
                    if (toDelete.find(inst) == toDelete.end())
                    {
                        newInsts.push_back(inst);
                    }
                    else
                    {
                        delete inst;
                    }
                }
                block->insts.swap(newInsts);
            }
        }
    }

    size_t CSEPass::hashInstruction(Instruction* inst) const
    {
        size_t h = std::hash<int>{}(static_cast<int>(inst->opcode));

        switch (inst->opcode)
        {
            case Operator::ADD:
            case Operator::SUB:
            case Operator::MUL:
            case Operator::DIV:
            case Operator::FADD:
            case Operator::FSUB:
            case Operator::FMUL:
            case Operator::FDIV:
            case Operator::MOD:
            case Operator::BITXOR:
            case Operator::BITAND:
            case Operator::SHL:
            case Operator::ASHR:
            case Operator::LSHR: {
                auto* ai = static_cast<ArithmeticInst*>(inst);
                if (ai->lhs && ai->lhs->getType() == OperandType::REG)
                    h ^= std::hash<size_t>{}(ai->lhs->getRegNum()) << 1;
                else if (ai->lhs && ai->lhs->getType() == OperandType::IMMEI32)
                    h ^= std::hash<int>{}(static_cast<ImmeI32Operand*>(ai->lhs)->value) << 1;
                
                if (ai->rhs && ai->rhs->getType() == OperandType::REG)
                    h ^= std::hash<size_t>{}(ai->rhs->getRegNum()) << 2;
                else if (ai->rhs && ai->rhs->getType() == OperandType::IMMEI32)
                    h ^= std::hash<int>{}(static_cast<ImmeI32Operand*>(ai->rhs)->value) << 2;
                break;
            }
            case Operator::ICMP:
            case Operator::FCMP: {
                auto* ci = static_cast<IcmpInst*>(inst);
                h ^= std::hash<int>{}(static_cast<int>(ci->cond));
                if (ci->lhs && ci->lhs->getType() == OperandType::REG)
                    h ^= std::hash<size_t>{}(ci->lhs->getRegNum()) << 1;
                if (ci->rhs && ci->rhs->getType() == OperandType::REG)
                    h ^= std::hash<size_t>{}(ci->rhs->getRegNum()) << 2;
                break;
            }
            default:
                break;
        }

        return h;
    }

    bool CSEPass::areInstructionsEquivalent(Instruction* i1, Instruction* i2) const
    {
        if (i1->opcode != i2->opcode)
            return false;

        switch (i1->opcode)
        {
            case Operator::ADD:
            case Operator::SUB:
            case Operator::MUL:
            case Operator::DIV:
            case Operator::FADD:
            case Operator::FSUB:
            case Operator::FMUL:
            case Operator::FDIV:
            case Operator::MOD:
            case Operator::BITXOR:
            case Operator::BITAND:
            case Operator::SHL:
            case Operator::ASHR:
            case Operator::LSHR: {
                auto* a1 = static_cast<ArithmeticInst*>(i1);
                auto* a2 = static_cast<ArithmeticInst*>(i2);
                return areOperandsEquivalent(a1->lhs, a2->lhs) && 
                       areOperandsEquivalent(a1->rhs, a2->rhs);
            }
            case Operator::ICMP:
            case Operator::FCMP: {
                auto* c1 = static_cast<IcmpInst*>(i1);
                auto* c2 = static_cast<IcmpInst*>(i2);
                return c1->cond == c2->cond &&
                       areOperandsEquivalent(c1->lhs, c2->lhs) && 
                       areOperandsEquivalent(c1->rhs, c2->rhs);
            }
            default:
                return false;
        }
    }

    bool CSEPass::areOperandsEquivalent(Operand* o1, Operand* o2) const
    {
        if (!o1 || !o2)
            return o1 == o2;

        if (o1->getType() != o2->getType())
            return false;

        switch (o1->getType())
        {
            case OperandType::REG:
                return o1->getRegNum() == o2->getRegNum();
            case OperandType::IMMEI32:
                return static_cast<ImmeI32Operand*>(o1)->value == 
                       static_cast<ImmeI32Operand*>(o2)->value;
            case OperandType::IMMEF32:
                return static_cast<ImmeF32Operand*>(o1)->value == 
                       static_cast<ImmeF32Operand*>(o2)->value;
            case OperandType::GLOBAL:
                return static_cast<GlobalOperand*>(o1)->name == 
                       static_cast<GlobalOperand*>(o2)->name;
            default:
                return false;
        }
    }

}  // namespace ME
