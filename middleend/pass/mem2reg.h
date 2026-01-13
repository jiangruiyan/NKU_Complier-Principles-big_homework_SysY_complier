#pragma once
#ifndef __MIDDLEEND_PASS_MEM2REG_H__
#define __MIDDLEEND_PASS_MEM2REG_H__

#include <interfaces/middleend/pass.h>
#include <middleend/module/ir_module.h>
#include <middleend/module/ir_function.h>
#include <middleend/module/ir_block.h>
#include <middleend/module/ir_instruction.h>
#include <middleend/pass/analysis/cfg.h>
#include <middleend/pass/analysis/dominfo.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <set>

namespace ME
{
    // 将基于栈的 Alloca + Load/Store 提升为 SSA
    class Mem2RegPass : public ModulePass
    {
      public:
        Mem2RegPass()  = default;
        ~Mem2RegPass() = default;

        void runOnModule(Module& module) override;
        void runOnFunction(Function& function) override;

      private:
        struct VarInfo
        {
            AllocaInst*   allocaInst{nullptr};  // 要提升的变量
            DataType      ty{DataType::UNK};
            size_t        ptrReg{(size_t)-1};  // Alloca 的结果寄存器号
            std::set<int> defBlocks;           // 有 Store 的块
            // phi 放置块 -> phi 指令
            std::unordered_map<int, PhiInst*> phiAtBlock;
        };

        // 入口：对单个函数做 mem2reg
        void promoteInFunction(Function& func);

        // 收集入口块中的可提升 Alloca，并检查其使用是否只在 load/store
        void collectPromotableAllocas(Analysis::CFG* cfg, std::vector<VarInfo>& vars);

        // 基于支配前沿插入 phi，为每个变量填充 VarInfo::phiAtBlock
        void insertPhi(Function& func, Analysis::CFG* cfg, Analysis::DomInfo* dom, std::vector<VarInfo>& vars);

        // 沿支配树做重命名，删除 load/store，补充 phi incoming
        void renameAndCleanup(Function& func, Analysis::CFG* cfg, Analysis::DomInfo* dom, std::vector<VarInfo>& vars);

        // 工具：在全函数内将 fromReg 的使用替换为 toOp（不替换定义位）
        void replaceRegUsesInFunction(Function& func, size_t fromReg, Operand* toOp);

        // 工具：对单条指令进行寄存器操作数替换
        static void replaceRegUseInInst(Instruction* inst, size_t fromReg, Operand* toOp);

        // 小工具
        static bool isRegOperand(Operand* op, size_t& outReg);
        static bool isSameReg(Operand* op, size_t reg);
    };
}  // namespace ME

#endif  // __MIDDLEEND_PASS_MEM2REG_H__