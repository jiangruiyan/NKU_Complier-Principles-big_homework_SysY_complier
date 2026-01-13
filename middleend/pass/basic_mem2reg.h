#ifndef __MIDDLEEND_PASS_BASIC_MEM2REG_H__
#define __MIDDLEEND_PASS_BASIC_MEM2REG_H__

#include <interfaces/middleend/pass.h>
#include <middleend/module/ir_module.h>
#include <middleend/module/ir_function.h>
#include <middleend/module/ir_block.h>
#include <middleend/module/ir_instruction.h>
#include <middleend/visitor/utils/rename_visitor.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ME
{
    // 简易版 mem2reg：仅处理标量 alloca，且 load/store 直接作用于该 alloca 产生的指针
    class BasicMem2RegPass : public ModulePass
    {
      public:
        BasicMem2RegPass()  = default;
        ~BasicMem2RegPass() = default;

        void runOnModule(Module& module) override;
        void runOnFunction(Function& function) override;

      private:
        struct AllocaInfo
        {
            AllocaInst*                alloc      = nullptr;  // 该 alloca 指令
            size_t                     regId      = 0;        // alloca 结果寄存器号
            Block*                     allocBlock = nullptr;
            std::vector<StoreInst*>    stores;     // 对该指针的 store
            std::vector<LoadInst*>     loads;      // 对该指针的 load
            std::unordered_set<Block*> defBlocks;  // store 所在基本块
            std::unordered_set<Block*> useBlocks;  // load 所在基本块
        };

        using RegId = size_t;

        // 1 次遍历：收集函数内所有标量 alloca 及其直接 load/store
        void collectFunctionAllocaInfos(Function& function, std::unordered_map<RegId, AllocaInfo>& infos);

        // Case 1：该 alloca 无任何 use（无 load）。标记删除相关的 alloca 和 store。
        void markDeadAllocaDeletes(const AllocaInfo& info, std::unordered_set<Instruction*>& delSet);

        // Case 2：该 alloca 的 def/use 全在同一基本块。单次遍历该块，记录替换并标记删除 load/store。
        // 注意：若遇到 store 之前的 load，当前实现不会将其替换为 undef（框架未提供 Undef 操作数），因此保留该 load。
        // 返回值：仅当该块内针对该 alloca 的所有 load 均被替换删除时，返回 true（可安全删除该 alloca）。
        bool processLocalAllocaInBlock(
            Block* block, const AllocaInfo& info, RegMap& renameMap, std::unordered_set<Instruction*>& delSet);

        // 应用寄存器替换（源操作数）
        void applySrcRegRename(Function& function, RegMap& renameMap);

        // 批量删除：线性重建每个基本块的 deque
        void applyBatchDelete(Function& function, const std::unordered_set<Instruction*>& delSet);
        
        // 将 RegMap 的链式映射压缩为直接映射，避免出现使用已删除的中间寄存器
        void flattenRegRenameMap(RegMap& renameMap);
    };

}  // namespace ME

#endif  // __MIDDLEEND_PASS_BASIC_MEM2REG_H__