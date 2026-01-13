#include <middleend/visitor/printer/module_printer.h>

namespace ME
{
    void IRPrinter::visit(LoadInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(StoreInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(ArithmeticInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(IcmpInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(FcmpInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(AllocaInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(BrCondInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(BrUncondInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(GlbVarDeclInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(CallInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(FuncDeclInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(FuncDefInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(RetInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(GEPInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(FP2SIInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(SI2FPInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(ZextInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
    void IRPrinter::visit(PhiInst& inst, std::ostream& os)
    {
        (void)inst;
        os << inst.toString();
    }
}  // namespace ME
