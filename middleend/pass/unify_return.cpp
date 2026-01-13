#include <middleend/pass/unify_return.h>
#include <middleend/pass/analysis/analysis_manager.h>
#include <middleend/pass/analysis/cfg.h>
#include <middleend/module/ir_operand.h>
#include <algorithm>
#include <iostream>

namespace ME
{
    void UnifyReturnPass::runOnModule(Module& module)
    {
        for (auto* function : module.functions) unifyFunctionReturns(*function);
    }

    void UnifyReturnPass::runOnFunction(Function& function) { unifyFunctionReturns(function); }

    void UnifyReturnPass::unifyFunctionReturns(Function& function)
    {
        auto* cfg = Analysis::AM.get<Analysis::CFG>(function);

        auto retInstructions = findReturnInstructions(cfg);

        if (retInstructions.size() <= 1) return;

        Block* exitBlock = function.createBlock();

        std::vector<std::pair<Operand*, Operand*>> returnValues;
        DataType                                   returnType = DataType::VOID;

        for (auto* retInst : retInstructions)
        {
            Block* containingBlock = getBlockContaining(function, retInst);
            if (!containingBlock) continue;

            returnType = retInst->rt;

            Operand* labelOp = getLabelOperand(containingBlock->blockId);

            if (retInst->res)
                returnValues.push_back({retInst->res, labelOp});
            else
                returnValues.push_back({nullptr, labelOp});

            auto it = std::find(containingBlock->insts.begin(), containingBlock->insts.end(), retInst);
            if (it != containingBlock->insts.end())
            {
                Operand* exitLabel  = getLabelOperand(exitBlock->blockId);
                auto*    branchInst = new BrUncondInst(exitLabel);
                *it                 = branchInst;
                delete retInst;
            }
        }

        if (returnType != DataType::VOID && !returnValues.empty())
        {
            std::vector<std::pair<Operand*, Operand*>> validValues;
            for (auto& [val, label] : returnValues)
                if (val != nullptr) validValues.push_back({val, label});

            if (!validValues.empty())
            {
                Operand* resultReg = getRegOperand(function.getNewRegId());

                auto* phiInst = new PhiInst(returnType, resultReg);
                for (auto& [val, label] : validValues) phiInst->addIncoming(val, label);
                exitBlock->insertBack(phiInst);

                auto* finalRet = new RetInst(returnType, resultReg);
                exitBlock->insertBack(finalRet);
            }
            else
            {
                auto* finalRet = new RetInst(DataType::VOID, nullptr);
                exitBlock->insertBack(finalRet);
            }
        }
        else
        {
            auto* finalRet = new RetInst(DataType::VOID, nullptr);
            exitBlock->insertBack(finalRet);
        }

        // 由于在 `if (retInstructions.size() <= 1) return;` 处没有退出
        // 我们可以确定该 pass 的执行一定向当前函数插入了新的基本块并修改了跳转关系
        // 因此需要 invalidate 当前函数的 CFG 缓存
        Analysis::AM.invalidate(function);
    }

    std::vector<RetInst*> UnifyReturnPass::findReturnInstructions(Analysis::CFG* cfg)
    {
        std::vector<RetInst*> retInstructions;

        for (auto& [blockId, block] : cfg->id2block)
        {
            for (auto* inst : block->insts)
            {
                if (inst->opcode != Operator::RET) continue;
                retInstructions.push_back(static_cast<RetInst*>(inst));
            }
        }

        return retInstructions;
    }

    Block* UnifyReturnPass::getBlockContaining(Function& function, Instruction* inst)
    {
        auto* cfg = Analysis::AM.get<Analysis::CFG>(function);
        for (auto& [blockId, block] : cfg->id2block)
        {
            auto it = std::find(block->insts.begin(), block->insts.end(), inst);
            if (it != block->insts.end()) return block;
        }
        return nullptr;
    }

}  // namespace ME
