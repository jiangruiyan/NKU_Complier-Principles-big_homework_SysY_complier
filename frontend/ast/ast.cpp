#include <frontend/ast/ast.h>
#include <frontend/ast/stmt.h>

namespace FE::AST
{
    Root::~Root()
    {
        if (!stmts) return;

        for (auto stmt : *stmts)
        {
            if (!stmt) continue;

            delete stmt;
            stmt = nullptr;
        }
        delete stmts;
        stmts = nullptr;
    }
}  // namespace FE::AST
