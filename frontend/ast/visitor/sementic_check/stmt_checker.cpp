#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <debug.h>

namespace FE::AST
{
    bool ASTChecker::visit(ExprStmt& node)
    {
        // 示例实现：表达式语句的语义检查
        // 空表达式直接通过，否则访问内部表达式
        if (!node.expr) return true;
        return apply(*this, *node.expr);
    }

    bool ASTChecker::visit(FuncDeclStmt& node)
    {
        // 实现函数声明的语义检查
        // 进入函数作用域，处理参数，检查函数体，退出作用域
        
        if (!node.entry)//判断函数是否有名字
        {
            errors.push_back("Function declaration with no name at line " + std::to_string(node.line_num));
            return false;
        }

        bool res = true;

        // 上下文保存
        auto prevRetType = curFuncRetType;
        curFuncRetType = node.retType;

        // 添加：保存并重置 funcHasReturn，用于检测函数体内是否存在 return
        // bool prevFuncHasReturn = funcHasReturn;
        funcHasReturn = false;

        // 进入函数作用域
        symTable.enterScope();

        //将函数形参加入符号表中
        if (node.params)
        {
            for (auto* param : *node.params)
            {
                if (!param) continue;
                res &= apply(*this, *param);
            }
        }

        // 若该函数有函数体，则继续递归检查里面的所有语句
        if (node.body)
        {
            res &= apply(*this, *node.body);
        }

        // 退出函数作用域
        symTable.exitScope();

        // 添加：如果函数返回类型不是 void，则必须至少有一个 return
        if (curFuncRetType && curFuncRetType->getBaseType() != Type_t::VOID)
        {
            if (!funcHasReturn)
            {
                // 允许 special case：int main() 可以没有 return
                if (node.entry && std::string(node.entry->getName()) == "main")
                {
                    // main 特例：不报错
                }
                else
                {
                    errors.push_back("Function '" + std::string(node.entry->getName()) +
                                     "' has no return statement but non-void return type at line " +
                                     std::to_string(node.line_num));
                    res = false;
                }
            }
        }

        // 恢复函数上下文
        curFuncRetType = prevRetType;
        curFuncRetType = prevRetType; //添加

        return res;
    }

    bool ASTChecker::visit(VarDeclStmt& node)
    {
        // 实现变量声明语句的语义检查
        // 委托给 VarDeclaration 访问器处理
        if (!node.decl) return true;
        return apply(*this, *node.decl);
    }

    bool ASTChecker::visit(BlockStmt& node)
    {
        // 实现块语句的语义检查
        // 进入新作用域，逐条访问语句，退出作用域
        bool res = true;

        // 进入新的作用域
        symTable.enterScope();

        // 访问所有的语句块
        if (node.stmts)
        {
            for (auto* stmt : *node.stmts)
            {
                if (!stmt) continue;
                res &= apply(*this, *stmt);
            }
        }

        // Exit block scope
        symTable.exitScope();

        return res;
    }

    bool ASTChecker::visit(ReturnStmt& node)
    {
        // 实现返回语句的语义检查
        // 检查返回值类型是否匹配当前函数的返回类型
        bool res = true;

        // 添加：标记当前函数包含 return
        funcHasReturn = true;

        // 如果有返回表达式，先检查返回表达式本身
        if (node.retExpr)
        {
            res &= apply(*this, *node.retExpr);

            //检查返回值与函数返回类型之间的类型兼容性
            Type* retValType = node.retExpr->attr.val.value.type;
            if (!retValType)
            {
                errors.push_back("Return expression has no type at line " + std::to_string(node.line_num));
                res = false;
            }
            else if (curFuncRetType)
            {
                Type_t funcRetBase = curFuncRetType->getBaseType();
                Type_t exprRetBase = retValType->getBaseType();

                // 允许数值类型之间的隐式转换（C语言标准行为）
                auto isNumeric = [](Type_t t) {
                    return t == Type_t::INT || t == Type_t::LL || 
                           t == Type_t::FLOAT || t == Type_t::BOOL;
                };
                
                bool typeOk = false;
                if (funcRetBase == exprRetBase) typeOk = true;
                else if (isNumeric(funcRetBase) && isNumeric(exprRetBase)) typeOk = true;

                if (!typeOk)
                {
                    errors.push_back("Return type mismatch at line " + std::to_string(node.line_num));
                    res = false;
                }
            }
        }
        else
        {
            //当前正在检查的return语句没有携带返回表达式，且当前函数期望的返回类型不是void
            if (curFuncRetType && curFuncRetType->getBaseType() != Type_t::VOID)
            {
                errors.push_back("Return without value in non-void function at line " + std::to_string(node.line_num));
                res = false;
            }
        }

        return res;
    }

    bool ASTChecker::visit(WhileStmt& node)
    {
        // 实现while循环的语义检查
        // 访问条件表达式，管理循环深度，访问循环体
        bool res = true;

        //若存在条件表达式，递归调用语义检查器去检查它
        if (node.cond)
        {
            res &= apply(*this, *node.cond);
        }

        // Increment loop depth for break/continue validation
        //把循环嵌套深度加一，表示现在处于一个额外的循环内部
        //这样BreakStmt::visit和ContinueStmt::visit可以通过检查loopDepth>0来判定break/continue是否在合法的循环内
        loopDepth++;

        //检查循环体
        //递归检查循环体并把结果合并到res
        if (node.body)
        {
            res &= apply(*this, *node.body);
        }

        // Decrement loop depth
        //离开循环体上下文
        //把循环深度恢复到进入前的值
        //保证嵌套循环时层级正确
        loopDepth--;

        return res;
    }

    bool ASTChecker::visit(IfStmt& node)
    {
        // 实现if语句的语义检查
        // 访问条件表达式，分别访问then和else分支
        bool res = true;

        // Visit condition expression and ensure it's a scalar type
        if (node.cond)
        {
            res &= apply(*this, *node.cond);

            // 检查条件类型：允许数值类型（int, ll, float, bool）作为条件
            // C语言语义：非零即真
            Type* condType = node.cond->attr.val.value.type;
            if (condType)
            {
                if (condType->getTypeGroup() == TypeGroup::BASIC)
                {
                    auto b = condType->getBaseType();
                    // 允许 int, ll, float, bool 作为条件
                    if (b != Type_t::INT && b != Type_t::LL && b != Type_t::BOOL && b != Type_t::FLOAT)
                    {
                        errors.push_back("condition in if statement must be numeric type at line " + std::to_string(node.line_num));
                        res = false;
                    }
                }
                else
                {
                    // 指针类型（或其它非 BASIC 类型）不作为条件（但是理论上是允许的？程序好像实际上也允许？？）
                    errors.push_back("condition in if statement must be numeric type at line " + std::to_string(node.line_num));
                    res = false;
                }
            }
        }

        if (node.thenStmt)
            res &= apply(*this, *node.thenStmt);

        if (node.elseStmt)
            res &= apply(*this, *node.elseStmt);

        return res;
    }

    bool ASTChecker::visit(BreakStmt& node)
    {
        // 实现break语句的语义检查
        // 检查是否在循环内使用
        //break没有类型，只有位置要求：只能在循环中出现
        //break上下文依赖检查
        if (loopDepth == 0)
        {
            errors.push_back("'break' statement not within a loop at line " + std::to_string(node.line_num));
            return false;
        }
        return true;
    }

    bool ASTChecker::visit(ContinueStmt& node)
    {
        // 实现continue语句的语义检查
        // 检查是否在循环内使用
        if (loopDepth == 0)
        {
            errors.push_back("'continue' statement not within a loop at line " + std::to_string(node.line_num));
            return false;
        }
        return true;
    }

    bool ASTChecker::visit(ForStmt& node) // for语句好像不用实现？
    {
        // 实现for循环的语义检查
        // 访问初始化、条件、步进表达式，管理循环深度
        bool res = true;

        // Visit initialization statement (if present)
        if (node.init)
        {
            res &= apply(*this, *node.init);
        }

        // Visit condition expression (if present)
        if (node.cond)
        {
            res &= apply(*this, *node.cond);
        }

        // Increment loop depth for break/continue validation
        loopDepth++;

        // Visit loop body
        if (node.body)
        {
            res &= apply(*this, *node.body);
        }

        // Decrement loop depth
        //loopDepth--;

        // Visit step expression (if present)
        if (node.step)
        {
            res &= apply(*this, *node.step);
        }
        
        
        loopDepth--;

        return res;
    }
}  // namespace FE::AST
