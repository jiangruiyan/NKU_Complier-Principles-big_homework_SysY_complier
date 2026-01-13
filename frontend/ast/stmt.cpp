#include <frontend/ast/stmt.h>
#include <frontend/ast/expr.h>
#include <frontend/ast/decl.h>

namespace FE::AST
{
    ExprStmt::~ExprStmt()
    {
        if (!expr) return;
        delete expr;
        expr = nullptr;
    }

    FuncDeclStmt::~FuncDeclStmt()
    {
        if (params)
        {
            auto& param_vec = *params;
            for (auto param : param_vec)
            {
                if (!param) continue;

                delete param;
                param = nullptr;
            }
            delete params;
            params = nullptr;
        }

        if (body)
        {
            delete body;
            body = nullptr;
        }
    }

    VarDeclStmt::~VarDeclStmt()
    {
        if (decl)
        {
            delete decl;
            decl = nullptr;
        }
    }

    BlockStmt::~BlockStmt()
    {
        if (!stmts) return;

        auto& stmt_vec = *stmts;
        for (auto stmt : stmt_vec)
        {
            if (!stmt) continue;

            delete stmt;
            stmt = nullptr;
        }
        delete stmts;
        stmts = nullptr;
    }

    ReturnStmt::~ReturnStmt()
    {
        if (!retExpr) return;

        delete retExpr;
        retExpr = nullptr;
    }

    WhileStmt::~WhileStmt()
    {
        if (cond)
        {
            delete cond;
            cond = nullptr;
        }
        if (body)
        {
            delete body;
            body = nullptr;
        }
    }

    IfStmt::~IfStmt()
    {
        if (cond)
        {
            delete cond;
            cond = nullptr;
        }
        if (thenStmt)
        {
            delete thenStmt;
            thenStmt = nullptr;
        }
        if (elseStmt)
        {
            delete elseStmt;
            elseStmt = nullptr;
        }
    }

    ForStmt::~ForStmt()
    {
        if (init)
        {
            delete init;
            init = nullptr;
        }
        if (cond)
        {
            delete cond;
            cond = nullptr;
        }
        if (step)
        {
            delete step;
            step = nullptr;
        }
        if (body)
        {
            delete body;
            body = nullptr;
        }
    }
}  // namespace FE::AST
