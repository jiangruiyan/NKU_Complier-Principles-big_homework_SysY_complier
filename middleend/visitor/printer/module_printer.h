#ifndef __MIDDLEEND_VISITOR_PRINTER_MODULE_PRINTER_H__
#define __MIDDLEEND_VISITOR_PRINTER_MODULE_PRINTER_H__

#include <middleend/ir_visitor.h>
#include <middleend/module/ir_module.h>

namespace ME
{
    using Printer_t = Visitor_t<void, std::ostream&>;

    class IRPrinter : public Printer_t
    {
      public:
        void visit(Module& module, std::ostream& os) override;
        void visit(Function& func, std::ostream& os) override;
        void visit(Block& block, std::ostream& os) override;

        void visit(LoadInst& inst, std::ostream& os) override;
        void visit(StoreInst& inst, std::ostream& os) override;
        void visit(ArithmeticInst& inst, std::ostream& os) override;
        void visit(IcmpInst& inst, std::ostream& os) override;
        void visit(FcmpInst& inst, std::ostream& os) override;
        void visit(AllocaInst& inst, std::ostream& os) override;
        void visit(BrCondInst& inst, std::ostream& os) override;
        void visit(BrUncondInst& inst, std::ostream& os) override;
        void visit(GlbVarDeclInst& inst, std::ostream& os) override;
        void visit(CallInst& inst, std::ostream& os) override;
        void visit(FuncDeclInst& inst, std::ostream& os) override;
        void visit(FuncDefInst& inst, std::ostream& os) override;
        void visit(RetInst& inst, std::ostream& os) override;
        void visit(GEPInst& inst, std::ostream& os) override;
        void visit(FP2SIInst& inst, std::ostream& os) override;
        void visit(SI2FPInst& inst, std::ostream& os) override;
        void visit(ZextInst& inst, std::ostream& os) override;
        void visit(PhiInst& inst, std::ostream& os) override;
    };
}  // namespace ME

#endif  // __MIDDLEEND_VISITOR_PRINTER_MODULE_PRINTER_H__
