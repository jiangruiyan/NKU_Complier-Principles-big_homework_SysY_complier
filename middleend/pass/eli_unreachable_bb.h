#pragma once

#include <vector>
#include <unordered_set>

namespace ME
{

    class Module;
    class Function;
    class Block;
    class Instruction;

    class EliminateUnreachableBBPass
    {
      public:
        void runOnModule(Module& module);
        void runOnFunction(Function& function);

      private:
        // 删除块内第一个终止指令之后的所有指令（若存在）
        void pruneAfterTerminator(Block* block);

        // // 判断是否为终止指令（ret / branch）
        // bool isTerminator(Instruction* inst) const;
    };

}  // namespace ME