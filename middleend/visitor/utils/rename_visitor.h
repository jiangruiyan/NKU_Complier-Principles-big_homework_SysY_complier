#ifndef __MIDDLEEND_VISITOR_UTILS_RENAME_VISITOR_H__
#define __MIDDLEEND_VISITOR_UTILS_RENAME_VISITOR_H__

#include <middleend/ir_visitor.h>
#include <middleend/module/ir_instruction.h>
#include <map>

namespace ME
{
    using RegRename_t = InsVisitor_t<void, RegMap&>;

    class RegRename : public RegRename_t
    {
      public:
        RegRename() = default;

        void visit(LoadInst&, RegMap&) override;
        void visit(StoreInst&, RegMap&) override;
        void visit(ArithmeticInst&, RegMap&) override;
        void visit(IcmpInst&, RegMap&) override;
        void visit(FcmpInst&, RegMap&) override;
        void visit(AllocaInst&, RegMap&) override;
        void visit(BrCondInst&, RegMap&) override;
        void visit(BrUncondInst&, RegMap&) override;
        void visit(GlbVarDeclInst&, RegMap&) override;
        void visit(CallInst&, RegMap&) override;
        void visit(FuncDeclInst&, RegMap&) override;
        void visit(FuncDefInst&, RegMap&) override;
        void visit(RetInst&, RegMap&) override;
        void visit(GEPInst&, RegMap&) override;
        void visit(FP2SIInst&, RegMap&) override;
        void visit(SI2FPInst&, RegMap&) override;
        void visit(ZextInst&, RegMap&) override;
        void visit(PhiInst&, RegMap&) override;
    };

    class SrcRegRename : public RegRename_t
    {
      public:
        SrcRegRename() = default;

        void visit(LoadInst&, RegMap&) override;
        void visit(StoreInst&, RegMap&) override;
        void visit(ArithmeticInst&, RegMap&) override;
        void visit(IcmpInst&, RegMap&) override;
        void visit(FcmpInst&, RegMap&) override;
        void visit(AllocaInst&, RegMap&) override;
        void visit(BrCondInst&, RegMap&) override;
        void visit(BrUncondInst&, RegMap&) override;
        void visit(GlbVarDeclInst&, RegMap&) override;
        void visit(CallInst&, RegMap&) override;
        void visit(FuncDeclInst&, RegMap&) override;
        void visit(FuncDefInst&, RegMap&) override;
        void visit(RetInst&, RegMap&) override;
        void visit(GEPInst&, RegMap&) override;
        void visit(FP2SIInst&, RegMap&) override;
        void visit(SI2FPInst&, RegMap&) override;
        void visit(ZextInst&, RegMap&) override;
        void visit(PhiInst&, RegMap&) override;
    };

    class ResRegRename : public RegRename_t
    {
      public:
        ResRegRename() = default;

        void visit(LoadInst&, RegMap&) override;
        void visit(StoreInst&, RegMap&) override;
        void visit(ArithmeticInst&, RegMap&) override;
        void visit(IcmpInst&, RegMap&) override;
        void visit(FcmpInst&, RegMap&) override;
        void visit(AllocaInst&, RegMap&) override;
        void visit(BrCondInst&, RegMap&) override;
        void visit(BrUncondInst&, RegMap&) override;
        void visit(GlbVarDeclInst&, RegMap&) override;
        void visit(CallInst&, RegMap&) override;
        void visit(FuncDeclInst&, RegMap&) override;
        void visit(FuncDefInst&, RegMap&) override;
        void visit(RetInst&, RegMap&) override;
        void visit(GEPInst&, RegMap&) override;
        void visit(FP2SIInst&, RegMap&) override;
        void visit(SI2FPInst&, RegMap&) override;
        void visit(ZextInst&, RegMap&) override;
        void visit(PhiInst&, RegMap&) override;
    };
}  // namespace ME

#endif  // __MIDDLEEND_VISITOR_UTILS_RENAME_VISITOR_H__
