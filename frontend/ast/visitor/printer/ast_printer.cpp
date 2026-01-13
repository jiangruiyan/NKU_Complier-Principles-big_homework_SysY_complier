#include "ast_printer.h"
#include <frontend/symbol/symbol_entry.h>

namespace FE::AST
{
    void ASTPrinter::emitPrefix(std::ostream& os) const
    {
        if (lastStack.empty()) return;
        for (size_t i = 0; i + 1 < lastStack.size(); ++i) { os << (lastStack[i] ? "    " : "|   "); }
        os << (lastStack.back() ? "`-- " : "|-- ");
    }

    void ASTPrinter::emitHeader(std::ostream& os, const std::string& text) const
    {
        emitPrefix(os);
        os << text << "\n";
    }

    void ASTPrinter::pushLast(bool isLast) { lastStack.push_back(isLast); }
    void ASTPrinter::popLast()
    {
        if (!lastStack.empty()) lastStack.pop_back();
    }

    void ASTPrinter::withChild(bool isLast, const std::function<void()>& fn)
    {
        pushLast(isLast);
        fn();
        popLast();
    }

    void ASTPrinter::visit(Root& node, std::ostream* os)
    {

        lastStack.clear();
        *os << "ASTree\n";

        auto* stmts = node.getStmts();
        if (!stmts) return;

        size_t cnt = stmts->size();
        for (size_t i = 0; i < cnt; ++i)
        {
            if (!(*stmts)[i]) continue;
            withChild(i + 1 == cnt, [&]() { apply(*this, *(*stmts)[i], os); });
        }
    }
}  // namespace FE::AST
