#ifndef __FRONTEND_SYMBOL_ISYMBOL_TABLE_H__
#define __FRONTEND_SYMBOL_ISYMBOL_TABLE_H__

#include <frontend/ast/ast_defs.h>
#include <frontend/symbol/symbol_entry.h>

namespace FE::Sym
{
    template <typename Derived>
    class iSymTable
    {
      public:
        iSymTable()          = default;
        virtual ~iSymTable() = default;

      public:
        void reset() { static_cast<Derived*>(this)->reset_impl(); }

        void addSymbol(Entry* entry, AST::VarAttr& attr) { static_cast<Derived*>(this)->addSymbol_impl(entry, attr); }
        AST::VarAttr* getSymbol(Entry* entry) { return static_cast<Derived*>(this)->getSymbol_impl(entry); }

        void enterScope() { static_cast<Derived*>(this)->enterScope_impl(); }
        void exitScope() { static_cast<Derived*>(this)->exitScope_impl(); }

        bool isGlobalScope() { return static_cast<Derived*>(this)->isGlobalScope_impl(); }
        int  getScopeDepth() { return static_cast<Derived*>(this)->getScopeDepth_impl(); }
    };
}  // namespace FE::Sym

#endif  // __FRONTEND_SYMBOL_ISYMBOL_TABLE_H__
