#include <frontend/ast/decl.h>
#include <frontend/ast/expr.h>

namespace FE::AST
{
    Initializer::~Initializer()
    {
        if (!init_val) return;
        delete init_val;
        init_val = nullptr;
    }

    InitializerList::~InitializerList()
    {
        if (!init_list) return;

        auto& init_vec = *init_list;
        for (auto init : init_vec)
        {
            if (!init) continue;
            delete init;
            init = nullptr;
        }
        delete init_list;
        init_list = nullptr;
    }
    size_t InitializerList::size()
    {
        if (!init_list) return 0;
        return init_list->size();
    }

    VarDeclarator::~VarDeclarator()
    {
        if (lval)
        {
            delete lval;
            lval = nullptr;
        }
        if (init)
        {
            delete init;
            init = nullptr;
        }
    }

    ParamDeclarator::~ParamDeclarator()
    {
        if (!dims) return;

        auto& dim_vec = *dims;
        for (auto dim : dim_vec)
        {
            if (!dim) continue;
            delete dim;
            dim = nullptr;
        }
        delete dims;
        dims = nullptr;
    }

    VarDeclaration::~VarDeclaration()
    {
        if (!decls) return;
        auto& ds = *decls;
        for (auto d : ds)
        {
            if (!d) continue;
            delete d;
            d = nullptr;
        }
        delete decls;
        decls = nullptr;
    }
}  // namespace FE::AST
