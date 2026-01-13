#ifndef __FRONTEND_AST_VISITOR_PRINTER_AST_PRINTER_H__
#define __FRONTEND_AST_VISITOR_PRINTER_AST_PRINTER_H__

#include <frontend/ast/ast_visitor.h>
#include <frontend/ast/ast.h>
#include <frontend/ast/decl.h>
#include <frontend/ast/expr.h>
#include <frontend/ast/stmt.h>
#include <iostream>
#include <functional>
#include <vector>
#include <string>
/*
语法分析及之后的编译阶段的结果打印均采用访问者模式，如果你对此不熟悉，可先了解相关语法知识。
采用访问者模式有助于同学们更专注于编译器原理的实现，忽略打印这些琐碎的内容。
但是在后续阶段中，阅读打印过程或许也有助于同学们理解某阶段的实现思路。
*/
namespace FE::AST
{
    using Printer_t = Visitor_t<void, std::ostream*>;  // void return type, ostream pointer

    class ASTPrinter : public Printer_t
    {
      public:
        // Basic AST nodes
        void visit(Root& node, std::ostream* os) override;

        // Declaration nodes
        void visit(Initializer& node, std::ostream* os) override;
        void visit(InitializerList& node, std::ostream* os) override;
        void visit(VarDeclarator& node, std::ostream* os) override;
        void visit(ParamDeclarator& node, std::ostream* os) override;
        void visit(VarDeclaration& node, std::ostream* os) override;

        // Expression nodes
        void visit(LeftValExpr& node, std::ostream* os) override;
        void visit(LiteralExpr& node, std::ostream* os) override;
        void visit(UnaryExpr& node, std::ostream* os) override;
        void visit(BinaryExpr& node, std::ostream* os) override;
        void visit(CallExpr& node, std::ostream* os) override;
        void visit(CommaExpr& node, std::ostream* os) override;

        // Statement nodes
        void visit(ExprStmt& node, std::ostream* os) override;
        void visit(FuncDeclStmt& node, std::ostream* os) override;
        void visit(VarDeclStmt& node, std::ostream* os) override;
        void visit(BlockStmt& node, std::ostream* os) override;
        void visit(ReturnStmt& node, std::ostream* os) override;
        void visit(WhileStmt& node, std::ostream* os) override;
        void visit(IfStmt& node, std::ostream* os) override;
        void visit(BreakStmt& node, std::ostream* os) override;
        void visit(ContinueStmt& node, std::ostream* os) override;
        void visit(ForStmt& node, std::ostream* os) override;

      private:
        void              emitPrefix(std::ostream& os) const;
        void              emitHeader(std::ostream& os, const std::string& text) const;
        void              pushLast(bool isLast);
        void              popLast();
        void              withChild(bool isLast, const std::function<void()>& fn);
        std::vector<bool> lastStack;
    };
}  // namespace FE::AST

#endif  // __FRONTEND_AST_VISITOR_PRINTER_AST_PRINTER_H__
