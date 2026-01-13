#ifndef __INTERFACES_MIDDLEEND_PASS_H__
#define __INTERFACES_MIDDLEEND_PASS_H__

namespace ME
{
    class Module;
    class Function;
}  // namespace ME

namespace ME
{
    class Pass
    {
      public:
        virtual ~Pass()                                = default;
        virtual void runOnModule(Module& module)       = 0;
        virtual void runOnFunction(Function& function) = 0;
    };

    class ModulePass : public Pass
    { /*
       * 全局优化Pass的基类
       */
      public:
        virtual void runOnModule(Module& module) override       = 0;
        virtual void runOnFunction(Function& function) override = 0;
    };

    class FunctionPass : public Pass
    { /*
       * 过程内优化Pass的基类
       */
      public:
        virtual void runOnModule(Module& module) override;
        virtual void runOnFunction(Function& function) override = 0;
    };
}  // namespace ME

#endif  // __INTERFACES_MIDDLEEND_PASS_H__
