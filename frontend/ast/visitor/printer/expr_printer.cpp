#include "ast_printer.h"
#include <frontend/symbol/symbol_entry.h>

namespace FE::AST
{
    void ASTPrinter::visit(LeftValExpr& node, std::ostream* os)
    {
        emitHeader(*os, std::string("LeftValueExpr ") + node.entry->getName());
        if (!node.indices) return;
        size_t cnt = node.indices->size();
        for (size_t i = 0; i < cnt; ++i)
        {
            auto* index = (*node.indices)[i];
            if (!index) continue;
            withChild(i + 1 == cnt, [&]() { apply(*this, *index, os); });
        }
    }

    void ASTPrinter::visit(LiteralExpr& node, std::ostream* os)
    {
        std::string text = "literal ";
        switch (node.literal.type->getBaseType())
        {
            case Type_t::INT: text += "int: " + std::to_string(static_cast<int>(node.literal.get())); break;
            case Type_t::LL: text += "long long: " + std::to_string(static_cast<long long>(node.literal.get())); break;
            case Type_t::FLOAT: text += "float: " + std::to_string(static_cast<float>(node.literal.get())); break;
            default: text += "Undefined"; break;
        }
        emitHeader(*os, text);
    }

    void ASTPrinter::visit(UnaryExpr& node, std::ostream* os)
    {
        emitHeader(*os, std::string("UnaryExpr ") + toString(node.op));
        if (!node.expr) return;
        withChild(true, [&]() { apply(*this, *node.expr, os); });
    }

    void ASTPrinter::visit(BinaryExpr& node, std::ostream* os)
    {
        emitHeader(*os, std::string("BinaryExpr ") + toString(node.op));
        if (node.lhs) withChild(false, [&]() { apply(*this, *node.lhs, os); });
        if (node.rhs) withChild(true, [&]() { apply(*this, *node.rhs, os); });
    }

    void ASTPrinter::visit(CallExpr& node, std::ostream* os)
    {
        emitHeader(*os, std::string("Call ") + node.func->getName());
        if (!node.args) return;
        size_t cnt = node.args->size();
        for (size_t i = 0; i < cnt; ++i)
        {
            auto* arg = (*node.args)[i];
            if (!arg) continue;
            withChild(i + 1 == cnt, [&]() {
                std::string argHead = std::string("Arg ") + std::to_string(i) + ": ";
                emitHeader(*os, argHead);
                withChild(true, [&]() { apply(*this, *arg, os); });
            });
        }
    }

    void ASTPrinter::visit(CommaExpr& node, std::ostream* os)
    {
        emitHeader(*os, "ExprList");
        if (!node.exprs) return;
        size_t cnt = node.exprs->size();
        for (size_t i = 0; i < cnt; ++i)
        {
            auto* e = (*node.exprs)[i];
            if (!e) continue;
            withChild(i + 1 == cnt, [&]() { apply(*this, *e, os); });
        }
    }
}  // namespace FE::AST
