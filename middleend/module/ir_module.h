#ifndef __MIDDLEEND_MODULE_IR_MODULE_H__
#define __MIDDLEEND_MODULE_IR_MODULE_H__

#include <middleend/module/ir_function.h>

namespace ME
{
    class Module : public Visitable
    {
      public:
        std::vector<GlbVarDeclInst*> globalVars;
        std::vector<FuncDeclInst*>   funcDecls;
        std::vector<Function*>       functions;

      public:
        Module();
        ~Module();

      public:
        virtual void accept(Visitor& visitor) override { visitor.visit(*this); }
    };
}  // namespace ME

#endif  // __MIDDLEEND_MODULE_IR_MODULE_H__
