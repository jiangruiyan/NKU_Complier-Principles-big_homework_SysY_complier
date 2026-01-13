#ifndef __FRONTEND_AST_AST_H__
#define __FRONTEND_AST_AST_H__

#include <frontend/ast/ast_defs.h>
#include <frontend/ast/ast_visitor.h>
#include <frontend/symbol/symbol_entry.h>
#include <vector>

/*
如果需要，你可以在类中添加成员变量和成员函数，辅助你完成实验
*/

namespace FE::AST
{
  class StmtNode;
  class ExprNode;
  class DeclNode;
  class VarDeclaration;

  using Entry = FE::Sym::Entry;

  // AST的节点类
  class Node
  {
  public:
    int line_num;
    int col_num;
    NodeAttr attr; // 携带节点属性，是语法树标记的重点对象

    Node(int line_num = -1, int col_num = -1) : line_num(line_num), col_num(col_num), attr() {}
    virtual ~Node() = default;

    virtual void accept(Visitor &visitor) = 0;
  };

  // AST的根节点
  class Root : public Node
  {
  private:
    std::vector<StmtNode *> *stmts;

  public:
    Root(std::vector<StmtNode *> *stmts) : Node(-1, -1), stmts(stmts) {}
    virtual ~Root() override;

    virtual void accept(Visitor &visitor) override { visitor.visit(*this); }

    std::vector<StmtNode *> *getStmts() const { return stmts; }
  };
} // namespace FE::AST

#endif // __FRONTEND_AST_AST_H__
