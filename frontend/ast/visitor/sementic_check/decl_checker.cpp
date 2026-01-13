#include <frontend/ast/visitor/sementic_check/ast_checker.h>
#include <debug.h>
#include <functional>

namespace FE::AST
{
    bool ASTChecker::visit(Initializer& node)
    {
        // 示例实现：单个初始化器的语义检查
        // 1) 访问初始化值表达式
        // 2) 将子表达式的属性拷贝到当前节点
        ASSERT(node.init_val && "Null initializer value");
        bool res  = apply(*this, *node.init_val);
        node.attr = node.init_val->attr;
        return res;
    }

    bool ASTChecker::visit(InitializerList& node)
    {
        // 示例实现：初始化器列表的语义检查
        // 遍历列表中的每个初始化器并逐个访问
        if (!node.init_list) return true;
        bool res = true;
        for (auto* init : *(node.init_list))
        {
            if (!init) continue;
            res &= apply(*this, *init);
        }
        return res;
    }

    bool ASTChecker::visit(VarDeclarator& node)
    {
        // TODO(Lab3-1): 实现变量声明器的语义检查
        // 访问左值表达式，同步属性，处理初始化器（如果有）
        // 左值必须存在并且为identifier/left value（但此处不应查symbol table）

        // 断言与存在性检查
        if (!node.lval)
        {
            errors.push_back("Invalid variable declarator at line " + std::to_string(node.line_num));
            return false;
        }

        auto* lval = dynamic_cast<LeftValExpr*>(node.lval);
        if (!lval)
        {
            errors.push_back("Invalid variable declarator (not a left value) at line " + std::to_string(node.line_num));
            return false;
        }

        bool res = true;

        // 如果声明是数组（左值中包含indices），检查维度表达式必须为正整数常量
        if (lval->indices)
        {
            for (auto* idx : *lval->indices)
            {
                if (!idx) continue;
                // 先访问下标表达式以触发类型/常量折叠检查
                res &= apply(*this, *idx);

                // 期待下标为编译期常量且为整数类型
                if (!idx->attr.val.isConstexpr)
                {
                    errors.push_back(
                        "Array dimension must be a compile-time constant at line " + std::to_string(idx->line_num));
                    res = false;
                    continue;
                }

                auto base = idx->attr.val.value.type->getBaseType();
                if (base != Type_t::INT && base != Type_t::LL)
                {
                    errors.push_back("Array dimension must be an integer at line " + std::to_string(idx->line_num));
                    res = false;
                    continue;
                }

                // 检查维度 > 0
                long long dim = idx->attr.val.getLL();
                if (dim <= 0)
                {
                    errors.push_back("Array dimension must be positive at line " + std::to_string(idx->line_num));
                    res = false;
                    continue;
                }

                // 记录维度信息给上层（VarDeclaration）通过 lval 的 index expressions 可取得
                (void)dim;
            }
        }

        // 处理初始化器（若存在），复制其属性以供上层检查
        if (node.init)
        {
            res &= apply(*this, *node.init);
            node.attr = node.init->attr;
        }

        return res;
    }

    bool ASTChecker::visit(ParamDeclarator& node)
    {
        // TODO(Lab3-1): 实现函数形参的语义检查
        // 检查形参重定义，处理数组形参的类型退化，将形参加入符号表
        if (!node.entry)
        {
            errors.push_back("Parameter with no name at line " + std::to_string(node.line_num));
            return false;
        }

        bool res = true;

        // 如果参数的维度存在，访问每个表达式（以触发常量和类型检查）并记录维度信息
        std::vector<int> dimsVals;
        if (node.dims)
        {
            for (auto* d : *node.dims)
            {
                if (!d) continue;
                res &= apply(*this, *d);

                // 若为字面量-1，表示原本的 `[]`（不定长），保留标记 -1
                if (d->attr.val.isConstexpr)
                {
                    long long v = d->attr.val.getLL();
                    dimsVals.push_back(static_cast<int>(v));
                }
                else
                {
                    // 对于形参数组，可以允许非编译期常量，但此处仍记录为 -1
                    dimsVals.push_back(-1);
                }
            }
        }

        // 参数类型：如果是数组声明，退化为指针
        Type* paramType = node.type;
        if (node.dims) { paramType = TypeFactory::getPtrType(node.type); }

        // 重定义检查：若符号表中当前作用域已有该entry，视为重定义
        FE::AST::VarAttr* curSym = symTable.getSymbol(node.entry);
        if (curSym && curSym->scopeLevel == symTable.getScopeDepth())
        {
            errors.push_back(std::string("redefinition of parameter '") + node.entry->getName() + "' at line " +
                             std::to_string(node.line_num));
            res = false;
        }
        else
        {
            // 添加到符号表：标记为非const，记录类型与作用域深度
            VarAttr attr(paramType, false, symTable.getScopeDepth());
            // 如果是数组参数，记录各维度信息（其中 -1 表示不定）
            if (!dimsVals.empty()) attr.arrayDims = dimsVals;

            symTable.addSymbol(node.entry, attr);

            // 将属性同步到 AST 节点，便于后续检查/IR 生成
            node.attr.val.value.type  = attr.type;
            node.attr.val.isConstexpr = false;
        }

        return res;
    }

    bool ASTChecker::visit(VarDeclaration& node)
    {
        // TODO(Lab3-1): 实现变量声明的语义检查
        // 遍历声明列表，检查重定义，处理数组维度和初始化，将符号加入符号表
        bool res = true;

        if (!node.decls) return true;

        // 辅助函数：将初始化器展平为初始化值列表（支持嵌套 / 展平 / 混合）
        // 额外检查：
        // 不允许初始化嵌套层级超过数组维度
        // 不允许初始化元素总数超过数组容量（过量初始化）
        std::function<void(InitDecl*, VarAttr&, bool&, size_t)> flattenInit =
            [&](InitDecl* init, VarAttr& attr, bool& allConst, size_t depth) {
                if (!init) return;
                // 当遇到初始化列表，但当前嵌套深度 >= 数组维度，说明嵌套层级超出
                size_t maxDepth = attr.arrayDims.size();
                if (auto* list = dynamic_cast<InitializerList*>(init))
                {
                    if (depth >= maxDepth)
                    {
                        errors.push_back("Initializer nesting too deep at line " + std::to_string(list->line_num));
                        res = false;
                        // 仍然遍历以避免后续空指针等问题，但不继续增加 depth
                        for (auto* sub : *(list->init_list))
                        {
                            if (!sub) continue;
                            // 递归仍然调用以触发可能的子错误检查，但标记已错误
                            flattenInit(sub, attr, allConst, depth + 1);
                        }
                        return;
                    }

                    for (auto* sub : *(list->init_list))
                    {
                        if (!sub) continue;

                        // 判断子项是否为子列表（可能被一个 Initializer 包裹）
                        InitializerList* subList = nullptr;
                        if (auto* dl = dynamic_cast<InitializerList*>(sub))
                        {
                            subList = dl;
                        }
                        else if (auto* singleChild = dynamic_cast<Initializer*>(sub))
                        {
                            if (singleChild->init_val)
                            {
                                if (auto* inner = dynamic_cast<InitializerList*>(singleChild->init_val)) subList = inner;
                            }
                        }

                        // 如果是子列表，那么它表示一个完整的子对象；在展开前记录当前已展平的元素数，展开后对该子对象按子容量进行填充
                        if (subList)
                        {
                            size_t before = attr.initList.size();
                            flattenInit(subList, attr, allConst, depth + 1);
                            size_t after = attr.initList.size();
                            size_t added = (after >= before) ? (after - before) : 0;

                            // 计算该子对象的容量（子维度乘积）
                            long long subCap = 1;
                            for (size_t k = depth + 1; k < maxDepth; ++k) subCap *= attr.arrayDims[k];

                            // 已经在该子对象中占用的偏移（可能不是从边界开始）
                            long long offsetInSub = (long long)before % subCap;
                            long long remaining    = subCap - offsetInSub;

                            // 如果当前子列表展开的元素超过子对象剩余容量，则报错
                            if ((long long)added > remaining)
                            {
                                errors.push_back("Excess elements in array initializer at line " + std::to_string(list->line_num));
                                res = false;
                                return;
                            }

                            // 若子列表未填满该子对象剩余部分，则补零到剩余容量
                            if ((long long)added < remaining)
                            {
                                VarValue zero;
                                zero.type = attr.type;
                                for (long long pad = (long long)added; pad < remaining; ++pad) attr.initList.push_back(zero);
                            }
                        }
                        else
                        {
                            // 不是子列表，按普通元素处理（递归会把叶子值 push 进 initList）
                            flattenInit(sub, attr, allConst, depth + 1);
                        }

                        // 检查是否已经超过总体容量
                        if (!attr.arrayDims.empty())
                        {
                            long long cap = 1;
                            for (int d : attr.arrayDims) cap *= d;
                            if ((long long)attr.initList.size() > cap)
                            {
                                errors.push_back(
                                    "Excess elements in array initializer at line " + std::to_string(list->line_num));
                                res = false;
                                return;
                            }
                        }
                    }
                }
                else if (auto* single = dynamic_cast<Initializer*>(init))
                {
                    // 叶子初始化表达式
                    res &= apply(*this, *single);
                    // 记录属性并加入展平列表
                    if (single->attr.val.isConstexpr) { attr.initList.push_back(single->attr.val.value); }
                    else
                    {
                        allConst = false;
                        attr.initList.push_back(single->attr.val.value);
                    }

                    // 检查是否超过数组容量
                    if (!attr.arrayDims.empty())
                    {
                        long long cap = 1;
                        for (int d : attr.arrayDims) cap *= d;
                        if ((long long)attr.initList.size() > cap)
                        {
                            errors.push_back(
                                "Excess elements in array initializer at line " + std::to_string(single->line_num));
                            res = false;
                            return;
                        }
                    }
                }
            };

        for (auto* decl : *(node.decls))
        {
            if (!decl) continue;

            // 访问变量声明器
            res &= apply(*this, *decl);

            // 处理左值，获取符号表条目
            auto* lval = dynamic_cast<LeftValExpr*>(decl->lval);
            if (!lval || !lval->entry)
            {
                errors.push_back("Invalid variable declarator at line " + std::to_string(decl->line_num));
                res = false;
                continue;
            }

            FE::Sym::Entry* entry = lval->entry;

            // 重定义检查：若符号表中当前作用域已有该entry，视为重定义
            FE::AST::VarAttr* cur = symTable.getSymbol(entry);
            if (cur && cur->scopeLevel == symTable.getScopeDepth())
            {
                errors.push_back(std::string("redefinition of variable '") + entry->getName() + "' at line " +
                                 std::to_string(decl->line_num));
                res = false;
                continue;
            }

            // 构造 VarAttr，记录类型、const属性、作用域深度
            VarAttr attr(node.type, node.isConstDecl, symTable.getScopeDepth());

            // 处理数组维度信息
            if (lval->indices && !lval->indices->empty())
            {
                for (auto* idx : *lval->indices)
                {
                    if (!idx) continue;
                    // We expect index was checked in VarDeclarator::visit: positive compile-time integer
                    if (idx->attr.val.isConstexpr)
                    {
                        long long v = idx->attr.val.getLL();
                        attr.arrayDims.push_back(static_cast<int>(v));
                    }
                    else
                    {
                        // Non-constexpr dimension (should already be an error), but tolerate by marking -1
                        attr.arrayDims.push_back(-1);
                        res = false;
                    }
                }
            }

            // 处理初始化器（如果有）
            if (decl->init)
            {
                // 展平初始化器（从 depth=0 开始）
                bool allConst = true;
                flattenInit(decl->init, attr, allConst, 0);

                // 全局变量初始化器必须为编译期常量
                if (symTable.isGlobalScope() && !allConst)
                {
                    errors.push_back("Global variable initializer must be a compile-time constant at line " +
                                     std::to_string(decl->line_num));
                    res = false;
                }

                // 类型检查与转换（原有逻辑），在此基础上还需检查初始化元素数量是否超出
                if (!attr.initList.empty())
                {
                    // 如果是标量（非数组），应只允许单个初始化元素
                    if (attr.arrayDims.empty())
                    {
                        if (attr.initList.size() > 1)
                        {
                            errors.push_back(
                                "Too many initializers for scalar at line " + std::to_string(decl->line_num));
                            res = false;
                        }
                    }
                    else
                    {
                        // 检查总元素数是否超过容量（再次验证）
                        long long cap = 1;
                        for (int d : attr.arrayDims) cap *= d;
                        if ((long long)attr.initList.size() > cap)
                        {
                            errors.push_back(
                                "Excess elements in array initializer at line " + std::to_string(decl->line_num));
                            res = false;
                        }
                    }

                    // 类型兼容性检查与必要的常量转换（保留原实现）
                    auto isNumeric = [](Type_t t) -> bool {
                        return t == Type_t::INT || t == Type_t::LL || t == Type_t::FLOAT || t == Type_t::BOOL;
                    };

                    // 非空初始化列表
                    if (attr.arrayDims.empty())
                    {
                        auto&  val = attr.initList[0];
                        Type_t bv  = val.type->getBaseType();
                        Type_t dv  = node.type->getBaseType();

                        bool typeCompatible = (bv == dv) || (isNumeric(bv) && isNumeric(dv));

                        if (!typeCompatible)
                        {
                            errors.push_back("Type mismatch in initializer at line " + std::to_string(decl->line_num));
                            res = false;
                        }
                        else if (bv != dv && attr.isConstDecl)
                        {
                            // 对于常量声明，如果类型不匹配，需要进行类型转换
                            VarValue convertedVal;
                            convertedVal.type = node.type;

                            switch (dv)
                            {
                                case Type_t::INT:
                                    if (bv == Type_t::FLOAT)
                                        convertedVal.intValue = static_cast<int>(val.floatValue);
                                    else if (bv == Type_t::LL)
                                        convertedVal.intValue = static_cast<int>(val.llValue);
                                    else if (bv == Type_t::BOOL)
                                        convertedVal.intValue = static_cast<int>(val.boolValue);
                                    else
                                        convertedVal = val;
                                    break;

                                case Type_t::LL:
                                    if (bv == Type_t::INT)
                                        convertedVal.llValue = static_cast<long long>(val.intValue);
                                    else if (bv == Type_t::FLOAT)
                                        convertedVal.llValue = static_cast<long long>(val.floatValue);
                                    else if (bv == Type_t::BOOL)
                                        convertedVal.llValue = static_cast<long long>(val.boolValue);
                                    else
                                        convertedVal = val;
                                    break;

                                case Type_t::FLOAT:
                                    if (bv == Type_t::INT)
                                        convertedVal.floatValue = static_cast<float>(val.intValue);
                                    else if (bv == Type_t::LL)
                                        convertedVal.floatValue = static_cast<float>(val.llValue);
                                    else if (bv == Type_t::BOOL)
                                        convertedVal.floatValue = static_cast<float>(val.boolValue);
                                    else
                                        convertedVal = val;
                                    break;

                                default: convertedVal = val; break;
                            }

                            attr.initList[0] = convertedVal;
                        }
                    }
                    else
                    {
                        // 对于数组：检查每个初始化值的类型兼容性
                        auto   elementType = node.type;  // base type
                        Type_t targetType  = elementType->getBaseType();

                        for (auto& v : attr.initList)
                        {
                            Type_t sourceType = v.type->getBaseType();

                            if (sourceType != targetType)
                            {
                                if (isNumeric(sourceType) && isNumeric(targetType))
                                {
                                    VarValue convertedVal;
                                    convertedVal.type = elementType;

                                    switch (targetType)
                                    {
                                        case Type_t::INT:
                                            if (sourceType == Type_t::FLOAT)
                                                convertedVal.intValue = static_cast<int>(v.floatValue);
                                            else if (sourceType == Type_t::LL)
                                                convertedVal.intValue = static_cast<int>(v.llValue);
                                            else if (sourceType == Type_t::BOOL)
                                                convertedVal.intValue = static_cast<int>(v.boolValue);
                                            break;

                                        case Type_t::FLOAT:
                                            if (sourceType == Type_t::INT)
                                                convertedVal.floatValue = static_cast<float>(v.intValue);
                                            else if (sourceType == Type_t::LL)
                                                convertedVal.floatValue = static_cast<float>(v.llValue);
                                            else if (sourceType == Type_t::BOOL)
                                                convertedVal.floatValue = static_cast<float>(v.boolValue);
                                            break;

                                        case Type_t::LL:
                                            if (sourceType == Type_t::INT)
                                                convertedVal.llValue = static_cast<long long>(v.intValue);
                                            else if (sourceType == Type_t::FLOAT)
                                                convertedVal.llValue = static_cast<long long>(v.floatValue);
                                            else if (sourceType == Type_t::BOOL)
                                                convertedVal.llValue = static_cast<long long>(v.boolValue);
                                            break;

                                        default: convertedVal = v; break;
                                    }

                                    v = convertedVal;
                                }
                                else
                                {
                                    errors.push_back(
                                        "Array initializer type mismatch at line " + std::to_string(decl->line_num));
                                    res = false;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                // 未提供初始化器
                if (node.isConstDecl)
                {
                    errors.push_back("Const variable must be initialized at line " + std::to_string(decl->line_num));
                    res = false;
                }
            }

            // 将符号加入符号表
            if (symTable.isGlobalScope())
            {
                // 检查全局变量重定义
                if (glbSymbols.find(entry) != glbSymbols.end())
                {
                    errors.push_back(std::string("redefinition of global variable '") + entry->getName() +
                                     "' at line " + std::to_string(decl->line_num));
                    res = false;
                    continue;
                }
                glbSymbols[entry] = attr;
            }
            else
            {
                symTable.addSymbol(entry, attr);
            }
        }

        return res;
    }
}  // namespace FE::AST
