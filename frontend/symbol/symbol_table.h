#ifndef __FRONTEND_SYMBOL_SYMBOL_TABLE_H__
#define __FRONTEND_SYMBOL_SYMBOL_TABLE_H__

#include <frontend/symbol/isymbol_table.h>
#include <frontend/ast/ast_defs.h>
#include <map>
#include <vector>

namespace FE::Sym
{
    class SymTable : public iSymTable<SymTable>
    {
        friend iSymTable<SymTable>;

        // 作用域链：每个元素是当前作用域的符号表（Entry* -> VarAttr）
        // 使用 thread_local 保证线程安全
        thread_local static std::vector<std::map<Entry*, FE::AST::VarAttr>> scopeChain;

        void reset_impl();

        void              addSymbol_impl(Entry* entry, FE::AST::VarAttr& attr);
        FE::AST::VarAttr* getSymbol_impl(Entry* entry);
        void              enterScope_impl();
        void              exitScope_impl();

        bool isGlobalScope_impl();
        int  getScopeDepth_impl();
    };
}  // namespace FE::Sym

#endif  // __FRONTEND_SYMBOL_SYMBOL_TABLE_H__
