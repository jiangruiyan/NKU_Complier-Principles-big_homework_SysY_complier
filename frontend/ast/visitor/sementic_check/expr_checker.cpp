#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <debug.h>

namespace FE::AST
{
    bool ASTChecker::visit(LeftValExpr& node)
    {
        // TODO(Lab3-1): 实现左值表达式的语义检查
        // 检查变量是否存在，处理数组下标访问，进行类型检查和常量折叠
        bool res = true;

        if (!node.entry)
        {
            errors.push_back("Undefined identifier at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            node.isLval = false;
            return false;
        }

        // 查找符号表中的变量属性（优先本地，再全局）
        FE::AST::VarAttr* varAttr = symTable.getSymbol(node.entry);
        if (!varAttr)
        {
            auto it = glbSymbols.find(node.entry);
            if (it != glbSymbols.end()) varAttr = &it->second;
        }

        if (!varAttr)
        {
            errors.push_back(std::string("Undefined variable '") + node.entry->getName() + "' at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            node.isLval = false;
            return false;
        }

        // 初始类型
        Type* curType = varAttr->type;

        // 处理数组下标：每个下标都要进行类型检查（索引应为整数）
        if (node.indices)
        {
            for (auto* idx : *node.indices)
            {
                if (!idx) continue;
                //先对下标表达式做语义检查
                res &= apply(*this, *idx);

                //如果类型推导失败，则直接报错返回
                if (!idx->attr.val.value.type)
                {
                    node.attr.val.value.type = voidType;
                    node.isLval = false;
                    return false;
                }


                //把数组下标表达式的最低层真实类型取出进行检查
                auto base = idx->attr.val.value.type->getBaseType();
                if (base != Type_t::INT && base != Type_t::LL && base != Type_t::BOOL)
                {
                    errors.push_back("Array index must be an integer at line " + std::to_string(idx->line_num));
                    res = false;
                }

                // 通过索引，类型应当向下剥离指针类型，判断是否为指针/数组类型
                // 检查：当前类型是否为指针，或者变量有 arrayDims（是数组变量）
                if (curType && curType->getTypeGroup() == TypeGroup::POINTER)
                {
                    auto ptr = static_cast<PtrType*>(curType);
                    curType    = ptr->base;
                }
                else if (!varAttr->arrayDims.empty())
                {
                    // 对于声明为数组的变量（有 arrayDims），即使 type 不是 POINTER
                    // 也允许索引访问，索引后类型保持为基础类型
                    // (这种情况主要针对局部数组变量)
                    // curType 保持不变
                }
                else
                {
                    // 索引作用于非指针/数组类型
                    errors.push_back("Subscripted value is not an array or pointer at line " + std::to_string(idx->line_num));
                    curType = voidType;
                    res     = false;
                }
            }
        }

        // 将最终推断的类型写入属性
        node.attr.val.value.type = curType ? curType : voidType;

        // left value 是否可被赋值
        // 若含下标（arr[i]），则为可赋值；若不含下标且原变量为数组（有arrayDims），则不可赋值
        if (node.indices && !node.indices->empty()) node.isLval = true;
        else node.isLval = varAttr->arrayDims.empty();

        // 常量折叠 (若变量为 const 且为编译期常量初始化，则视为常量)
        if (varAttr->isConstDecl && !node.indices && !varAttr->initList.empty())
        {
            // 对于常量变量，复制其初始化值作为常量表达式的值
            node.attr.val.isConstexpr = true;
            node.attr.val.value = varAttr->initList[0];  // 标量常量的值
        }
        else
        {
            node.attr.val.isConstexpr = false;
        }

        return res;
    }

    bool ASTChecker::visit(LiteralExpr& node)
    {
        // 示例实现：字面量表达式的语义检查
        // 字面量总是编译期常量，直接设置属性
        node.attr.val.isConstexpr = true;
        node.attr.val.value       = node.literal;
        return true;
    }

    bool ASTChecker::visit(UnaryExpr& node)
    {
        // TODO(Lab3-1): 实现一元表达式的语义检查
        // 访问子表达式，检查操作数类型，调用类型推断函数
        // 访问子表达式
        if (!node.expr)
        {
            errors.push_back("Unary operator missing operand at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }

        bool res = apply(*this, *node.expr);

        // 记录操作符
        node.attr.op = node.op;

        // 进行类型推断与可能的常量折叠
        bool hasError = false;
        ExprValue val = typeInfer(node.expr->attr.val, node.op, node, hasError);
        node.attr.val = val;

        // 一元表达式通常不是左值：UnaryExpr 没有 isLval 成员，属性通过 node.attr 反映
        //node.attr.isLval = false;

        if (hasError) res = false;
        return res;
    }

    bool ASTChecker::visit(BinaryExpr& node)
    {
        // TODO(Lab3-1): 实现二元表达式的语义检查
        // 访问左右子表达式，检查操作数类型，调用类型推断
        bool res = true;

        if (!node.lhs || !node.rhs)
        {
            errors.push_back("Binary expression missing operand at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }

        // 访问左右子表达式
        res &= apply(*this, *node.lhs);
        res &= apply(*this, *node.rhs);

        // 如果是赋值，要检查左值是否合法
        if (node.op == Operator::ASSIGN)
        {
            auto* lval = dynamic_cast<LeftValExpr*>(node.lhs);
            if (!lval || !lval->isLval)
            {
                errors.push_back("Left operand of assignment must be an lvalue at line " + std::to_string(node.line_num));
                res = false;
            }
            else
            {
                // 获取 varAttr 以检查 const / array assignment
                FE::AST::VarAttr* varAttr = symTable.getSymbol(lval->entry);
                if (!varAttr)
                {
                    auto it = glbSymbols.find(lval->entry);
                    if (it != glbSymbols.end()) varAttr = &it->second;
                }

                if (varAttr)
                {
                    if (varAttr->isConstDecl)
                    {
                        errors.push_back(std::string("Cannot assign to const variable '") + lval->entry->getName() + "' at line " + std::to_string(node.line_num));
                        res = false;
                    }

                    // 不允许将整个数组赋值（a = b）——只能赋值数组元素
                    if ((!lval->indices || lval->indices->empty()) && !varAttr->arrayDims.empty())
                    {
                        errors.push_back(std::string("Cannot assign to array '") + lval->entry->getName() + "' at line " + std::to_string(node.line_num));
                        res = false;
                    }
                }
            }
        }

        // 添加：操作数类型合法性检查
        // 规则：
        // void 类型不能出现在任何表达式中（报错）
        // 对于非赋值操作（算术/比较/逻辑等），不允许指针参与运算（报错）
        // int/float/bool 互相允许转换；其它类型组合视为不匹配（报错）
        Type* ltype = node.lhs->attr.val.value.type;
        Type* rtype = node.rhs->attr.val.value.type;

        auto isNumericOrBool = [](Type* t) -> bool {
            if (!t) return false;
            auto b = t->getBaseType();
            return b == Type_t::INT || b == Type_t::LL || b == Type_t::FLOAT || b == Type_t::BOOL;
        };

        // void 出现即报错
        if ((ltype && ltype->getBaseType() == Type_t::VOID) || (rtype && rtype->getBaseType() == Type_t::VOID))
        {
            errors.push_back("Void type cannot appear in expression at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }

        // 非赋值运算中不允许指针参与
        if (node.op != Operator::ASSIGN)
        {
            if ((ltype && ltype->getTypeGroup() == TypeGroup::POINTER) ||
                (rtype && rtype->getTypeGroup() == TypeGroup::POINTER))
            {
                errors.push_back("Pointer type cannot participate in this binary operation at line " +
                                 std::to_string(node.line_num));
                node.attr.val.value.type = voidType;
                return false;
            }
        }

        // 对于非指针非void类型，检查是否为可互转的数值/布尔类型
        if (!isNumericOrBool(ltype) || !isNumericOrBool(rtype))
        {
            // 如果两边都不是数字/布尔类型，则视为不匹配
            // （指针/void 已在上面被排除）
            errors.push_back("Type mismatch in binary expression at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }
        

        // 类型推断与常量折叠
        node.attr.op = node.op;
        bool hasError = false;
        ExprValue val = typeInfer(node.lhs->attr.val, node.rhs->attr.val, node.op, node, hasError);
        node.attr.val = val;

        if (hasError || node.attr.val.value.type == voidType) res = false;
        return res;
    }

    bool ASTChecker::visit(CallExpr& node)
    {
        // TODO(Lab3-1): 实现函数调用表达式的语义检查
        // 检查函数是否存在，访问实参列表，检查参数数量和类型匹配
        bool res = true;

        //检查函数是否存在
        //检查AST节点结构完整性，
        //检查当前函数调用表达式节点node本身是否已经成功解析出了一个指向函数信息的指针
        //如果出现错误，则意味着解析器在构建AST时出现问题，或者AST结构不完整
        if (!node.func)
        {
            errors.push_back("Call expression has no function at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }

        // Visit args
        std::vector<ExprNode*> argList;
        if (node.args)
        {
            for (auto* arg : *node.args)
            {
                if (!arg) continue;
                //让所有实参先完成语义检查和常量折叠
                res &= apply(*this, *arg);
                argList.push_back(arg);
            }
        }

        // 检查函数声明
        FE::Sym::Entry* funcEntry = node.func;
        // funcDecls是全局注册过的所有函数，包括库函数和用户定义函数
        // 检查符号表/声明的有效性
        // 未定义函数语义错误:函数名在语法上是存在的，但它从未被定义或声明。
        auto it = funcDecls.find(funcEntry);
        if (it == funcDecls.end())
        {
            errors.push_back(std::string("Undefined function '") + funcEntry->getName() + "' at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }

        FuncDeclStmt* fdecl = it->second;
        // 检查参数数量
        size_t pcount = fdecl->params ? fdecl->params->size() : 0;
        size_t acount = argList.size();
        if (pcount != acount)
        {
            errors.push_back("Function '" + funcEntry->getName() + "' called with wrong number of arguments at line " + std::to_string(node.line_num));
            res = false;
        }

        // 检查参数类型匹配（有问题，int*类型不能当做int类型，改！！）
        for (size_t i = 0; i < std::min(pcount, acount); ++i)
        {
            ParamDeclarator* p = (*(fdecl->params))[i]; // 函数参数声明
            ExprNode* a = argList[i]; // 实参表达式

            if (!p)
            {
                continue; // 跳过nullptr参数
            }

            Type* ptype = p->type;
            if (p->dims && ptype) // 参数为数组类型，转换为指针类型
                ptype = TypeFactory::getPtrType(ptype);

            Type* atype = a->attr.val.value.type;
            if (!atype)
            {
                errors.push_back("Argument has no type at line " + std::to_string(a->line_num));
                res = false;
                continue;
            }

            // 如果参数类型是指针类型
            if (ptype->getTypeGroup() == TypeGroup::POINTER)
            {
                bool ok = false;
                auto pBase = static_cast<PtrType*>(ptype)->base;
                
                // 若实参类型也是指针类型
                if (atype->getTypeGroup() == TypeGroup::POINTER) {
                    // 指针类型匹配：检查指向的基本类型是否相同
                    auto aBase = static_cast<PtrType*>(atype)->base;
                    if (aBase && pBase && aBase->getBaseType() == pBase->getBaseType()) ok = true;
                }
                // 若实参是数组变量（LeftValExpr）
                else if (auto* al = dynamic_cast<LeftValExpr*>(a))
                {
                    FE::AST::VarAttr* av = symTable.getSymbol(al->entry);
                    if (!av)
                    {
                        auto it2 = glbSymbols.find(al->entry);
                        if (it2 != glbSymbols.end()) av = &it2->second;
                    }
                    
                    if (av && !av->arrayDims.empty())
                    {
                        // 数组变量的情况：
                        // 1. 完整数组名（无索引）：int a[10] -> a 退化为 int*
                        // 2. 部分索引：int a[10][20] -> a[0] 退化为 int*
                        //    - 索引数量 < 维度数量时，仍然是数组类型，可以退化为指针
                        
                        size_t numIndices = (al->indices) ? al->indices->size() : 0;
                        size_t numDims = av->arrayDims.size();
                        
                        // 如果索引数量少于维度数量，剩余的维度可以退化为指针
                        if (numIndices < numDims)
                        {
                            // 检查剩余部分的基本类型是否匹配
                            // av->type 是数组元素的基本类型（如 int）
                            // pBase 是参数指针指向的类型（如 int* 的 base 是 int）
                            if (pBase && av->type->getBaseType() == pBase->getBaseType())
                                ok = true;
                        }
                    }
                }
                // 检查实参类型本身是否匹配（对于结果为数组的表达式）
                else if (pBase && atype->getBaseType() == pBase->getBaseType())
                {
                    // 允许基本类型匹配（例如标量到指针的隐式转换在某些情况下）
                    // 但这通常不正确，保守处理
                }
                
                if (!ok)
                {
                    errors.push_back("Type mismatch for parameter " + std::to_string(i) + " of function '" + funcEntry->getName() + "' at line " + std::to_string(node.line_num));
                    res = false;
                }
            }
            else
            {
                // 新增：若参数期望非指针，但实参是指针类型，不允许
                if (atype->getTypeGroup() == TypeGroup::POINTER)
                {
                    errors.push_back("Type mismatch for parameter " + std::to_string(i) + " of function '" +
                                     funcEntry->getName() + "' at line " + std::to_string(node.line_num));
                    res = false;
                    continue;
                }

                // 新增：若实参是数组变量（会退化为指针），也不允许传给非指针参数
                if (auto* al = dynamic_cast<LeftValExpr*>(a))
                {
                    FE::AST::VarAttr* av = symTable.getSymbol(al->entry);
                    if (!av)
                    {
                        auto it2 = glbSymbols.find(al->entry);
                        if (it2 != glbSymbols.end()) av = &it2->second;
                    }
                    if (av && !av->arrayDims.empty())
                    {
                        // 当传入完整数组名或不足维度索引时，会退化为指针 -> 不允许
                        size_t numIndices = (al->indices) ? al->indices->size() : 0;
                        size_t numDims = av->arrayDims.size();
                        if (numIndices < numDims)
                        {
                            errors.push_back("Type mismatch for parameter " + std::to_string(i) + " of function '" +
                                             funcEntry->getName() + "' at line " + std::to_string(node.line_num));
                            res = false;
                            continue;
                        }
                    }
                }

                // For non-pointer parameter, check type compatibility
                // 允许数值类型之间的隐式转换（C语言标准行为）
                auto argBase = atype->getBaseType();
                auto paramBase = ptype->getBaseType();
                
                auto isNumeric = [](Type_t t) {
                    return t == Type_t::INT || t == Type_t::LL || 
                           t == Type_t::FLOAT || t == Type_t::BOOL;
                };
                
                bool canConvert = false;
                if (paramBase == argBase) {
                    canConvert = true;
                } else if (isNumeric(paramBase) && isNumeric(argBase)) {
                    // 允许所有数值类型之间的转换
                    canConvert = true;
                } else {
                    canConvert = false;
                }
                
                if (!canConvert)
                {
                    errors.push_back("Type mismatch for parameter " + std::to_string(i) + " of function '" + funcEntry->getName() + "' at line " + std::to_string(node.line_num));
                    res = false;
                }
            }
        }

        // 返回值类型
        node.attr.val.value.type = fdecl->retType;
        node.attr.val.isConstexpr = false; // 函数调用结果通常不是编译期常量
        return res;
    }

    bool ASTChecker::visit(CommaExpr& node)
    {
        // 实现逗号表达式的语义检查
        // 依序访问各子表达式（从左到右），最终表达式的属性即为逗号表达式的属性
        bool res = true;

        if (!node.exprs || node.exprs->empty())
        {
            errors.push_back("Comma expression requires at least one subexpression at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }

        ExprNode* last = nullptr;
        for (auto* e : *node.exprs)
        {
            if (!e) continue;
            // 依次访问每个子表达式
            res &= apply(*this, *e);
            last = e;
        }

        if (!last)
        {
            errors.push_back("Comma expression contains no valid subexpressions at line " + std::to_string(node.line_num));
            node.attr.val.value.type = voidType;
            return false;
        }

        // 值与常量性取决于最后一个子表达式
        node.attr.val = last->attr.val;
        // 逗号表达式的结果不是左值（即使最后一个是 LeftValExpr，也转为右值）
        // 因为 ExprNode 没有 isLval 成员，只有 LeftValExpr 才有 isLval，我们无需修改那里
        if (!node.attr.val.value.type)
        {
            res = false;
        }
        return res;
    }
}  // namespace FE::AST
