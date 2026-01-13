#include <middleend/visitor/codegen/ast_codegen.h>
#include <debug.h>

namespace ME
{
    void ASTCodeGen::libFuncRegister(Module* m)
    {
        auto& decls = m->funcDecls;

        // int getint();
        decls.emplace_back(new FuncDeclInst(DataType::I32, "getint"));

        // int getch();
        decls.emplace_back(new FuncDeclInst(DataType::I32, "getch"));

        // int getarray(int a[]);
        decls.emplace_back(new FuncDeclInst(DataType::I32, "getarray", {DataType::PTR}));

        // float getfloat();
        decls.emplace_back(new FuncDeclInst(DataType::F32, "getfloat"));

        // int getfarray(float a[]);
        decls.emplace_back(new FuncDeclInst(DataType::I32, "getfarray", {DataType::PTR}));

        // void putint(int a);
        decls.emplace_back(new FuncDeclInst(DataType::VOID, "putint", {DataType::I32}));

        // void putch(int a);
        decls.emplace_back(new FuncDeclInst(DataType::VOID, "putch", {DataType::I32}));

        // void putarray(int n, int a[]);
        decls.emplace_back(new FuncDeclInst(DataType::VOID, "putarray", {DataType::I32, DataType::PTR}));

        // void putfloat(float a);
        decls.emplace_back(new FuncDeclInst(DataType::VOID, "putfloat", {DataType::F32}));

        // void putfarray(int n, float a[]);
        decls.emplace_back(new FuncDeclInst(DataType::VOID, "putfarray", {DataType::I32, DataType::PTR}));

        // void starttime(int lineno);
        decls.emplace_back(new FuncDeclInst(DataType::VOID, "_sysy_starttime", {DataType::I32}));

        // void stoptime(int lineno);
        decls.emplace_back(new FuncDeclInst(DataType::VOID, "_sysy_stoptime", {DataType::I32}));

        // llvm memset
        decls.emplace_back(new FuncDeclInst(
            DataType::VOID, "llvm.memset.p0.i32", {DataType::PTR, DataType::I8, DataType::I32, DataType::I1}));
    }


    void ASTCodeGen::handleGlobalVarDecl(FE::AST::VarDeclStmt* decls, Module* m)
    {
        // TODO(Lab 3-2): 生成全局变量声明 IR（支持标量与数组的初值）
        // (void)decls;
        // (void)m;
        // TODO("Lab3-2: Implement global var declaration IR generation");
        auto* varDecl = decls->decl;
        for (auto* d : *varDecl->decls)
        {
            if (!d) continue;

            auto* lval = dynamic_cast<FE::AST::LeftValExpr*>(d->lval);

            const auto& attr = getGlobalVarAttr(lval->entry); // 获取全局变量属性

            DataType dt = convert(attr.type);
            std::string name = lval->entry->getName();

            if (attr.arrayDims.empty()) // 非数组全局变量
            {
                Operand* initOp = nullptr;
                if (!attr.initList.empty())
                {
                    const auto& initVal = attr.initList.front(); // 返回第一个初始化值（此处为非数组，OK）
                    if (dt == DataType::F32){
                        initOp = getImmeF32Operand(initVal.getFloat());
                    }
                    else{
                        initOp = getImmeI32Operand(initVal.getInt());
                    }
                }
                m->globalVars.emplace_back(new GlbVarDeclInst(dt, name, initOp));
            }
            else // 数组全局变量
            {
                FE::AST::VarAttr arrayAttr = attr;
                size_t           totalElem = 1;
                bool             validDims = true;
                for (int dim : arrayAttr.arrayDims)
                {
                    if (dim <= 0)
                    {
                        validDims = false;
                        break;
                    }
                    totalElem *= static_cast<size_t>(dim);
                }

                if (validDims && totalElem > arrayAttr.initList.size())
                {
                    auto makeZeroValue = [&]() -> FE::AST::VarValue {
                        switch (arrayAttr.type->getBaseType())
                        {
                            case FE::AST::Type_t::BOOL: return FE::AST::VarValue(false);
                            case FE::AST::Type_t::INT: return FE::AST::VarValue(0);
                            case FE::AST::Type_t::LL: return FE::AST::VarValue(0LL);
                            case FE::AST::Type_t::FLOAT: return FE::AST::VarValue(0.0f);
                            default:
                            {
                                FE::AST::VarValue zero;
                                zero.type     = arrayAttr.type;
                                zero.intValue = 0;
                                return zero;
                            }
                        }
                    };
                    auto zeroValue = makeZeroValue();
                    arrayAttr.initList.resize(totalElem, zeroValue);
                }

                m->globalVars.emplace_back(new GlbVarDeclInst(dt, name, arrayAttr));
            }
        }
    }

    // 貌似SysY语言不需要实现这个？（以下3个函数）
    void ASTCodeGen::normalizeArrayDims(FE::AST::VarAttr& attr, size_t initElemCount)
    {
        if (attr.arrayDims.empty()) return;

        size_t unknownIdx = attr.arrayDims.size(); // 记录未指定维度的索引位置，初始为无效值
        for (size_t i = 0; i < attr.arrayDims.size(); ++i)
        {
            if (attr.arrayDims[i] <= 0) // 负维度或零维度也算未指定
            {
                if (unknownIdx != attr.arrayDims.size()) return; // 已有未指定维度，不能再有第二个，否则返回
                unknownIdx = i;
            }
        }

        if (unknownIdx == attr.arrayDims.size() || initElemCount == 0) return; // 无未指定维度或无初始化元素，直接返回

        long long stride = 1;
        for (size_t i = unknownIdx + 1; i < attr.arrayDims.size(); ++i)
        {
            int dim = attr.arrayDims[i];
            if (dim <= 0) return;
            stride *= dim;
        }
        if (stride <= 0) return;

        long long derived = (static_cast<long long>(initElemCount) + stride - 1) / stride;
        if (derived <= 0) return;
        attr.arrayDims[unknownIdx] = static_cast<int>(derived);
    }
    const FE::AST::VarAttr& ASTCodeGen::getGlobalVarAttr(FE::Sym::Entry* entry)
    {
        auto cacheIt = glbAttrCache.find(entry);
        if (cacheIt != glbAttrCache.end()) return cacheIt->second;

        auto symIt = glbSymbols.find(entry);
        auto [insertIt, _] = glbAttrCache.emplace(entry, symIt->second);
        auto& cached = insertIt->second;
        if (!cached.arrayDims.empty())
        {
            normalizeArrayDims(cached, cached.initList.size());
        }
        return cached;
    }
    std::vector<int> ASTCodeGen::sanitizeArrayDims(const FE::AST::VarAttr& attr) const
    {
        std::vector<int> cleaned;
        cleaned.reserve(attr.arrayDims.size());
        for (int dim : attr.arrayDims)
        {
            if (dim > 0) cleaned.push_back(dim);
        }
        return cleaned;
    }

    void ASTCodeGen::visit(FE::AST::Root& node, Module* m)
    {
        // 示例：注册库函数
        libFuncRegister(m);

        // TODO(Lab 3-2): 生成模块级 IR
        // 处理顶层语句：全局变量声明、函数定义等
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement Root IR generation");
        auto* stmts = node.getStmts();
        if (!stmts) return;

        // 逐个处理根节点的顶层语句，仅允许全局变量声明或函数定义
        for (auto* stmt : *stmts)
        {
            if (!stmt) continue;

            if (auto* varDecl = dynamic_cast<FE::AST::VarDeclStmt*>(stmt)) { handleGlobalVarDecl(varDecl, m); }
            else if (auto* funcDecl = dynamic_cast<FE::AST::FuncDeclStmt*>(stmt)) 
            {
                apply(*this, *funcDecl, m);  // apply函数简化访问者调用（封装了“根据节点实际类型选择正确 visit 并转发额外参数”的模板工具）
            }  
            else
            {
                ERROR("Unsupported top-level statement at line %d", stmt->line_num);
            }
        }
    }

    LoadInst* ASTCodeGen::createLoadInst(DataType t, Operand* ptr, size_t resReg)
    {
        return new LoadInst(t, ptr, getRegOperand(resReg));
    }

    StoreInst* ASTCodeGen::createStoreInst(DataType t, size_t valReg, Operand* ptr)
    {
        return new StoreInst(t, getRegOperand(valReg), ptr);
    }
    StoreInst* ASTCodeGen::createStoreInst(DataType t, Operand* val, Operand* ptr)
    {
        return new StoreInst(t, val, ptr);
    }

    ArithmeticInst* ASTCodeGen::createArithmeticI32Inst(Operator op, size_t lhsReg, size_t rhsReg, size_t resReg)
    {
        return new ArithmeticInst(
            op, DataType::I32, getRegOperand(lhsReg), getRegOperand(rhsReg), getRegOperand(resReg));
    }
    ArithmeticInst* ASTCodeGen::createArithmeticI32Inst_ImmeLeft(Operator op, int lhsVal, size_t rhsReg, size_t resReg)
    {
        return new ArithmeticInst(
            op, DataType::I32, getImmeI32Operand(lhsVal), getRegOperand(rhsReg), getRegOperand(resReg));
    }
    ArithmeticInst* ASTCodeGen::createArithmeticI32Inst_ImmeAll(Operator op, int lhsVal, int rhsVal, size_t resReg)
    {
        return new ArithmeticInst(
            op, DataType::I32, getImmeI32Operand(lhsVal), getImmeI32Operand(rhsVal), getRegOperand(resReg));
    }
    ArithmeticInst* ASTCodeGen::createArithmeticF32Inst(Operator op, size_t lhsReg, size_t rhsReg, size_t resReg)
    {
        return new ArithmeticInst(
            op, DataType::F32, getRegOperand(lhsReg), getRegOperand(rhsReg), getRegOperand(resReg));
    }
    ArithmeticInst* ASTCodeGen::createArithmeticF32Inst_ImmeLeft(
        Operator op, float lhsVal, size_t rhsReg, size_t resReg)
    {
        return new ArithmeticInst(
            op, DataType::F32, getImmeF32Operand(lhsVal), getRegOperand(rhsReg), getRegOperand(resReg));
    }
    ArithmeticInst* ASTCodeGen::createArithmeticF32Inst_ImmeAll(Operator op, float lhsVal, float rhsVal, size_t resReg)
    {
        return new ArithmeticInst(
            op, DataType::F32, getImmeF32Operand(lhsVal), getImmeF32Operand(rhsVal), getRegOperand(resReg));
    }

    IcmpInst* ASTCodeGen::createIcmpInst(ICmpOp cond, size_t lhsReg, size_t rhsReg, size_t resReg)
    {
        return new IcmpInst(DataType::I32, cond, getRegOperand(lhsReg), getRegOperand(rhsReg), getRegOperand(resReg));
    }
    IcmpInst* ASTCodeGen::createIcmpInst_ImmeRight(ICmpOp cond, size_t lhsReg, int rhsVal, size_t resReg)
    {
        return new IcmpInst(
            DataType::I32, cond, getRegOperand(lhsReg), getImmeI32Operand(rhsVal), getRegOperand(resReg));
    }
    FcmpInst* ASTCodeGen::createFcmpInst(FCmpOp cond, size_t lhsReg, size_t rhsReg, size_t resReg)
    {
        return new FcmpInst(DataType::F32, cond, getRegOperand(lhsReg), getRegOperand(rhsReg), getRegOperand(resReg));
    }
    FcmpInst* ASTCodeGen::createFcmpInst_ImmeRight(FCmpOp cond, size_t lhsReg, float rhsVal, size_t resReg)
    {
        return new FcmpInst(
            DataType::F32, cond, getRegOperand(lhsReg), getImmeF32Operand(rhsVal), getRegOperand(resReg));
    }

    FP2SIInst* ASTCodeGen::createFP2SIInst(size_t srcReg, size_t destReg)
    {
        return new FP2SIInst(getRegOperand(srcReg), getRegOperand(destReg));
    }
    SI2FPInst* ASTCodeGen::createSI2FPInst(size_t srcReg, size_t destReg)
    {
        return new SI2FPInst(getRegOperand(srcReg), getRegOperand(destReg));
    }
    ZextInst* ASTCodeGen::createZextInst(size_t srcReg, size_t destReg, size_t srcBits, size_t destBits)
    {
        ASSERT(srcBits == 1 && destBits == 32 && "Currently only support i1 to i32 zext");
        return new ZextInst(DataType::I1, DataType::I32, getRegOperand(srcReg), getRegOperand(destReg));
    }

    GEPInst* ASTCodeGen::createGEP_I32Inst(
        DataType t, Operand* ptr, std::vector<int> dims, std::vector<Operand*> is, size_t resReg)
    {
        return new GEPInst(t, DataType::I32, ptr, getRegOperand(resReg), dims, is);
    }

    CallInst* ASTCodeGen::createCallInst(DataType t, std::string funcName, CallInst::argList args, size_t resReg)
    {
        return new CallInst(t, funcName, args, getRegOperand(resReg));
    }
    CallInst* ASTCodeGen::createCallInst(DataType t, std::string funcName, CallInst::argList args)
    {
        return new CallInst(t, funcName, args);
    }
    CallInst* ASTCodeGen::createCallInst(DataType t, std::string funcName, size_t resReg)
    {
        return new CallInst(t, funcName, getRegOperand(resReg));
    }
    CallInst* ASTCodeGen::createCallInst(DataType t, std::string funcName) { return new CallInst(t, funcName); }

    RetInst* ASTCodeGen::createRetInst() { return new RetInst(DataType::VOID); }
    RetInst* ASTCodeGen::createRetInst(DataType t, size_t retReg) { return new RetInst(t, getRegOperand(retReg)); }
    RetInst* ASTCodeGen::createRetInst(int val) { return new RetInst(DataType::I32, getImmeI32Operand(val)); }
    RetInst* ASTCodeGen::createRetInst(float val) { return new RetInst(DataType::F32, getImmeF32Operand(val)); }

    BrCondInst* ASTCodeGen::createBranchInst(size_t condReg, size_t trueTar, size_t falseTar)
    {
        return new BrCondInst(getRegOperand(condReg), getLabelOperand(trueTar), getLabelOperand(falseTar));
    }
    BrUncondInst* ASTCodeGen::createBranchInst(size_t tar) { return new BrUncondInst(getLabelOperand(tar)); }

    AllocaInst* ASTCodeGen::createAllocaInst(DataType t, size_t ptrReg)
    {
        return new AllocaInst(t, getRegOperand(ptrReg));
    }
    AllocaInst* ASTCodeGen::createAllocaInst(DataType t, size_t ptrReg, std::vector<int> dims)
    {
        return new AllocaInst(t, getRegOperand(ptrReg), dims);
    }

    std::list<Instruction*> ASTCodeGen::createTypeConvertInst(DataType from, DataType to, size_t srcReg)
    {
        if (from == to) return {};
        ASSERT((from == DataType::I1) || (from == DataType::I32) || (from == DataType::F32));
        ASSERT((to == DataType::I1) || (to == DataType::I32) || (to == DataType::F32));

        std::list<Instruction*> insts;

        switch (from)
        {
            case DataType::I1:
            {
                switch (to)
                {
                    case DataType::I32:
                    {
                        size_t    destReg = getNewRegId();
                        ZextInst* zext    = createZextInst(srcReg, destReg, 1, 32);
                        insts.push_back(zext);
                        break;
                    }
                    case DataType::F32:
                    {
                        size_t    i32Reg = getNewRegId();
                        ZextInst* zext   = createZextInst(srcReg, i32Reg, 1, 32);
                        insts.push_back(zext);
                        size_t f32Reg = getNewRegId();
                        insts.push_back(createSI2FPInst(i32Reg, f32Reg));
                        break;
                    }
                    default: ERROR("Type conversion not supported");
                }
                break;
            }
            case DataType::I32:
            {
                switch (to)
                {
                    case DataType::I1:
                    {
                        size_t    destReg = getNewRegId();
                        IcmpInst* icmp    = createIcmpInst_ImmeRight(ICmpOp::NE, srcReg, 0, destReg);
                        insts.push_back(icmp);
                        break;
                    }
                    case DataType::F32:
                    {
                        size_t destReg = getNewRegId();
                        insts.push_back(createSI2FPInst(srcReg, destReg));
                        break;
                    }
                    default: ERROR("Type conversion not supported");
                }
                break;
            }
            case DataType::F32:
            {
                switch (to)
                {
                    case DataType::I1:
                    {
                        size_t    destReg = getNewRegId();
                        FcmpInst* fcmp    = createFcmpInst_ImmeRight(FCmpOp::ONE, srcReg, 0.0f, destReg);
                        insts.push_back(fcmp);
                        break;
                    }
                    case DataType::I32:
                    {
                        size_t destReg = getNewRegId();
                        insts.push_back(createFP2SIInst(srcReg, destReg));
                        break;
                    }
                    default: ERROR("Type conversion not supported");
                }
                break;
            }
            default: ERROR("Type conversion not supported");
        }

        return insts;
    }
}  // namespace ME
