#pragma once

#include <unordered_map>

namespace ME
{
    class Module;
    class Function;
    class Instruction;
    class Operand;

    // Sparse Conditional Constant Propagation (简化实现)
    // 目标：在函数内对标量寄存器做稀疏常量传播/折叠并替换为立即数
    class SCCPPass
    {
      public:
        // Lattice types made public so implementation files can reference them
        enum class ValState { Unknown, ConstI32, ConstF32, Overdefined };

        struct LatticeVal
        {
            ValState state = ValState::Unknown;
            int i32 = 0;
            float f32 = 0.0f;
        };

        void runOnModule(Module& module);
        void runOnFunction(Function& function);

      private:
        bool evaluateInstructionConst(Instruction* inst, LatticeVal& out);
    };

} // namespace ME
