#ifndef __FRONTEND_AST_VISITOR_H__
#define __FRONTEND_AST_VISITOR_H__

#include <ivisitor.h>

namespace FE::AST
{
    class Root;
    using BasicTypeSet = TypeList<Root>;

    class Initializer;
    class InitializerList;
    class VarDeclarator;
    class ParamDeclarator;
    class VarDeclaration;
    using DeclTypeSet = TypeList<Initializer, InitializerList, VarDeclarator, ParamDeclarator, VarDeclaration>;

    class LeftValExpr;
    class LiteralExpr;
    class UnaryExpr;
    class BinaryExpr;
    class CallExpr;
    class CommaExpr;
    using ExprTypeSet = TypeList<LeftValExpr, LiteralExpr, UnaryExpr, BinaryExpr, CallExpr, CommaExpr>;

    class ExprStmt;
    class FuncDeclStmt;
    class VarDeclStmt;
    class BlockStmt;
    class ReturnStmt;
    class WhileStmt;
    class IfStmt;
    class BreakStmt;
    class ContinueStmt;
    class ForStmt;
    using StmtTypeSet = TypeList<ExprStmt, FuncDeclStmt, VarDeclStmt, BlockStmt, ReturnStmt, WhileStmt, IfStmt,
        BreakStmt, ContinueStmt, ForStmt>;

    using TypeSet0 = type_list_utils::Concat_t<BasicTypeSet, DeclTypeSet>;
    using TypeSet1 = type_list_utils::Concat_t<TypeSet0, ExprTypeSet>;
    using TypeSet  = type_list_utils::Concat_t<TypeSet1, StmtTypeSet>;

    template <typename... Ts>
    using Visitor_t = VisitSetFrom<TypeSet>::Visitor<Ts...>;
    using Visitor   = VisitSetFrom<TypeSet>::ErasedVisitor;
}  // namespace FE::AST

#endif  // __FRONTEND_AST_VISITOR_H__
