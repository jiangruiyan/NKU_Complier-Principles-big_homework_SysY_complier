#include "ast_printer.h"
#include <frontend/symbol/symbol_entry.h>

namespace FE::AST
{
    void ASTPrinter::visit(Initializer& node, std::ostream* os)
    {

        emitHeader(*os, "Initializer");
        if (!node.init_val) return;
        withChild(true, [&]() { apply(*this, *node.init_val, os); });
    }

    void ASTPrinter::visit(InitializerList& node, std::ostream* os)
    {

        emitHeader(*os, "InitializerList");
        if (!node.init_list) return;
        size_t cnt = node.init_list->size();
        for (size_t i = 0; i < cnt; ++i)
        {
            auto* init = (*node.init_list)[i];
            if (!init) continue;
            withChild(i + 1 == cnt, [&]() { apply(*this, *init, os); });
        }
    }

    void ASTPrinter::visit(VarDeclarator& node, std::ostream* os)
    {

        emitHeader(*os, "VarDeclarator");
        // Var:
        if (node.lval)
        {
            withChild(node.init == nullptr, [&]() {
                emitHeader(*os, "Var: ");
                withChild(true, [&]() { apply(*this, *node.lval, os); });
            });
        }
        // Init:
        if (node.init)
        {
            withChild(true, [&]() {
                emitHeader(*os, "Init: ");
                withChild(true, [&]() { apply(*this, *node.init, os); });
            });
        }
    }

    void ASTPrinter::visit(ParamDeclarator& node, std::ostream* os)
    {

        std::string head = "ParamDeclarator: " + node.type->toString() + " " + node.entry->getName();
        emitHeader(*os, head);
        if (!node.dims) return;
        size_t cnt = node.dims->size();
        for (size_t i = 0; i < cnt; ++i)
        {
            auto* dim = (*node.dims)[i];
            if (!dim) continue;
            withChild(i + 1 == cnt, [&]() { apply(*this, *dim, os); });
        }
    }

    void ASTPrinter::visit(VarDeclaration& node, std::ostream* os)
    {

        std::string head = std::string("VarDeclaration, BaseType: ") + node.type->toString();
        emitHeader(*os, head);
        if (!node.decls) return;
        size_t cnt = node.decls->size();
        for (size_t i = 0; i < cnt; ++i)
        {
            auto* d = (*node.decls)[i];
            if (!d) continue;
            withChild(i + 1 == cnt, [&]() { apply(*this, *d, os); });
        }
    }
}  // namespace FE::AST
