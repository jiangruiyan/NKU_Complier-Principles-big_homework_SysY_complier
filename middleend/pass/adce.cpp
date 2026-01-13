#include <middleend/pass/adce.h>
#include <middleend/module/ir_module.h>
#include <middleend/module/ir_function.h>
#include <middleend/module/ir_block.h>
#include <middleend/module/ir_instruction.h>
#include <middleend/module/ir_operand.h>

#include <queue>
#include <unordered_set>

namespace ME
{
    void ADCEPass::runOnModule(Module& module)
    {
        for (auto* func : module.functions)
        {
            runOnFunction(*func);
        }
    }

    void ADCEPass::runOnFunction(Function& function)
    {
        std::unordered_set<Instruction*> liveSet;
        std::queue<Instruction*> worklist;

        // 第一步：标记所有关键指令为活跃
        for (auto& [bid, block] : function.blocks)
        {
            for (auto* inst : block->insts)
            {
                if (isCritical(inst))
                {
                    liveSet.insert(inst);
                    worklist.push(inst);
                }
            }
        }

        // 第二步：向后传播，标记所有活跃指令依赖的指令
        while (!worklist.empty())
        {
            Instruction* inst = worklist.front();
            worklist.pop();

            // 根据指令类型添加依赖
            switch (inst->opcode)
            {
                case Operator::LOAD: {
                    auto* li = static_cast<LoadInst*>(inst);
                    addOperandDeps(li->ptr, worklist, liveSet, function);
                    break;
                }
                case Operator::STORE: {
                    auto* si = static_cast<StoreInst*>(inst);
                    addOperandDeps(si->ptr, worklist, liveSet, function);
                    addOperandDeps(si->val, worklist, liveSet, function);
                    break;
                }
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
                    addOperandDeps(ai->lhs, worklist, liveSet, function);
                    addOperandDeps(ai->rhs, worklist, liveSet, function);
                    break;
                }
                case Operator::ICMP:
                case Operator::FCMP: {
                    auto* ci = static_cast<IcmpInst*>(inst);
                    addOperandDeps(ci->lhs, worklist, liveSet, function);
                    addOperandDeps(ci->rhs, worklist, liveSet, function);
                    break;
                }
                case Operator::PHI: {
                    auto* phi = static_cast<PhiInst*>(inst);
                    for (auto& [label, val] : phi->incomingVals)
                    {
                        addOperandDeps(val, worklist, liveSet, function);
                    }
                    break;
                }
                case Operator::BR_COND: {
                    auto* br = static_cast<BrCondInst*>(inst);
                    addOperandDeps(br->cond, worklist, liveSet, function);
                    break;
                }
                case Operator::RET: {
                    auto* ret = static_cast<RetInst*>(inst);
                    addOperandDeps(ret->res, worklist, liveSet, function);
                    break;
                }
                case Operator::CALL: {
                    auto* call = static_cast<CallInst*>(inst);
                    for (auto& [dt, arg] : call->args)
                    {
                        addOperandDeps(arg, worklist, liveSet, function);
                    }
                    break;
                }
                case Operator::GETELEMENTPTR: {
                    auto* gep = static_cast<GEPInst*>(inst);
                    addOperandDeps(gep->basePtr, worklist, liveSet, function);
                    for (auto* idx : gep->idxs)
                    {
                        addOperandDeps(idx, worklist, liveSet, function);
                    }
                    break;
                }
                case Operator::SITOFP: {
                    auto* si2fp = static_cast<SI2FPInst*>(inst);
                    addOperandDeps(si2fp->src, worklist, liveSet, function);
                    break;
                }
                case Operator::FPTOSI: {
                    auto* fp2si = static_cast<FP2SIInst*>(inst);
                    addOperandDeps(fp2si->src, worklist, liveSet, function);
                    break;
                }
                case Operator::ZEXT: {
                    auto* zext = static_cast<ZextInst*>(inst);
                    addOperandDeps(zext->src, worklist, liveSet, function);
                    break;
                }
                default:
                    break;
            }
        }

        // 第三步：删除所有非活跃指令
        for (auto& [bid, block] : function.blocks)
        {
            std::deque<Instruction*> newInsts;
            for (auto* inst : block->insts)
            {
                if (liveSet.find(inst) != liveSet.end())
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

    bool ADCEPass::isCritical(Instruction* inst) const
    {
        // 关键指令：
        // 1. 有副作用的指令（store, call, ret, branch）
        // 2. 可能影响程序行为的指令
        switch (inst->opcode)
        {
            case Operator::STORE:
            case Operator::CALL:
            case Operator::RET:
            case Operator::BR_COND:
            case Operator::BR_UNCOND:
                return true;
            default:
                return false;
        }
    }

    void ADCEPass::markLive(Instruction* inst, std::unordered_set<Instruction*>& liveSet)
    {
        if (liveSet.find(inst) == liveSet.end())
        {
            liveSet.insert(inst);
        }
    }

    void ADCEPass::addOperandDeps(Operand* op, std::queue<Instruction*>& worklist,
                                  std::unordered_set<Instruction*>& liveSet, Function& function)
    {
        if (!op || op->getType() != OperandType::REG)
            return;

        size_t regNum = op->getRegNum();

        // 查找定义此寄存器的指令
        for (auto& [bid, block] : function.blocks)
        {
            for (auto* inst : block->insts)
            {
                // 检查指令是否定义了这个寄存器
                Operand* defReg = nullptr;
                switch (inst->opcode)
                {
                    case Operator::LOAD:
                        defReg = static_cast<LoadInst*>(inst)->res;
                        break;
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
                        defReg = static_cast<ArithmeticInst*>(inst)->res;
                        break;
                    case Operator::ICMP:
                    case Operator::FCMP:
                        defReg = static_cast<IcmpInst*>(inst)->res;
                        break;
                    case Operator::PHI:
                        defReg = static_cast<PhiInst*>(inst)->res;
                        break;
                    case Operator::CALL:
                        defReg = static_cast<CallInst*>(inst)->res;
                        break;
                    case Operator::ALLOCA:
                        defReg = static_cast<AllocaInst*>(inst)->res;
                        break;
                    case Operator::GETELEMENTPTR:
                        defReg = static_cast<GEPInst*>(inst)->res;
                        break;
                    case Operator::SITOFP:
                        defReg = static_cast<SI2FPInst*>(inst)->dest;
                        break;
                    case Operator::FPTOSI:
                        defReg = static_cast<FP2SIInst*>(inst)->dest;
                        break;
                    case Operator::ZEXT:
                        defReg = static_cast<ZextInst*>(inst)->dest;
                        break;
                    default:
                        break;
                }

                if (defReg && defReg->getType() == OperandType::REG && defReg->getRegNum() == regNum)
                {
                    if (liveSet.find(inst) == liveSet.end())
                    {
                        liveSet.insert(inst);
                        worklist.push(inst);
                    }
                    return;  // 找到定义，结束搜索
                }
            }
        }
    }

}  // namespace ME
