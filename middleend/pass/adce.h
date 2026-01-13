#pragma once

#include <unordered_set>
#include <queue>

namespace ME
{
    class Module;
    class Function;
    class Instruction;
    class Operand;

    // Aggressive Dead Code Elimination Pass
    // 激进死代码消除：删除所有对程序输出无影响的指令
    class ADCEPass
    {
      public:
        void runOnModule(Module& module);
        void runOnFunction(Function& function);

      private:
        // 判断指令是否关键（有副作用）
        bool isCritical(Instruction* inst) const;
        
        // 标记指令及其依赖为活跃
        void markLive(Instruction* inst, std::unordered_set<Instruction*>& liveSet);
        
        // 添加操作数依赖到工作列表
        void addOperandDeps(Operand* op, std::queue<Instruction*>& worklist, 
                           std::unordered_set<Instruction*>& liveSet, Function& function);
    };

}  // namespace ME
