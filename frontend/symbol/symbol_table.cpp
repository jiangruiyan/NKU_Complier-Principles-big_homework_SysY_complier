#include <frontend/symbol/symbol_table.h>
#include <debug.h>

namespace FE::Sym
{
    // 初始化 thread_local 静态成员变量
    thread_local std::vector<std::map<Entry*, FE::AST::VarAttr>> SymTable::scopeChain;

    void SymTable::reset_impl()
    {
        //TODO("Lab3-1: Reset symbol table");
        //重置符号表状态
        // 清空整个作用域链，回到初始状态
        scopeChain.clear();
        // 进入全局作用域
        enterScope_impl();
    }

    void SymTable::enterScope_impl()
    {
        //TODO("Lab3-1: Enter new scope");
        // 创建新的作用域（新的符号表）并压入作用域栈
        scopeChain.push_back(std::map<Entry*, FE::AST::VarAttr>());
    }

    void SymTable::exitScope_impl()
    {
        //TODO("Lab3-1: Exit current scope");
        // 从作用域栈中移除最内层的作用域
        // 注意：不应该移除全局作用域，除非显式调用 reset
        //防止将全局作用域移除
        if (scopeChain.size() > 1)
        {
            scopeChain.pop_back();
        }
    }

    void SymTable::addSymbol_impl(Entry* entry, FE::AST::VarAttr& attr)
    {
        //TODO("Lab3-1: Add symbol to current scope");
        // 将符号添加到当前（最内层）作用域中
        if (scopeChain.empty()) enterScope_impl();
        auto& cur = scopeChain.back();
        if (cur.count(entry))
        {
            ERROR("redefinition");
            return;
        }
        cur[entry] = attr;
        /*
        if (!scopeChain.empty())
        {
            scopeChain.back()[entry] = attr;
        }
            */
    }

    FE::AST::VarAttr* SymTable::getSymbol_impl(Entry* entry)
    {
        //TODO("Lab3-1: Get symbol from symbol table");
        // 从内到外搜索作用域链，找到符号则返回其指针
        // 这实现了作用域的遮蔽规则（inner scope hides outer scope）
        if (scopeChain.empty()) return nullptr;
        for (auto it = scopeChain.rbegin(); it != scopeChain.rend(); ++it)
        {
            auto found = it->find(entry);
            if (found != it->end())
            {
                return &found->second;
            }
        }
        // 未找到
        return nullptr;
    }

    bool SymTable::isGlobalScope_impl()
    {
        //TODO("Lab3-1: Check if current scope is global scope");
        // 当作用域深度为 1 时，即处于全局作用域
        return scopeChain.size() == 1;
    }

    int SymTable::getScopeDepth_impl()
    {
        //TODO("Lab3-1: Get current scope depth");
        // 返回当前作用域深度（1 表示全局，2 表示局部，依此类推）
        return static_cast<int>(scopeChain.size());
    }
}  // namespace FE::Sym
