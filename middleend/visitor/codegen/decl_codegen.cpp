#include <middleend/visitor/codegen/ast_codegen.h>
#include <debug.h>
#include <algorithm>
#include <functional>

namespace
{
    // 用于统计“初始化器树”中叶子标量元素的个数
    // 也就是把可能带有多层花括号的初始化器（InitializerList）视为一棵树
    // 递归求和其所有叶子 Initializer的数量
    size_t countInitializerElements(FE::AST::InitDecl* init)
    {
        if (!init) return 0;
        if (auto* list = dynamic_cast<FE::AST::InitializerList*>(init))
        {
            if (!list->init_list) return 0;
            size_t total = 0;
            for (auto* sub : *list->init_list) total += countInitializerElements(sub);
            return total;
        }
        return 0;
    }
}  // namespace 匿名命名空间

namespace ME
{
    void ASTCodeGen::visit(FE::AST::Initializer& node, Module* m)
    {
        (void)m;
        ERROR("Initializer should not appear here, at line %d", node.line_num);
    }
    void ASTCodeGen::visit(FE::AST::InitializerList& node, Module* m)
    {
        (void)m;
        ERROR("InitializerList should not appear here, at line %d", node.line_num);
    }
    void ASTCodeGen::visit(FE::AST::VarDeclarator& node, Module* m)
    {
        (void)m;
        ERROR("VarDeclarator should not appear here, at line %d", node.line_num);
    }
    void ASTCodeGen::visit(FE::AST::ParamDeclarator& node, Module* m)
    {
        (void)m;
        ERROR("ParamDeclarator should not appear here, at line %d", node.line_num);
    }

    void ASTCodeGen::visit(FE::AST::VarDeclaration& node, Module* m)
    {
        // TODO(Lab 3-2): 生成变量声明 IR（alloca、数组零初始化、可选初始化表达式）
        // (void)node;
        // (void)m;
        // TODO("Lab3-2: Implement VarDeclaration IR generation");
        if (!node.decls) return;

        DataType baseType = convert(node.type);
        // 匿名函数：在为局部数组分配栈内存（alloca）后，统一将未显式初始化的元素置零
        auto zeroInitArray = [&](size_t ptrReg, const std::vector<int>& dims) {
            int elemCnt = 1;
            for (int d : dims)
            {
                elemCnt *= d;
            }
            if (elemCnt == 0) return;
            int elemBytes  = (baseType == DataType::F32 || baseType == DataType::I32) ? 4 : 1;
            int totalBytes = elemCnt * elemBytes;
            CallInst::argList args = {{DataType::PTR, getRegOperand(ptrReg)},
                {DataType::I8, getImmeI32Operand(0)},
                {DataType::I32, getImmeI32Operand(totalBytes)},
                {DataType::I1, getImmeI32Operand(0)}};
            insert(createCallInst(DataType::VOID,
                "llvm.memset.p0.i32",
                args));  // 相当于调用 memset 函数清零数组（memset(ptr, 0, totalBytes)）
        };

        // 保存当前块，因为alloca需要插入到入口块
        Block* savedBlock = curBlock;
        Block* entryBlock = curFunc->getBlock(0);

        for (auto* decl : *node.decls) // 遍历每个变量声明
        {
            if (!decl) continue;

            auto* lval = dynamic_cast<FE::AST::LeftValExpr*>(decl->lval);

            FE::AST::VarAttr attr(node.type, node.isConstDecl, -1);
            if (lval->indices) // 数组维度处理
            {
                for (auto* idx : *lval->indices)
                {
                    if (!idx) continue;
                    int dim = 0;
                    if (idx->attr.val.value.type != FE::AST::voidType)
                        dim = idx->attr.val.getInt();
                    else if (auto* lit = dynamic_cast<FE::AST::LiteralExpr*>(idx))
                        dim = lit->literal.getInt();
                    attr.arrayDims.push_back(dim);
                }
            }

            size_t ptrReg = getNewRegId();

            // 将alloca指令插入到入口块的开头（在所有非alloca指令之前）
            enterBlock(entryBlock);
            // 找到入口块中第一个非alloca指令的位置
            auto insertPos = entryBlock->insts.begin();
            while (insertPos != entryBlock->insts.end() && dynamic_cast<AllocaInst*>(*insertPos) != nullptr)
            {
                ++insertPos;
            }

            AllocaInst* allocaInst;
            if (attr.arrayDims.empty())
                allocaInst = createAllocaInst(baseType, ptrReg);
            else
                allocaInst = createAllocaInst(baseType, ptrReg, attr.arrayDims);

            entryBlock->insts.insert(insertPos, allocaInst);

            // 恢复到原来的块进行初始化
            enterBlock(savedBlock);

            size_t initElemCnt = decl->init ? countInitializerElements(decl->init) : 0;
            normalizeArrayDims(attr, initElemCnt);

            name2reg.addSymbol(lval->entry, ptrReg);
            reg2attr[ptrReg] = attr;

            // 数组先整体清零，便于处理未显式初始化的元素
            if (!attr.arrayDims.empty()) zeroInitArray(ptrReg, attr.arrayDims);

            if (decl->init) // 有初始化表达式
            {
                if (auto* init = dynamic_cast<FE::AST::Initializer*>(decl->init)) // 单一初始化表达式
                {
                    Operand* dstPtr = getRegOperand(ptrReg);
                    if (!attr.arrayDims.empty())
                    {
                        // 单一初始化表达式赋值给数组首元素
                        std::vector<Operand*> idxOps = {getImmeI32Operand(0), getImmeI32Operand(0)};
                        size_t gepReg = getNewRegId();
                        insert(createGEP_I32Inst(baseType, dstPtr, attr.arrayDims, idxOps, gepReg));
                        dstPtr = getRegOperand(gepReg);
                    }

                    apply(*this, *init->init_val, m); // 生成初始化表达式 IR
                    size_t valReg = getMaxReg(); // 取到刚生成的表达式结果所在的寄存器号
                    DataType srcType = convert(init->init_val->attr.val.value.type); // 获取源表达式类型
                    auto converts = createTypeConvertInst(srcType, baseType, valReg);
                    for (auto* inst : converts) insert(inst);
                    if (!converts.empty()) valReg = getMaxReg(); // 若发生了转换，更新 valReg 为转换后的新结果寄存器
                    insert(createStoreInst(baseType, valReg, dstPtr));
                }
                else if (auto* initList = dynamic_cast<FE::AST::InitializerList*>(decl->init)) // 初始化列表
                {
                    // 将初始化列表按行主序展开，并处理对齐和补齐
                    std::vector<FE::AST::ExprNode*> flatInits;
                    
                    // 递归展开初始化器树，支持对齐和补齐
                    std::function<void(FE::AST::InitDecl*, size_t)> flattenWithPadding = 
                        [&](FE::AST::InitDecl* it, size_t depth) {
                        if (!it) return;
                        
                        if (auto* single = dynamic_cast<FE::AST::Initializer*>(it))
                        {
                            flatInits.push_back(single->init_val);
                        }
                        else if (auto* subList = dynamic_cast<FE::AST::InitializerList*>(it))
                        {
                            if (!subList->init_list) return;
                            for (auto* sub : *subList->init_list)
                            {
                                if (!sub) continue;
                                
                                // 如果是嵌套的InitializerList，进行对齐和补齐
                                if (dynamic_cast<FE::AST::InitializerList*>(sub))
                                {
                                    // 计算子对象大小
                                    long long subObjSize = 1;
                                    if (depth + 1 < attr.arrayDims.size())
                                    {
                                        for (size_t i = depth + 1; i < attr.arrayDims.size(); ++i)
                                        {
                                            subObjSize *= attr.arrayDims[i];
                                        }
                                    }
                                    
                                    // 对齐到子对象边界
                                    if (subObjSize > 0)
                                    {
                                        size_t currentPos = flatInits.size();
                                        size_t alignedPos = ((currentPos + subObjSize - 1) / subObjSize) * subObjSize;
                                        
                                        // 补齐到对齐位置
                                        while (flatInits.size() < alignedPos)
                                        {
                                            flatInits.push_back(nullptr); // nullptr表示需要补0
                                        }
                                    }
                                    
                                    // 记录处理前的位置
                                    size_t beforeSize = flatInits.size();
                                    
                                    // 递归处理
                                    flattenWithPadding(sub, depth + 1);
                                    
                                    // 补齐到子对象完整大小
                                    if (subObjSize > 0)
                                    {
                                        size_t actualAdded = flatInits.size() - beforeSize;
                                        while (actualAdded < (size_t)subObjSize)
                                        {
                                            flatInits.push_back(nullptr);
                                            actualAdded++;
                                        }
                                    }
                                }
                                else
                                {
                                    // 普通元素，直接递归
                                    flattenWithPadding(sub, depth + 1);
                                }
                            }
                        }
                    };
                    
                    flattenWithPadding(initList, 0);

                    int totalElems = 1;
                    for (int d : attr.arrayDims)
                    {
                        totalElems *= d;
                    }

                    // 匿名函数：将单个元素存储到数组中
                    auto storeOne = [&](FE::AST::ExprNode* expr, int linearIdx) {
                        // 如果expr为nullptr，说明是补齐的0，数组已经通过memset清零，跳过
                        if (!expr) return;
                        
                        apply(*this, *expr, m); // 生成该元素初始化表达式 IR
                        size_t valReg = getMaxReg();
                        DataType srcType = convert(expr->attr.val.value.type);
                        if (srcType != baseType)
                        {
                            auto converts = createTypeConvertInst(srcType, baseType, valReg);
                            for (auto* inst : converts) insert(inst);
                            if (!converts.empty()) valReg = getMaxReg();
                        }

                        std::vector<Operand*> idxOps;
                        idxOps.reserve(attr.arrayDims.size() + 1);
                        idxOps.push_back(getImmeI32Operand(0));

                        // 计算多维数组下标
                        // 用行主序展开：对每一维计算 stride（后续维度乘积），把 linearIdx 依次除以 stride 得到该维索引，余数继续传给下一维。
                        // 将各维索引依次 push 到 idxOps。
                        int offset = linearIdx;
                        for (size_t dimIdx = 0; dimIdx < attr.arrayDims.size(); ++dimIdx)
                        {
                            int stride = 1;
                            for (size_t j = dimIdx + 1; j < attr.arrayDims.size(); ++j) stride *= attr.arrayDims[j];
                            int idxVal = stride ? offset / stride : 0;
                            offset = stride ? offset % stride : 0;
                            idxOps.push_back(getImmeI32Operand(idxVal));
                        }

                        // 生成 GEP 指令计算该元素地址，并生成 Store 指令存储该元素值
                        size_t gepReg = getNewRegId();
                        insert(createGEP_I32Inst(baseType, getRegOperand(ptrReg), attr.arrayDims, idxOps, gepReg));
                        insert(createStoreInst(baseType, valReg, getRegOperand(gepReg)));
                    };

                    int writeCnt = std::min(totalElems, static_cast<int>(flatInits.size()));
                    for (int i = 0; i < writeCnt; ++i) storeOne(flatInits[i], i);
                }
            }
        }
    }
}  // namespace ME
