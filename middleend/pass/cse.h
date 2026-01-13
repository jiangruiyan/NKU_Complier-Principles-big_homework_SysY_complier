#pragma once

#include <cstddef>
#include <unordered_map>

namespace ME
{
    class Module;
    class Function;
    class Instruction;
    class Operand;
    
    // Common Subexpression Elimination Pass
    // 公共子表达式消除：识别并消除重复计算
    class CSEPass
    {
      public:
        void runOnModule(Module& module);
        void runOnFunction(Function& function);

      private:
        // 计算指令的哈希值（用于识别相同的表达式）
        size_t hashInstruction(Instruction* inst) const;
        
        // 判断两条指令是否等价
        bool areInstructionsEquivalent(Instruction* i1, Instruction* i2) const;
        
        // 判断两个操作数是否等价
        bool areOperandsEquivalent(Operand* o1, Operand* o2) const;
    };

}  // namespace ME
