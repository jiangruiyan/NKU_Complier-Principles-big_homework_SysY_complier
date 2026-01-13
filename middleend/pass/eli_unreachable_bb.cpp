#include <middleend/pass/eli_unreachable_bb.h>
#include <middleend/pass/analysis/analysis_manager.h>
#include <middleend/pass/analysis/cfg.h>
#include <middleend/module/ir_module.h>
#include <middleend/module/ir_function.h>
#include <middleend/module/ir_block.h>
#include <middleend/module/ir_instruction.h>

#include <unordered_set>
#include <vector>

namespace ME
{

    void EliminateUnreachableBBPass::runOnModule(Module& module)
    {
        for (auto* func : module.functions)
        {
            if (func) runOnFunction(*func);
        }
    }

    void EliminateUnreachableBBPass::runOnFunction(Function& function)
    {
        // 构建/获取 CFG（该实现已从入口块 0 做到达性分析）
        auto* cfg = Analysis::AM.get<Analysis::CFG>(function);
        if (!cfg) return;

        // 若入口块不存在则不处理
        if (cfg->id2block.find(0) == cfg->id2block.end()) return;

        // 用 CFG 的 G_id 做一次 DFS，仅对首次访问到的块执行剪除终止指令之后的死代码
        std::unordered_set<size_t> visited;
        std::vector<size_t>        stack;
        stack.push_back(0);

        while (!stack.empty())
        {
            size_t bid = stack.back(); // 当前处理的基本块 ID
            stack.pop_back();
            if (visited.count(bid)) continue;
            visited.insert(bid);

            auto itBlk = cfg->id2block.find(bid);
            if (itBlk == cfg->id2block.end() || !itBlk->second) continue;

            Block* blk = itBlk->second;
            pruneAfterTerminator(blk);

            if (bid < cfg->G_id.size())
            {
                for (size_t succId : cfg->G_id[bid])
                {
                    if (!visited.count(succId)) stack.push_back(succId);
                }
            }
        }

        // 删除不可达基本块（防御性实现；CFG::build 已经基于 visited 过滤过 blocks）
        std::vector<size_t> toRemove;
        for (auto& [blockId, block] : function.blocks)
        {
            if (!visited.count(blockId)) toRemove.push_back(blockId);
        }
        for (size_t blockId : toRemove)
        {
            Block* blk = function.blocks[blockId];
            if (blk)
            {
                for (auto* inst : blk->insts) delete inst;
                blk->insts.clear();
                delete blk;
            }
            function.blocks.erase(blockId);
        }

        // 控制流/指令已改变，失效当前函数的分析缓存
        Analysis::AM.invalidate(function);
    }

    void EliminateUnreachableBBPass::pruneAfterTerminator(Block* block)
    {
        if (!block) return;

        int termIdx = -1;
        for (int i = 0; i < static_cast<int>(block->insts.size()); ++i)
        {
            if (block->insts[i] && block->insts[i]->isTerminator())
            {
                termIdx = i;
                break;
            }
        }

        if (termIdx >= 0 && termIdx + 1 < static_cast<int>(block->insts.size()))
        {
            for (int i = termIdx + 1; i < static_cast<int>(block->insts.size()); ++i) { delete block->insts[i]; }
            block->insts.erase(block->insts.begin() + termIdx + 1, block->insts.end());
        }
    }

}  // namespace ME