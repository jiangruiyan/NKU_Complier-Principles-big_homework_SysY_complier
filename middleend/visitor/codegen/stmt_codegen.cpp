#include <middleend/visitor/codegen/ast_codegen.h>

namespace ME
{
    void ASTCodeGen::visit(FE::AST::ExprStmt& node, Module* m)
    {
        if (!node.expr) return;
        apply(*this, *node.expr, m);
    }

    void ASTCodeGen::visit(FE::AST::FuncDeclStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成函数定义 IR（形参、入口/结束基本块、返回补丁）
        // 设置函数返回类型与参数寄存器，创建基本块骨架，并生成函数体
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement FuncDeclStmt IR generation");

        // 清理并初始化函数级环境
        while (name2reg.curScope && name2reg.curScope->parent) name2reg.exitScope();
        if (name2reg.curScope)
            name2reg.curScope->sym2Reg.clear();
        else
            name2reg.curScope = new RegTab::Scope(nullptr);
        reg2attr.clear();
        paramPtrTab.clear();
        lval2ptr.clear();

        DataType retType = convert(node.retType);

        // 处理函数参数
        FuncDefInst::argList args;
        std::vector<FE::AST::VarAttr> paramAttrs;
        size_t idx = 0;
        if (node.params)
        {
            paramAttrs.reserve(node.params->size()); // 预分配空间
            for (auto* p : *node.params)
            {
                DataType pType = (p->dims && !p->dims->empty()) ? DataType::PTR : convert(p->type);
                size_t   regId = idx + 1;
                args.push_back({pType, getRegOperand(regId)});

                FE::AST::VarAttr attr(p->type, false, 1);
                // 如果参数在 IR 层被视为指针（例如形参 int a[][N] 退化为指针），
                // 则在 reg2attr 中记录的类型也应该为指针类型，
                // 以便后续的 GEP 生成能正确判断 baseIsArrayObject。
                if (pType == DataType::PTR)
                {
                    attr.type = FE::AST::TypeFactory::getPtrType(p->type);
                }
                if (p->dims)
                {
                    for (auto* d : *p->dims)
                    {
                        int dim = 0;
                        if (d && d->attr.val.value.type != FE::AST::voidType)
                            dim = d->attr.val.getInt();
                        else if (auto* lit = dynamic_cast<FE::AST::LiteralExpr*>(d))
                            dim = lit->literal.getInt();
                        attr.arrayDims.push_back(dim);
                    }
                }
                paramAttrs.push_back(attr);
                paramPtrTab[idx] = (pType == DataType::PTR);
                ++idx;
            }
        }

        // 创建函数定义与函数对象
        auto* funcDef = new FuncDefInst(retType, node.entry->getName(), args);
        auto* func = new Function(funcDef);
        func->setMaxReg(args.size());
        m->functions.push_back(func);

        Block* entryBlock = func->createBlock();
        entryBlock->setComment("entry");
        Block* endBlock = func->createBlock();
        endBlock->setComment("end");

        enterFunc(func); // 进入函数
        enterBlock(entryBlock); // 进入函数入口块

        for (size_t i = 0; node.params && i < node.params->size(); ++i)
        {
            auto* p = (*node.params)[i];
            auto& attr = paramAttrs[i];
            bool isPtr = paramPtrTab[i];
            size_t argReg = i + 1;

            if (isPtr)
            {
                name2reg.addSymbol(p->entry, argReg);
                reg2attr[argReg] = attr;
            }
            else
            {
                size_t ptrReg = getNewRegId();
                insert(createAllocaInst(convert(p->type), ptrReg));
                insert(createStoreInst(convert(p->type), getRegOperand(argReg), getRegOperand(ptrReg)));
                name2reg.addSymbol(p->entry, ptrReg);
                reg2attr[ptrReg] = attr;
            }
        }

        if (node.body) apply(*this, *node.body, m);

        // 处理函数结束块与返回补丁（若当前块没有终结指令，则补一条跳转到 end 的分支）
        if (!curBlock || curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(endBlock->blockId));

        enterBlock(endBlock);
        if (endBlock->insts.empty() || !endBlock->insts.back()->isTerminator())
        {
            if (retType == DataType::F32)
                insert(createRetInst(0.0f));
            else if (retType == DataType::VOID)
                insert(createRetInst());
            else
                insert(createRetInst(0));
        }

        exitBlock(); // 退出函数结束块
        exitFunc();  // 退出函数
    }

    void ASTCodeGen::visit(FE::AST::VarDeclStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成变量声明语句 IR（局部变量分配、初始化）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement VarDeclStmt IR generation");
        if (node.decl) apply(*this, *node.decl, m);
    }

    void ASTCodeGen::visit(FE::AST::BlockStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成语句块 IR（作用域管理，顺序生成子语句）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement BlockStmt IR generation");

        name2reg.enterScope();
        if (node.stmts)
        {
            for (auto* stmt : *node.stmts)
            {
                if (!stmt) continue;
                apply(*this, *stmt, m);
            }
        }
        name2reg.exitScope();
    }

    void ASTCodeGen::visit(FE::AST::ReturnStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成 return 语句 IR（可选返回值与类型转换）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement ReturnStmt IR generation");

        DataType retType = curFunc->funcDef->retType;

        if (node.retExpr) // 有返回值
        {
            apply(*this, *node.retExpr, m);
            size_t retReg = getMaxReg();
            DataType srcType = convert(node.retExpr->attr.val.value.type);

            if (retType != DataType::VOID && srcType != retType)
            {
                auto convs = createTypeConvertInst(srcType, retType, retReg);
                for (auto* inst : convs) insert(inst);
                if (!convs.empty()) retReg = getMaxReg();
            }

            if (retType == DataType::VOID)
                insert(createRetInst());
            else
                insert(createRetInst(retType, retReg));
        }
        else // 无返回值
        {
            if (retType == DataType::F32)
                insert(createRetInst(0.0f));
            else if (retType == DataType::VOID)
                insert(createRetInst());
            else
                insert(createRetInst(0));
        }
    }

    void ASTCodeGen::visit(FE::AST::WhileStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成 while 循环 IR（条件块、循环体与结束块、循环标签）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement WhileStmt IR generation");

        // 构建循环的基本块
        Block* condBlock = createBlock();
        condBlock->setComment("while.cond");
        Block* bodyBlock = createBlock();
        bodyBlock->setComment("while.body");
        Block* endBlock = createBlock();
        endBlock->setComment("while.end");

        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(condBlock->blockId));

        // 保存之前的循环标签，更新为当前循环的标签
        size_t prevLoopStart = curFunc->loopStartLabel;
        size_t prevLoopEnd = curFunc->loopEndLabel;
        curFunc->loopStartLabel = condBlock->blockId;
        curFunc->loopEndLabel = endBlock->blockId;

        // 进入条件块，生成条件表达式
        enterBlock(condBlock);
        apply(*this, *node.cond, m);
        size_t condReg = getMaxReg();
        DataType condType = convert(node.cond->attr.val.value.type);
        if (condType != DataType::I1)
        {
            auto convs = createTypeConvertInst(condType, DataType::I1, condReg);
            for (auto* inst : convs) insert(inst);
            if (!convs.empty()) condReg = getMaxReg();
        }
        // 根据条件结果分支到循环体或结束块
        insert(createBranchInst(condReg, bodyBlock->blockId, endBlock->blockId));

        // 进入循环体块，生成循环体语句
        enterBlock(bodyBlock);
        if (node.body) apply(*this, *node.body, m);
        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(condBlock->blockId));

        // 进入结束块
        enterBlock(endBlock);
        curFunc->loopStartLabel = prevLoopStart;
        curFunc->loopEndLabel = prevLoopEnd;
    }

    void ASTCodeGen::visit(FE::AST::IfStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成 if/else IR（then/else/end 基本块与条件分支）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement IfStmt IR generation");

        Block* thenBlock = createBlock();
        thenBlock->setComment("if.then");
        Block* elseBlock = nullptr;
        if (node.elseStmt) // 有 else 分支
        {
            elseBlock = createBlock();
            elseBlock->setComment("if.else");
        }
        Block* endBlock = createBlock();
        endBlock->setComment("if.end");

        // 生成条件表达式
        apply(*this, *node.cond, m);
        size_t condReg = getMaxReg();
        DataType condType = convert(node.cond->attr.val.value.type);
        if (condType != DataType::I1)
        {
            auto convs = createTypeConvertInst(condType, DataType::I1, condReg);
            for (auto* inst : convs) insert(inst);
            if (!convs.empty()) condReg = getMaxReg();
        }

        // 根据条件结果分支到 then 或 else/end
        size_t falseLabel = elseBlock ? elseBlock->blockId : endBlock->blockId;
        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(condReg, thenBlock->blockId, falseLabel));

        // 生成 then 块
        enterBlock(thenBlock);
        if (node.thenStmt) apply(*this, *node.thenStmt, m);
        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(endBlock->blockId));

        // 生成 else 块（若有）
        if (elseBlock)
        {
            enterBlock(elseBlock);
            if (node.elseStmt) apply(*this, *node.elseStmt, m);
            if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
                insert(createBranchInst(endBlock->blockId));
        }

        enterBlock(endBlock);
    }

    void ASTCodeGen::visit(FE::AST::BreakStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成 break 的无条件跳转至循环结束块
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement BreakStmt IR generation");

        (void)node;
        (void)m;

        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(curFunc->loopEndLabel));
    }

    void ASTCodeGen::visit(FE::AST::ContinueStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成 continue 的无条件跳转至循环步进/条件块
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement ContinueStmt IR generation");

        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(curFunc->loopStartLabel));
    }

    // 此部分可不实现？
    void ASTCodeGen::visit(FE::AST::ForStmt& node, Module* m)
    {
        // TODO(Lab 3-2): 生成 for 循环 IR（init/cond/body/step 基本块与循环标签）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement ForStmt IR generation");

        Block* initBlock = createBlock();
        initBlock->setComment("for.init");
        Block* condBlock = createBlock();
        condBlock->setComment("for.cond");
        Block* bodyBlock = createBlock();
        bodyBlock->setComment("for.body");
        Block* stepBlock = createBlock();
        stepBlock->setComment("for.step");
        Block* endBlock = createBlock();
        endBlock->setComment("for.end");

        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(initBlock->blockId));

        size_t prevLoopStart = curFunc->loopStartLabel;
        size_t prevLoopEnd = curFunc->loopEndLabel;
        curFunc->loopStartLabel = stepBlock->blockId;
        curFunc->loopEndLabel = endBlock->blockId;

        name2reg.enterScope();

        enterBlock(initBlock);
        if (node.init) apply(*this, *node.init, m);
        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(condBlock->blockId));

        enterBlock(condBlock);
        if (node.cond)
        {
            apply(*this, *node.cond, m);
            size_t   condReg  = getMaxReg();
            DataType condType = convert(node.cond->attr.val.value.type);
            if (condType != DataType::I1)
            {
                auto convs = createTypeConvertInst(condType, DataType::I1, condReg);
                for (auto* inst : convs) insert(inst);
                if (!convs.empty()) condReg = getMaxReg();
            }
            insert(createBranchInst(condReg, bodyBlock->blockId, endBlock->blockId));
        }
        else
        {
            insert(createBranchInst(bodyBlock->blockId));
        }

        enterBlock(bodyBlock);
        if (node.body) apply(*this, *node.body, m);
        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(stepBlock->blockId));

        enterBlock(stepBlock);
        if (node.step) apply(*this, *node.step, m);
        if (curBlock->insts.empty() || !curBlock->insts.back()->isTerminator())
            insert(createBranchInst(condBlock->blockId));

        enterBlock(endBlock);
        curFunc->loopStartLabel = prevLoopStart;
        curFunc->loopEndLabel = prevLoopEnd;
        name2reg.exitScope();
    }
}  // namespace ME
