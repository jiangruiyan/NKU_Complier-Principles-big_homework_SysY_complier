#ifndef __FRONTEND_AST_AST_DEFS_H__
#define __FRONTEND_AST_AST_DEFS_H__

#include <debug.h>
#include <iostream>
#include <vector>
#include <array>
#include <map>

#define AST_TYPEGROUP_DECL  \
    X(BASIC, basic type, 0) \
    X(POINTER, pointer type, 1)

#define AST_BASETYPE_DECL                                                           \
    X(UNK, unknown type, 0)                                                         \
    X(VOID, void, 1)                                                                \
    X(BOOL, bool, 2)                                                                \
    X(INT, int, 3)                                                                  \
    X(LL, long long, 4) /* 前端解析中用于暂存超出 int 范围的数字 */ \
    X(FLOAT, float, 5)

#define AST_OPERATOR_DECL       \
    X(UNK, unknown operator, 0) \
    X(ADD, +, 1)                \
    X(SUB, -, 2)                \
    X(MUL, *, 3)                \
    X(DIV, /, 4)                \
    X(GT, >, 5)                 \
    X(GE, >=, 6)                \
    X(LT, <, 7)                 \
    X(LE, <=, 8)                \
    X(EQ, ==, 9)                \
    X(MOD, %, 10)               \
    X(NEQ, !=, 11)              \
    X(NOT, !, 12)               \
    X(BITOR, |, 13)             \
    X(BITAND, &, 14)            \
    X(AND, &&, 15)              \
    X(OR, ||, 16)               \
    X(ASSIGN, =, 17)

namespace FE::AST
{
    constexpr size_t maxTypeIdx = []() {
        int max_idx = 0;
#define X(name, lname, idx)        \
    /* static_assert(idx >= 0); */ \
    if (idx > max_idx) max_idx = idx;
        AST_BASETYPE_DECL
#undef X
        return static_cast<size_t>(max_idx);
    }();

    enum class TypeGroup
    {
#define X(name, lname, idx) name = idx,
        AST_TYPEGROUP_DECL
#undef X
    };

    enum class Type_t
    {
#define X(name, lname, idx) name = idx,
        AST_BASETYPE_DECL
#undef X
    };

    struct Type
    {
        virtual std::string toString() const     = 0;
        virtual Type_t      getBaseType() const  = 0;
        virtual TypeGroup   getTypeGroup() const = 0;

        virtual ~Type() = default;
    };

    struct BasicType : public Type
    {
        friend class TypeFactory;

        Type_t      base;
        std::string toString() const override
        {
            switch (base)
            {
#define X(name, lname, idx) \
    case Type_t::name: return #lname;
                AST_BASETYPE_DECL
#undef X
                default: return "unknown type";
            }
        }
        Type_t    getBaseType() const override { return base; }
        TypeGroup getTypeGroup() const override { return TypeGroup::BASIC; }

      private:
        BasicType(Type_t t = Type_t::UNK) : base(t) {}
    };

    struct PtrType : public Type
    {
        friend class TypeFactory;

        Type*       base;
        std::string toString() const override { return base->toString() + "*"; }
        Type_t      getBaseType() const override { return base ? base->getBaseType() : Type_t::UNK; }
        TypeGroup   getTypeGroup() const override { return TypeGroup::POINTER; }

      private:
        PtrType(Type* t = nullptr) : base(t) {}
    };

    class TypeFactory
    {
      public:
        static Type* getBasicType(Type_t t = Type_t::UNK);
        static Type* getPtrType(Type* t = nullptr);

        static TypeFactory& getInstance()
        {
            static TypeFactory instance;
            return instance;
        }

      private:
        TypeFactory();
        ~TypeFactory();

        static std::array<Type*, maxTypeIdx + 1> baseTypes;

        using PtrBase_t = Type*;
        using PtrType_t = Type*;
        static std::map<PtrBase_t, PtrType_t> ptrTypeMap;
    };

    extern Type* voidType;
    extern Type* boolType;
    extern Type* intType;
    extern Type* llType;
    extern Type* floatType;

    enum class Operator
    {
#define X(name, lname, idx) name = idx,
        AST_OPERATOR_DECL
#undef X
    };

    struct VarValue
    {
        Type* type;
        union
        {
            bool      boolValue;
            int       intValue;
            long long llValue;
            float     floatValue;
        };

        VarValue() : type(voidType), intValue(0) {}
        VarValue(bool v) : type(boolType), boolValue(v) {}
        VarValue(int v) : type(intType), intValue(v) {}
        VarValue(long long v) : type(llType), llValue(v) {}
        VarValue(float v) : type(floatType), floatValue(v) {}

        struct Proxy
        {
            const VarValue* obj;

            Proxy(const VarValue& Obj) : obj(&Obj) {}

            operator bool() const;
            operator int() const;
            operator long long() const;
            operator float() const;
        };

        Proxy get() const { return Proxy(*this); }

        bool getBool() const
        {
            if (type == boolType) return boolValue;
            if (type == intType) return intValue != 0;
            if (type == llType) return llValue != 0;
            if (type == floatType) return floatValue != 0.0f;
            ERROR("Invalid type conversion to bool");
        }
        int getInt() const
        {
            if (type == intType) return intValue;
            if (type == boolType) return static_cast<int>(boolValue);
            if (type == llType) return static_cast<int>(llValue);
            if (type == floatType) return static_cast<int>(floatValue);
            ERROR("Invalid type conversion to int");
        }
        long long getLL() const
        {
            if (type == intType) return static_cast<long long>(intValue);
            if (type == boolType) return static_cast<long long>(boolValue);
            if (type == llType) return llValue;
            if (type == floatType) return static_cast<long long>(floatValue);
            ERROR("Invalid type conversion to long long");
        }
        float getFloat() const
        {
            if (type == intType) return static_cast<float>(intValue);
            if (type == boolType) return static_cast<float>(boolValue);
            if (type == llType) return static_cast<float>(llValue);
            if (type == floatType) return floatValue;
            ERROR("Invalid type conversion to float");
        }
    };

    struct ExprValue
    {
        VarValue value;
        bool     isConstexpr;  // 该成员用于表示该表达式的值是否为编译期可以确定的常量

        ExprValue() : value(), isConstexpr(false) {}
        ExprValue(const VarValue& v, bool isConst = false) : value(v), isConstexpr(isConst) {}
        ExprValue(int v, bool isConst = false) : value(v), isConstexpr(isConst) {}
        ExprValue(long long v, bool isConst = false) : value(v), isConstexpr(isConst) {}
        ExprValue(float v, bool isConst = false) : value(v), isConstexpr(isConst) {}

        VarValue::Proxy get() const { return value.get(); }
        bool            getBool() const { return value.getBool(); }
        int             getInt() const { return value.getInt(); }
        long long       getLL() const { return value.getLL(); }
        float           getFloat() const { return value.getFloat(); }
    };

    struct VarAttr
    {
        bool  isConstDecl;
        Type* type;
        int   scopeLevel;

        std::vector<int>      arrayDims;
        std::vector<VarValue> initList;

        VarAttr() : isConstDecl(false), type(voidType), scopeLevel(-1), arrayDims(), initList() {}
        VarAttr(Type* t, bool isConst = false, int level = -1)
            : isConstDecl(isConst), type(t), scopeLevel(level), arrayDims(), initList()
        {}
    };

    struct NodeAttr
    {
        Operator  op;
        ExprValue val;

        NodeAttr(Operator o = Operator::UNK, const ExprValue& v = ExprValue()) : op(o), val(v) {}
    };
}  // namespace FE::AST

std::ostream& operator<<(std::ostream& os, FE::AST::Operator op);
std::string   toString(FE::AST::Operator op);

#endif  // __FRONTEND_AST_AST_DEFS_H__
