#include <frontend/ast/expr.h>

namespace FE::AST
{
    LeftValExpr::~LeftValExpr()
    {
        if (!indices) return;

        auto& idx_vec = *indices;
        for (auto idx : idx_vec)
        {
            if (!idx) continue;
            delete idx;
            idx = nullptr;
        }
        delete indices;
        indices = nullptr;
    }

    UnaryExpr::~UnaryExpr()
    {
        if (!expr) return;

        delete expr;
        expr = nullptr;
    }

    BinaryExpr::~BinaryExpr()
    {
        if (lhs)
        {
            delete lhs;
            lhs = nullptr;
        }
        if (rhs)
        {
            delete rhs;
            rhs = nullptr;
        }
    }

    CallExpr::~CallExpr()
    {
        if (!args) return;

        for (auto arg : *args)
        {
            if (!arg) continue;
            delete arg;
            arg = nullptr;
        }
        delete args;
        args = nullptr;
    }

    CommaExpr::~CommaExpr()
    {
        if (!exprs) return;

        auto& expr_vec = *exprs;
        for (auto expr : expr_vec)
        {
            if (!expr) continue;

            delete expr;
            expr = nullptr;
        }
        delete exprs;
        exprs = nullptr;
    }
}  // namespace FE::AST
