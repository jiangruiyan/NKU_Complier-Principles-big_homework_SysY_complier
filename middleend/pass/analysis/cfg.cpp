#include <middleend/pass/analysis/cfg.h>
#include <middleend/pass/analysis/analysis_manager.h>
#include <middleend/module/ir_function.h>
#include <middleend/module/ir_block.h>
#include <middleend/module/ir_instruction.h>
#include <middleend/module/ir_operand.h>
#include <algorithm>

namespace ME::Analysis
{
    CFG::CFG() { id2block.clear(); }

    void CFG::build(ME::Function& function)
    {
        func = &function;
        id2block.clear();

        for (auto& [blockId, block] : function.blocks) id2block[blockId] = block;

        if (id2block.empty()) return;

        size_t maxBlockId = 0;
        for (auto& [blockId, block] : id2block) maxBlockId = std::max(maxBlockId, blockId);

        G.clear();
        invG.clear();
        G_id.clear();
        invG_id.clear();

        G.resize(maxBlockId + 1);
        invG.resize(maxBlockId + 1);
        G_id.resize(maxBlockId + 1);
        invG_id.resize(maxBlockId + 1);

        std::map<size_t, bool> visited;
        buildFromBlock(0, visited);

        auto blocks_temp = func->blocks;
        func->blocks.clear();

        for (auto& [blockId, block] : blocks_temp)
            if (visited[blockId]) func->blocks[blockId] = block;

        id2block.clear();
        for (auto& [blockId, block] : func->blocks) id2block[blockId] = block;

        for (size_t i = 0; i <= maxBlockId; ++i)
        {
            if (!visited[i]) continue;

            auto& edges = G_id[i];
            edges.erase(
                std::remove_if(edges.begin(), edges.end(), [&](size_t target_id) { return !visited[target_id]; }),
                edges.end());
        }

        for (size_t i = 0; i <= maxBlockId; ++i)
        {
            if (!visited[i]) continue;

            auto& edges = invG_id[i];
            edges.erase(
                std::remove_if(edges.begin(), edges.end(), [&](size_t source_id) { return !visited[source_id]; }),
                edges.end());
        }
    }

    void CFG::buildFromBlock(size_t blockId, std::map<size_t, bool>& visited)
    {
        if (visited[blockId] || id2block.find(blockId) == id2block.end()) return;

        visited[blockId]        = true;
        ME::Block* currentBlock = id2block[blockId];

        Instruction* terminator = nullptr;
        for (auto inst : currentBlock->insts)
        {
            if (!inst->isTerminator()) continue;

            terminator = inst;
            break;
        }

        if (!terminator) return;

        if (terminator->opcode == Operator::BR_COND)
        {
            BrCondInst* brInst = static_cast<BrCondInst*>(terminator);

            if (brInst->trueTar->getType() == OperandType::LABEL && brInst->falseTar->getType() == OperandType::LABEL)
            {
                LabelOperand* trueLabel  = static_cast<LabelOperand*>(brInst->trueTar);
                LabelOperand* falseLabel = static_cast<LabelOperand*>(brInst->falseTar);

                size_t trueLabelId  = trueLabel->lnum;
                size_t falseLabelId = falseLabel->lnum;

                if (id2block.find(trueLabelId) != id2block.end())
                {
                    G[blockId].push_back(id2block[trueLabelId]);
                    G_id[blockId].push_back(trueLabelId);
                    invG[trueLabelId].push_back(currentBlock);
                    invG_id[trueLabelId].push_back(blockId);

                    buildFromBlock(trueLabelId, visited);
                }

                if (id2block.find(falseLabelId) != id2block.end())
                {
                    G[blockId].push_back(id2block[falseLabelId]);
                    G_id[blockId].push_back(falseLabelId);
                    invG[falseLabelId].push_back(currentBlock);
                    invG_id[falseLabelId].push_back(blockId);

                    buildFromBlock(falseLabelId, visited);
                }
            }
        }
        else if (terminator->opcode == Operator::BR_UNCOND)
        {
            BrUncondInst* brInst = static_cast<BrUncondInst*>(terminator);

            if (brInst->target->getType() == OperandType::LABEL)
            {
                LabelOperand* targetLabel   = static_cast<LabelOperand*>(brInst->target);
                size_t        targetLabelId = targetLabel->lnum;

                if (id2block.find(targetLabelId) != id2block.end())
                {
                    G[blockId].push_back(id2block[targetLabelId]);
                    G_id[blockId].push_back(targetLabelId);
                    invG[targetLabelId].push_back(currentBlock);
                    invG_id[targetLabelId].push_back(blockId);

                    buildFromBlock(targetLabelId, visited);
                }
            }
        }
    }

    template <>
    CFG* Manager::get<CFG>(Function& func)
    {
        if (auto* cached = getCached<CFG>(func)) return cached;

        auto* cfg = new CFG();
        cfg->build(func);
        cache<CFG>(func, cfg);
        return cfg;
    }
}  // namespace ME::Analysis
