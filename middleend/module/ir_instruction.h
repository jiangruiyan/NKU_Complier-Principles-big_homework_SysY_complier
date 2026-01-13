#ifndef __MIDDLEEND_MODULE_IR_INSTRUCTION_H__
#define __MIDDLEEND_MODULE_IR_INSTRUCTION_H__

#include <middleend/ir_defs.h>
#include <middleend/ir_visitor.h>
#include <middleend/module/ir_operand.h>
#include <frontend/ast/ast_defs.h>
#include <string>
#include <vector>
#include <utility>

#define ENABLE_IRINST_COMMENT

namespace ME
{ /*
   * 本文件定义了LLVM IR的指令类，在完成Lab3-2中间代码生成时，你的一个工作重点就是
   * 根据AST构建这些指令实例。
   * 你可以根据需要自行添加成员变量和函数，辅助你完成实验。
   */
    class Instruction : public Visitable, public InsVisitable
    {
      public:
        Operator opcode;

      public:
#ifndef ENABLE_IRINST_COMMENT
        Instruction(Operator op, const std::string& c = "") : opcode(op) {}
        void        setComment(const std::string& c) {}
        std::string getComment() const
        {
            if (comment.empty()) return "";
            return " ; " + comment;
        }
#else
        std::string comment;
        Instruction(Operator op, const std::string& c = "") : opcode(op), comment(c) {}
        void        setComment(const std::string& c) { comment = c; }
        std::string getComment() const { return ""; }
#endif
        virtual ~Instruction() = default;

      public:
        virtual std::string toString() const                     = 0;
        virtual void        accept(Visitor& visitor) override    = 0;
        virtual void        accept(InsVisitor& visitor) override = 0;

        virtual bool isTerminator() const = 0;
    };

    class LoadInst : public Instruction
    {
      public:
        DataType dt;
        Operand* ptr;
        Operand* res;

      public:
        LoadInst(DataType t, Operand* p, Operand* d, const std::string& c = "")
            : Instruction(Operator::LOAD, c), dt(t), ptr(p), res(d)
        {}
        ~LoadInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class StoreInst : public Instruction
    {
      public:
        DataType dt;
        Operand* ptr;
        Operand* val;

      public:
        StoreInst(DataType t, Operand* v, Operand* p, const std::string& c = "")
            : Instruction(Operator::STORE, c), dt(t), ptr(p), val(v)
        {}
        ~StoreInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class ArithmeticInst : public Instruction
    {
      public:
        DataType dt;
        Operand* lhs;
        Operand* rhs;
        Operand* res;

      public:
        ArithmeticInst(Operator op, DataType t, Operand* l, Operand* r, Operand* d, const std::string& c = "")
            : Instruction(op, c), dt(t), lhs(l), rhs(r), res(d)
        {}
        ~ArithmeticInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class IcmpInst : public Instruction
    {
      public:
        DataType dt;
        ICmpOp   cond;
        Operand* lhs;
        Operand* rhs;
        Operand* res;

      public:
        IcmpInst(DataType t, ICmpOp c, Operand* l, Operand* r, Operand* res)
            : Instruction(Operator::ICMP), dt(t), cond(c), lhs(l), rhs(r), res(res)
        {}
        ~IcmpInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class FcmpInst : public Instruction
    {
      public:
        DataType dt;
        FCmpOp   cond;
        Operand* lhs;
        Operand* rhs;
        Operand* res;

      public:
        FcmpInst(DataType t, FCmpOp c, Operand* l, Operand* r, Operand* res)
            : Instruction(Operator::FCMP), dt(t), cond(c), lhs(l), rhs(r), res(res)
        {}
        ~FcmpInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class AllocaInst : public Instruction
    {
      public:
        DataType         dt;
        Operand*         res;
        std::vector<int> dims;

      public:
        AllocaInst(DataType t, Operand* r, std::vector<int> d = {}, const std::string& c = "")
            : Instruction(Operator::ALLOCA, c), dt(t), res(r), dims(d)
        {}
        ~AllocaInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class BrCondInst : public Instruction
    {
      public:
        Operand* cond;
        Operand* trueTar;
        Operand* falseTar;

      public:
        BrCondInst(Operand* c, Operand* t, Operand* f, const std::string& cm = "")
            : Instruction(Operator::BR_COND, cm), cond(c), trueTar(t), falseTar(f)
        {}
        ~BrCondInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return true; }
    };

    class BrUncondInst : public Instruction
    {
      public:
        Operand* target;

      public:
        BrUncondInst(Operand* t, const std::string& c = "") : Instruction(Operator::BR_UNCOND, c), target(t) {}
        ~BrUncondInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return true; }
    };

    class GlbVarDeclInst : public Instruction
    {
      public:
        DataType         dt;
        std::string      name;
        Operand*         init;
        FE::AST::VarAttr initList;

      public:
        GlbVarDeclInst(DataType t, const std::string& n, Operand* i = nullptr)
            : Instruction(Operator::GLOBAL_VAR), dt(t), name(n), init(i)
        {}
        GlbVarDeclInst(DataType t, const std::string& n, FE::AST::VarAttr il)
            : Instruction(Operator::GLOBAL_VAR), dt(t), name(n), init(nullptr), initList(il)
        {}
        ~GlbVarDeclInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class CallInst : public Instruction
    {
      public:
        DataType    retType;
        std::string funcName;

        using argType = DataType;
        using argOp   = Operand*;
        using argPair = std::pair<argType, argOp>;
        using argList = std::vector<argPair>;
        argList  args;
        Operand* res;

      public:
        CallInst(DataType rt, const std::string& fn, Operand* r = nullptr, const std::string& c = "")
            : Instruction(Operator::CALL, c), retType(rt), funcName(fn), args({}), res(r)
        {}
        CallInst(DataType rt, const std::string& fn, argList a, Operand* r = nullptr, const std::string& c = "")
            : Instruction(Operator::CALL, c), retType(rt), funcName(fn), args(a), res(r)
        {}
        ~CallInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class RetInst : public Instruction
    {
      public:
        DataType rt;
        Operand* res;

      public:
        RetInst(DataType t, Operand* r = nullptr, const std::string& c = "")
            : Instruction(Operator::RET, c), rt(t), res(r)
        {}
        ~RetInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return true; }
    };

    class FuncDeclInst : public Instruction
    {
      public:
        DataType              retType;
        std::string           funcName;
        std::vector<DataType> argTypes;
        bool                  isVarArg;

      public:
        FuncDeclInst(DataType rt, const std::string& fn, std::vector<DataType> at = {}, bool is_va = false,
            const std::string& c = "")
            : Instruction(Operator::FUNCDECL, c), retType(rt), funcName(fn), argTypes(at), isVarArg(is_va)
        {}
        ~FuncDeclInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class FuncDefInst : public Instruction
    {
      public:
        DataType    retType;
        std::string funcName;

        using argType = DataType;
        using argOp   = Operand*;
        using argPair = std::pair<argType, argOp>;
        using argList = std::vector<argPair>;
        argList argRegs;

      public:
        FuncDefInst(DataType rt, const std::string& fn, argList ar = {}, const std::string& c = "")
            : Instruction(Operator::FUNCDEF, c), retType(rt), funcName(fn), argRegs(ar)
        {}
        ~FuncDefInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class GEPInst : public Instruction
    {
      public:
        DataType              dt;
        DataType              idxType;
        Operand*              basePtr;
        Operand*              res;
        std::vector<int>      dims;
        std::vector<Operand*> idxs;

      public:
        GEPInst(
            DataType t, DataType it, Operand* bp, Operand* r, std::vector<int> d = {}, std::vector<Operand*> is = {})
            : Instruction(Operator::GETELEMENTPTR), dt(t), idxType(it), basePtr(bp), res(r), dims(d), idxs(is)
        {}
        ~GEPInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class SI2FPInst : public Instruction
    {
      public:
        Operand* src;
        Operand* dest;

      public:
        SI2FPInst(Operand* s, Operand* d) : Instruction(Operator::SITOFP), src(s), dest(d) {}
        ~SI2FPInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class FP2SIInst : public Instruction
    {
      public:
        Operand* src;
        Operand* dest;

      public:
        FP2SIInst(Operand* s, Operand* d) : Instruction(Operator::FPTOSI), src(s), dest(d) {}
        ~FP2SIInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class ZextInst : public Instruction
    {
      public:
        DataType from;
        DataType to;
        Operand* src;
        Operand* dest;

      public:
        ZextInst(DataType f, DataType t, Operand* s, Operand* d)
            : Instruction(Operator::ZEXT), from(f), to(t), src(s), dest(d)
        {}
        ~ZextInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }

        virtual bool isTerminator() const override { return false; }
    };

    class PhiInst : public Instruction
    {
      public:
        DataType dt;
        Operand* res;

        using ValOp   = Operand*;
        using LabelOp = Operand*;
        std::map<LabelOp, ValOp> incomingVals;  // label -> value

      public:
        // 注意这里的参数顺序：类型、结果寄存器、可选注释
        PhiInst(DataType t, Operand* r, const std::string& c = "")
            : Instruction(Operator::PHI, c), dt(t), res(r), incomingVals({})
        {}
        ~PhiInst() override = default;

      public:
        virtual std::string toString() const override;
        virtual void        accept(Visitor& visitor) override { visitor.visit(*this); }
        virtual void        accept(InsVisitor& visitor) override { visitor.visit(*this); }
        void                addIncoming(ValOp v, LabelOp l)
        {
            auto it = incomingVals.find(l);
            if (it != incomingVals.end())
            {
                ASSERT(it->second == v && "Inconsistent phi incoming value for the same label");
            }
            incomingVals[l] = v;
        }

        virtual bool isTerminator() const override { return false; }
    };
}  // namespace ME

#endif  // __MIDDLEEND_MODULE_IR_INSTRUCTION_H__
