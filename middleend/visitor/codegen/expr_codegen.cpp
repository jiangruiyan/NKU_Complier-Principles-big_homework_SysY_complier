#include <middleend/visitor/codegen/ast_codegen.h>

namespace ME
{
    void ASTCodeGen::visit(FE::AST::LeftValExpr& node, Module* m)
    {
        // TODO(Lab 3-2): 生成左值表达式的取址/取值 IR
        // 查找变量位置（全局或局部），处理数组下标/GEP，必要时发出load
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement LeftValExpr IR generation");
 
        // 定位变量的基地址指针与属性
        Operand* basePtr = nullptr;
        const FE::AST::VarAttr* attr = nullptr;

        size_t localReg = name2reg.getReg(node.entry);
        if (localReg != static_cast<size_t>(-1))
        {
            basePtr = getRegOperand(localReg);
            auto it = reg2attr.find(localReg);
            ASSERT(it != reg2attr.end() && "Local symbol attr not found");
            attr = &it->second;
        }
        else
        {
            basePtr = getGlobalOperand(node.entry->getName());
            attr = &getGlobalVarAttr(node.entry);
        }

        // 确定元素类型 elemType，规范数组维度与是否为数组对象
        DataType elemType = convert(attr->type);
        if (elemType == DataType::PTR && attr->type->getTypeGroup() == FE::AST::TypeGroup::POINTER)
            elemType = convert(static_cast<FE::AST::PtrType*>(attr->type)->base);

        auto gepDims = sanitizeArrayDims(*attr);
        bool baseIsArrayObject = !attr->arrayDims.empty() && attr->type->getTypeGroup() != FE::AST::TypeGroup::POINTER;
        std::vector<Operand*> idxOps;
        size_t usedIdx = 0;
        if (baseIsArrayObject && !gepDims.empty()) idxOps.push_back(getImmeI32Operand(0));

        // 处理用户提供的下标 node.indices
        if (node.indices)
        {
            idxOps.reserve(idxOps.size() + node.indices->size());
            for (auto* idx : *node.indices)
            {
                if (!idx) continue;
                ++usedIdx; // 记录已使用的下标数量，后续决定是否需要 load
                apply(*this, *idx, m); // 生成下标表达式 IR
                size_t idxReg = getMaxReg();
                DataType idxType = convert(idx->attr.val.value.type);
                if (idxType != DataType::I32)
                {
                    auto convs = createTypeConvertInst(idxType, DataType::I32, idxReg);
                    for (auto* inst : convs) insert(inst);
                    if (!convs.empty()) idxReg = getMaxReg();
                }
                idxOps.push_back(getRegOperand(idxReg));
            }
        }

        // 生成 GEP 指令计算最终地址
        Operand* elemPtr = basePtr;
        if (!idxOps.empty())
        {
            // 如果用户提供的下标数量不足以索引到标量元素（例如访问二维数组的一维时），
            // 在索引列表末尾追加一个0，使 GEP 返回指向第一个元素的 i32* 而不是指向子数组的指针。
            bool needDecayToElementPtr = (!attr->arrayDims.empty() && usedIdx < attr->arrayDims.size());
            if (needDecayToElementPtr)
            {
                idxOps.push_back(getImmeI32Operand(0));
            }

            size_t gepReg = getNewRegId();
            insert(createGEP_I32Inst(elemType, basePtr, gepDims, idxOps, gepReg));
            elemPtr = getRegOperand(gepReg);
        }

        lval2ptr[&node] = elemPtr;

        // 根据左值类型及下标使用情况决定是否需要发出 load 指令取值
        // 1. attr->arrayDims 为空
        // 2. 使用的下标数量大于等于数组维度数量，说明定位到了具体元素地址（实际上只能等于？）
        bool needLoad = attr->arrayDims.empty() || usedIdx >= attr->arrayDims.size();
        if (needLoad)
        {
            size_t resReg = getNewRegId();
            insert(createLoadInst(elemType, elemPtr, resReg));
        }
    }

    void ASTCodeGen::visit(FE::AST::LiteralExpr& node, Module* m)
    {
        (void)m;

        size_t reg = getNewRegId();
        switch (node.literal.type->getBaseType())
        {
            case FE::AST::Type_t::INT:
            case FE::AST::Type_t::LL:  // treat as I32
            {
                int             val  = node.literal.getInt();
                ArithmeticInst* inst = createArithmeticI32Inst_ImmeAll(Operator::ADD, val, 0, reg);  // reg = val + 0
                insert(inst);
                break;
            }
            case FE::AST::Type_t::FLOAT:
            {
                float           val  = node.literal.getFloat();
                ArithmeticInst* inst = createArithmeticF32Inst_ImmeAll(Operator::FADD, val, 0, reg);  // reg = val + 0
                insert(inst);
                break;
            }
            default: ERROR("Unsupported literal type");
        }
    }

    void ASTCodeGen::visit(FE::AST::UnaryExpr& node, Module* m)
    {
        // TODO(Lab 3-2): 生成一元运算的 IR（访问操作数、必要的类型转换、发出运算指令）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement UnaryExpr IR generation");
        // 统一委托给 handleUnaryCalc：内部会访问子表达式、根据类型做必要转换，再在当前基本块插入一元运算指令。
        switch (node.op)
        {
            case FE::AST::Operator::ADD:
            case FE::AST::Operator::SUB:
            case FE::AST::Operator::NOT:
                handleUnaryCalc(*node.expr, node.op, curBlock, m);
                break;
            default: ERROR("Unsupported unary operator");
        }
    }

    void ASTCodeGen::handleAssign(FE::AST::LeftValExpr& lhs, FE::AST::ExprNode& rhs, Module* m)
    {
        // TODO(Lab 3-2): 生成赋值语句的 IR（计算右值、类型转换、store 到左值地址）
        // (void)lhs;
        // (void)rhs;
        // (void)m;
        // TODO("Lab3-2: Implement assignment IR generation");
        apply(*this, lhs, m);
        auto ptrIt = lval2ptr.find(&lhs); // 获取左值地址
        Operand* lhsPtr = ptrIt->second; // 左值地址操作数

        const FE::AST::VarAttr* attr = nullptr;
        size_t lr = name2reg.getReg(lhs.entry);
        if (lr != static_cast<size_t>(-1))
        {
            auto it = reg2attr.find(lr);
            attr = &it->second;
        }
        else
        {
            attr = &getGlobalVarAttr(lhs.entry);
        }

        // 根据左值类型及下标使用情况确定 store 的目标类型
        DataType dstType = convert(attr->type);
        if (attr->type->getTypeGroup() == FE::AST::TypeGroup::POINTER && lhs.indices && !lhs.indices->empty())
        {
            // 指针加下标，表示要写入指向的元素，转到基类型
            dstType = convert(static_cast<FE::AST::PtrType*>(attr->type)->base);
        }

        apply(*this, rhs, m);
        size_t   rhsReg  = getMaxReg();
        DataType srcType = convert(rhs.attr.val.value.type);
        if (dstType == DataType::PTR || srcType == DataType::PTR)
        {
            // 指针不支持默认类型转换，要求左右类型一致
            // 应该到不了这儿
            //ASSERT(dstType == srcType && "Pointer assignment type mismatch");
        }
        else if (srcType != dstType)
        {
            auto convs = createTypeConvertInst(srcType, dstType, rhsReg);
            for (auto* inst : convs) insert(inst);
            if (!convs.empty()) rhsReg = getMaxReg();
        }

        insert(createStoreInst(dstType, rhsReg, lhsPtr));
    }

    void ASTCodeGen::handleLogicalAnd(
        FE::AST::BinaryExpr& node, FE::AST::ExprNode& lhs, FE::AST::ExprNode& rhs, Module* m)
    {
        // TODO(Lab 3-2): 生成短路与的基本块与条件分支
        // (void)node;
        // (void)lhs;
        // (void)rhs;
        // (void)m;
        // TODO("Lab3-2: Implement logical AND codegen");

        // 计算左侧表达式，若类型不是i1则转换
        apply(*this, lhs, m);
        size_t lhsReg = getMaxReg();
        DataType lhsType = convert(lhs.attr.val.value.type);
        if (lhsType != DataType::I1)
        {
            auto convs = createTypeConvertInst(lhsType, DataType::I1, lhsReg);
            for (auto* inst : convs) insert(inst);
            if (!convs.empty()) lhsReg = getMaxReg();
        }

        Block* lhsBlock = curBlock;
        Block* rhsBlock = createBlock();
        Block* endBlock = createBlock();

        // 分支语句：实现逻辑短路（分支到rhs或end）
        insert(createBranchInst(lhsReg, rhsBlock->blockId, endBlock->blockId));

        // 计算右侧表达式，若类型不是i1则转换（和左侧类似，并且只有在左侧为真时才会计算右侧）
        enterBlock(rhsBlock);
        apply(*this, rhs, m);
        size_t   rhsReg  = getMaxReg();
        DataType rhsType = convert(rhs.attr.val.value.type);
        if (rhsType != DataType::I1)
        {
            auto convs = createTypeConvertInst(rhsType, DataType::I1, rhsReg);
            for (auto* inst : convs) insert(inst);
            if (!convs.empty()) rhsReg = getMaxReg();
        }
        Block* rhsEndBlock = curBlock;

        insert(createBranchInst(endBlock->blockId)); // 无条件跳转到汇合块

        // 在汇合块用phi合并结果
        enterBlock(endBlock);
        size_t resReg = getNewRegId();
        auto* phi = new PhiInst(DataType::I1, getRegOperand(resReg));
        // lhs 为假时直接复用 lhsReg（已保证为 i1），避免使用 i32 立即数导致的类型不一致
        auto* lhsLabel = getLabelOperand(lhsBlock->blockId);
        auto* rhsLabel = getLabelOperand(rhsEndBlock->blockId);
        phi->addIncoming(getRegOperand(lhsReg), lhsLabel);
        phi->addIncoming(getRegOperand(rhsReg), rhsLabel);
        // %res = phi i1 [ %lhsReg, %lhsBlock ], [ %rhsReg, %rhsBlock ]
        insert(phi);
    }

    void ASTCodeGen::handleLogicalOr(
        FE::AST::BinaryExpr& node, FE::AST::ExprNode& lhs, FE::AST::ExprNode& rhs, Module* m)
    {
        // TODO(Lab 3-2): 生成短路或的基本块与条件分支
        // (void)node;
        // (void)lhs;
        // (void)rhs;
        // (void)m;
        // TODO("Lab3-2: Implement logical OR codegen");

        apply(*this, lhs, m);
        size_t lhsReg = getMaxReg();
        DataType lhsType = convert(lhs.attr.val.value.type);
        if (lhsType != DataType::I1)
        {
            auto convs = createTypeConvertInst(lhsType, DataType::I1, lhsReg);
            for (auto* inst : convs) insert(inst);
            if (!convs.empty()) lhsReg = getMaxReg();
        }

        Block* lhsBlock = curBlock;
        Block* rhsBlock = createBlock();
        Block* endBlock = createBlock();

        insert(createBranchInst(lhsReg, endBlock->blockId, rhsBlock->blockId));

        enterBlock(rhsBlock);
        apply(*this, rhs, m);
        size_t rhsReg = getMaxReg();
        DataType rhsType = convert(rhs.attr.val.value.type);
        if (rhsType != DataType::I1)
        {
            auto convs = createTypeConvertInst(rhsType, DataType::I1, rhsReg);
            for (auto* inst : convs) insert(inst);
            if (!convs.empty()) rhsReg = getMaxReg();
        }
        Block* rhsEndBlock = curBlock;

        insert(createBranchInst(endBlock->blockId));

        enterBlock(endBlock);
        size_t resReg = getNewRegId();
        auto* phi = new PhiInst(DataType::I1, getRegOperand(resReg));
        // 直接使用 lhs 已经转为 i1 的结果，避免使用 i32 立即数导致类型不匹配
        auto* lhsLabel = getLabelOperand(lhsBlock->blockId);
        auto* rhsLabel = getLabelOperand(rhsEndBlock->blockId);
        phi->addIncoming(getRegOperand(lhsReg), lhsLabel);
        phi->addIncoming(getRegOperand(rhsReg), rhsLabel);
        insert(phi);
    }

    void ASTCodeGen::visit(FE::AST::BinaryExpr& node, Module* m)
    {
        // TODO(Lab 3-2): 生成二元表达式 IR（含赋值、逻辑与/或、算术/比较）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement BinaryExpr IR generation");

        switch (node.op)
        {
            case FE::AST::Operator::ASSIGN:
            {
                auto* lval = dynamic_cast<FE::AST::LeftValExpr*>(node.lhs);
                ASSERT(lval && "Assignment lhs must be a LeftValExpr");
                // 赋值需要先取到左值地址，再写入右值结果
                handleAssign(*lval, *node.rhs, m);
                break;
            }
            case FE::AST::Operator::AND:
                // 逻辑与：需要短路控制流和 phi 汇聚
                handleLogicalAnd(node, *node.lhs, *node.rhs, m);
                break;
            case FE::AST::Operator::OR:
                // 逻辑或：同样走短路求值
                handleLogicalOr(node, *node.lhs, *node.rhs, m);
                break;
            default:
                // 其余算术/比较统一走普通二元计算，内部完成类型提升
                handleBinaryCalc(*node.lhs, *node.rhs, node.op, curBlock, m);
                break;
        }
    }

    void ASTCodeGen::visit(FE::AST::CallExpr& node, Module* m)
    {
        // TODO(Lab 3-2): 生成函数调用 IR（准备参数、可选返回寄存器、发出call）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement CallExpr IR generation");

        CallInst::argList args;
        const FE::AST::FuncDeclStmt* decl = nullptr; // 函数声明（用于获取参数类型和返回类型）
        auto fit = funcDecls.find(node.func); // 查找函数声明
        if (fit != funcDecls.end()) decl = fit->second; // fit是函数声明迭代器

        size_t argIdx = 0;
        if (node.args)
        {
            // 生成与准备参数列表
            for (auto* arg : *node.args)
            {
                if (!arg) // 跳过空参数
                {
                    ++argIdx;
                    continue;
                }

                apply(*this, *arg, m); // 对（每个）参数表达式生成 IR

                DataType expectType = DataType::UNK; // 期望的参数类型(UNK ir_defs.h:8)
                if (decl && decl->params && argIdx < decl->params->size())
                {
                    auto* p = (*decl->params)[argIdx];
                    // 若参数是数组且未完全下标化，则按指针传递
                    if (p) expectType = (p->dims && !p->dims->empty()) ? DataType::PTR : convert(p->type);
                }

                Operand* op = nullptr;
                if (auto* lval = dynamic_cast<FE::AST::LeftValExpr*>(arg))
                {
                    const FE::AST::VarAttr* attr = nullptr;
                    size_t lr = name2reg.getReg(lval->entry);
                    if (lr != static_cast<size_t>(-1))
                    {
                        auto it = reg2attr.find(lr);
                        if (it != reg2attr.end()) attr = &it->second;
                    }
                    else
                    {
                        attr = &getGlobalVarAttr(lval->entry);
                    }

                    // 数组实参未完全下标化时需要退化为指针传递（与语义检查保持一致）
                    if (attr && !attr->arrayDims.empty())
                    {
                        size_t idxCnt = lval->indices ? lval->indices->size() : 0;
                        if (idxCnt < attr->arrayDims.size())
                        {
                            auto ptrIt = lval2ptr.find(lval);
                            op = ptrIt->second;
                            if (expectType == DataType::UNK) expectType = DataType::PTR;
                        }
                    }
                }

                if (!op)  // 前面没有走左值数组退化为指针的路径时，需要把实参表达式的结果寄存器作为参数操作数
                {
                    size_t reg = getMaxReg();
                    DataType srcType = convert(arg->attr.val.value.type);

                    if (expectType == DataType::PTR)
                    {
                        // 语义阶段已保证指针匹配，这里仍断言一次，便于调试
                        //ASSERT(srcType == DataType::PTR && "Pointer argument type mismatch");
                    }
                    else if (expectType != DataType::UNK && expectType != srcType)
                    {
                        auto convs = createTypeConvertInst(srcType, expectType, reg);
                        for (auto* inst : convs) insert(inst);
                        if (!convs.empty())
                        {
                            reg = getMaxReg();
                            srcType = expectType;
                        }
                    }

                    if (expectType == DataType::UNK) expectType = srcType;
                    op = getRegOperand(reg);
                }

                args.push_back({expectType, op});
                ++argIdx;
            }
        }

        // 处理返回值
        DataType retType = decl ? convert(decl->retType) : convert(node.attr.val.value.type);
        if (retType != DataType::VOID)
        {
            size_t resReg = getNewRegId();
            insert(createCallInst(retType, node.func->getName(), args, resReg));
        }
        else
        {
            insert(createCallInst(retType, node.func->getName(), args));
        }
    }

    void ASTCodeGen::visit(FE::AST::CommaExpr& node, Module* m)
    {
        // TODO(Lab 3-2): 依序生成逗号表达式每个子表达式的 IR
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement CommaExpr IR generation");

        if (!node.exprs) return;
        // 逗号表达式：遍历每个子表达式，生成其 IR
        for (auto* expr : *node.exprs)
        {
            if (!expr) continue;
            apply(*this, *expr, m);
            // 当前基本块若已被终结（如短路生成的 branch/ret），后续子表达式无法继续插入
            if (!curBlock->insts.empty() && curBlock->insts.back()->isTerminator()) break;
        }
    }
}  // namespace ME
