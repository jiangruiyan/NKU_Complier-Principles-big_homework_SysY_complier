#include <middleend/pass/mem2reg.h>

#include <middleend/pass/analysis/analysis_manager.h>
#include <middleend/pass/analysis/cfg.h>
#include <middleend/pass/analysis/dominfo.h>
#include <middleend/module/ir_operand.h>
#include <unordered_map>
#include <unordered_set>
#include <queue>

using namespace std;

namespace ME
{
    // 检查给定的操作数op是否是一个寄存器操作数，并将寄存器号输出到outReg
    bool Mem2RegPass::isRegOperand(Operand* op, size_t& outReg)
    {
        if (!op) return false;
        if (auto* ro = dynamic_cast<RegOperand*>(op))
        {
            outReg = ro->regNum;
            return true;
        }
        return false;
    }

    // 检查给定的操作数op是否是指定寄存器号reg
    bool Mem2RegPass::isSameReg(Operand* op, size_t reg)
    {
        size_t r = (size_t)-1;
        return isRegOperand(op, r) && r == reg;
    }

    // 获取指定数据类型的默认初始值操作数
    static inline Operand* defaultValueFor(DataType ty)
    {
        switch (ty)
        {
            case DataType::F32: return getImmeF32Operand(0.0f);
            default: return getImmeI32Operand(0);
        }
    }

    void Mem2RegPass::replaceRegUseInInst(Instruction* inst, size_t fromReg, Operand* toOp)
    {
        if (!inst || !toOp) return;

        auto repl = [&](Operand*& slot) {
            if (!slot) return;
            size_t r = (size_t)-1;
            if (isRegOperand(slot, r) && r == fromReg) slot = toOp;
        };

        // 注意：不要替换“定义”的 res 位置，仅替换“使用”的操作数
        if (auto* I = dynamic_cast<LoadInst*>(inst))
        {
            // ptr 是 use，res 是 def
            repl(I->ptr);
        }
        else if (auto* I = dynamic_cast<StoreInst*>(inst))
        {
            // val/ptr 全是 use
            repl(I->val);
            repl(I->ptr);
        }
        else if (auto* I = dynamic_cast<ArithmeticInst*>(inst))
        {
            // lhs/rhs 是 use，res 是 def
            repl(I->lhs);
            repl(I->rhs);
        }
        else if (auto* I = dynamic_cast<IcmpInst*>(inst))
        {
            repl(I->lhs);
            repl(I->rhs);
        }
        else if (auto* I = dynamic_cast<FcmpInst*>(inst))
        {
            repl(I->lhs);
            repl(I->rhs);
        }
        else if (auto* I = dynamic_cast<ZextInst*>(inst)) { repl(I->src); }
        else if (auto* I = dynamic_cast<SI2FPInst*>(inst)) { repl(I->src); }
        else if (auto* I = dynamic_cast<FP2SIInst*>(inst)) { repl(I->src); }
        else if (auto* I = dynamic_cast<GEPInst*>(inst))
        {
            repl(I->basePtr);
            for (auto*& idx : I->idxs) repl(idx);
        }
        else if (auto* I = dynamic_cast<BrCondInst*>(inst)) { repl(I->cond); }
        else if (auto* I = dynamic_cast<BrUncondInst*>(inst)) { (void)I; }
        else if (auto* I = dynamic_cast<CallInst*>(inst))
        {
            for (auto& p : I->args) repl(p.second);
        }
        else if (auto* I = dynamic_cast<PhiInst*>(inst))
        {
            // incoming 的 value 是 use，label 键不变
            for (auto& kv : I->incomingVals) repl(kv.second);
        }
        else if (auto* I = dynamic_cast<RetInst*>(inst)) { repl(I->res); }
        else
        {
            (void)inst;
        }
    }

    void Mem2RegPass::replaceRegUsesInFunction(Function& func, size_t fromReg, Operand* toOp)
    {
        for (auto& [bid, block] : func.blocks)
        {
            for (auto* inst : block->insts) { replaceRegUseInInst(inst, fromReg, toOp); }
        }
    }

    // Pass 入口
    void Mem2RegPass::runOnModule(Module& module)
    {
        for (auto* fn : module.functions) runOnFunction(*fn);
    }

    void Mem2RegPass::runOnFunction(Function& function) { promoteInFunction(function); }

    // 主流程
    void Mem2RegPass::promoteInFunction(Function& func)
    {
        auto* cfg = Analysis::AM.get<Analysis::CFG>(func);
        auto* dom = Analysis::AM.get<Analysis::DomInfo>(func);

        vector<VarInfo> vars; // 要提升的变量列表
        collectPromotableAllocas(cfg, vars);
        if (vars.empty()) return;

        insertPhi(func, cfg, dom, vars); // 插入phi节点
        renameAndCleanup(func, cfg, dom, vars); // 重命名并清理

        Analysis::AM.invalidate(func);
    }

    void Mem2RegPass::collectPromotableAllocas(Analysis::CFG* cfg, vector<VarInfo>& vars)
    {
        auto it0 = cfg->id2block.find(0);
        if (it0 == cfg->id2block.end()) return;
        Block* entry = it0->second;

        vector<AllocaInst*> candAllocas;
        for (auto* inst : entry->insts)
        {
            if (auto* AI = dynamic_cast<AllocaInst*>(inst))
            {
                if (!AI->dims.empty()) continue;
                candAllocas.push_back(AI);
            }
        }
        if (candAllocas.empty()) return;

        for (auto* AI : candAllocas)
        {
            size_t ptrReg = 0;
            if (!isRegOperand(AI->res, ptrReg)) continue;

            bool     promotable = true;
            set<int> defBlocks;

            for (auto& [bid, block] : cfg->id2block)
            {
                for (auto* inst : block->insts)
                {
                    if (auto* LI = dynamic_cast<LoadInst*>(inst))
                    {
                        if (isSameReg(LI->ptr, ptrReg)) { /* ok */ }
                    }
                    else if (auto* SI = dynamic_cast<StoreInst*>(inst))
                    {
                        if (isSameReg(SI->ptr, ptrReg)) { defBlocks.insert((int)bid); }
                    }
                    else if (auto* GI = dynamic_cast<GEPInst*>(inst))
                    {
                        if (isSameReg(GI->basePtr, ptrReg))
                        {
                            promotable = false;
                            break;
                        }
                    }
                    else if (auto* CI = dynamic_cast<CallInst*>(inst))
                    {
                        for (auto& p : CI->args)
                        {
                            if (isSameReg(p.second, ptrReg))
                            {
                                promotable = false;
                                break;
                            }
                        }
                        if (!promotable) break;
                    }
                    else
                    {
                        bool bad      = false;
                        auto checkUse = [&](Operand* op) {
                            size_t r = (size_t)-1;
                            if (isRegOperand(op, r) && r == ptrReg) bad = true;
                        };
                        if (auto* A = dynamic_cast<ArithmeticInst*>(inst))
                        {
                            checkUse(A->lhs);
                            checkUse(A->rhs);
                        }
                        else if (auto* IC = dynamic_cast<IcmpInst*>(inst))
                        {
                            checkUse(IC->lhs);
                            checkUse(IC->rhs);
                        }
                        else if (auto* FC = dynamic_cast<FcmpInst*>(inst))
                        {
                            checkUse(FC->lhs);
                            checkUse(FC->rhs);
                        }
                        else if (auto* Z = dynamic_cast<ZextInst*>(inst)) { checkUse(Z->src); }
                        else if (auto* C1 = dynamic_cast<SI2FPInst*>(inst)) { checkUse(C1->src); }
                        else if (auto* C2 = dynamic_cast<FP2SIInst*>(inst)) { checkUse(C2->src); }
                        else if (auto* PH = dynamic_cast<PhiInst*>(inst))
                        {
                            for (auto& kv : PH->incomingVals) checkUse(kv.second);
                        }
                        if (bad)
                        {
                            promotable = false;
                            break;
                        }
                    }
                }
                if (!promotable) break;
            }

            if (promotable && !defBlocks.empty())
            {
                VarInfo vi;
                vi.allocaInst = AI;
                vi.ty         = AI->dt;
                vi.ptrReg     = ptrReg;
                vi.defBlocks  = move(defBlocks);
                vars.push_back(move(vi));
            }
        }
    }

    void Mem2RegPass::insertPhi(Function& func, Analysis::CFG* cfg, Analysis::DomInfo* dom, vector<VarInfo>& vars)
    {
        /*
        取DomInfo的支配前沿DF，按“迭代支配前沿”算法：
            工作队列初始为defBlocks。
            对队列元素x的支配前沿DF[x]中的每个y：
                若y尚未插入过φ，则在块y首部插入一个PhiInst（新建res寄存器），记录到var.phiAtBlock[y]。
                若y本身不是定义块，将y加入工作队列，继续传播。
        */
        const auto& DF = dom->getDomFrontier();
        for (auto& var : vars)
        {
            unordered_set<int> hasPhi; // 已插入phi的块
            queue<int>         work;   // 待处理块队列
            unordered_set<int> inWork; // 正在队列中的块集合

            for (int b : var.defBlocks)
            {
                work.push(b);
                inWork.insert(b);
            }

            while (!work.empty())
            {
                int x = work.front();
                work.pop();
                inWork.erase(x);

                if (x < 0 || (size_t)x >= DF.size()) continue;
                for (int y : DF[x])
                {
                    if (hasPhi.count(y)) continue;

                    auto itBlock = cfg->id2block.find((size_t)y);
                    if (itBlock == cfg->id2block.end()) continue;
                    Block* B = itBlock->second;

                    size_t   newReg = func.getNewRegId();
                    Operand* resOp  = getRegOperand(newReg);

                    auto* phi = new PhiInst(var.ty, resOp);
                    // 将phi放在块首
                    B->insts.push_front(phi);

                    var.phiAtBlock[y] = phi;
                    hasPhi.insert(y);

                    if (!var.defBlocks.count(y) && !inWork.count(y))
                    {
                        work.push(y);
                        inWork.insert(y);
                    }
                }
            }
        }
    }

    void Mem2RegPass::renameAndCleanup(
        Function& func, Analysis::CFG* cfg, Analysis::DomInfo* dom, vector<VarInfo>& vars)
    {
        unordered_map<size_t, vector<Operand*>> stacks;
        stacks.reserve(vars.size());

        struct ToRemove
        {
            vector<LoadInst*>   loads;
            vector<StoreInst*>  stores;
            vector<AllocaInst*> allocas;
        } garbage;

        // 为每个变量的版本栈预置一个“默认值”，保证所有路径上都有可用值
        for (auto& var : vars)
        {
            Operand* def = defaultValueFor(var.ty);
            if (def) stacks[var.ptrReg].push_back(def);
        }

        auto getTop = [&](size_t varPtrReg) -> Operand* {
            auto it = stacks.find(varPtrReg);
            if (it == stacks.end() || it->second.empty()) return nullptr;
            return it->second.back();
        };
        auto pushVal = [&](size_t varPtrReg, Operand* v) { stacks[varPtrReg].push_back(v); };
        auto popN    = [&](size_t varPtrReg, size_t n) {
            auto it = stacks.find(varPtrReg);
            if (it == stacks.end()) return;
            auto& st = it->second;
            while (n-- && !st.empty()) st.pop_back();
        };

        // 构建块到phi指令的映射id → {varPtrReg → PhiInst}
        unordered_map<int, unordered_map<size_t, PhiInst*>> blockPhi;
        for (auto& var : vars)
        {
            for (auto& kv : var.phiAtBlock)
            {
                int b                   = kv.first;
                blockPhi[b][var.ptrReg] = kv.second;
            }
        }

        const auto& DomTree = dom->getDomTree();
        // 沿支配树遍历基本块，进行重命名和清理
        function<void(int)> dfs = [&](int bid) {
            auto itB = cfg->id2block.find((size_t)bid);
            if (itB == cfg->id2block.end()) return;
            Block* B = itB->second;

            unordered_map<size_t, size_t> pushedPhiCnt;
            if (auto it = blockPhi.find(bid); it != blockPhi.end())
            {
                for (auto& kv : it->second)
                {
                    size_t   varPtr = kv.first;
                    PhiInst* phi    = kv.second;
                    pushVal(varPtr, phi->res);
                    pushedPhiCnt[varPtr]++;
                }
            }

            unordered_map<size_t, size_t> pushedStoreCnt;

            for (auto* inst : B->insts)
            {
                if (auto* SI = dynamic_cast<StoreInst*>(inst))
                {
                    size_t ptrR = (size_t)-1;
                    if (!isRegOperand(SI->ptr, ptrR)) continue;

                    bool isPromoted = false;
                    for (auto& var : vars)
                    {
                        if (var.ptrReg == ptrR)
                        {
                            isPromoted = true;
                            break;
                        }
                    }
                    if (!isPromoted) continue;

                    pushVal(ptrR, SI->val);
                    pushedStoreCnt[ptrR]++;
                    garbage.stores.push_back(SI);
                }
                else if (auto* LI = dynamic_cast<LoadInst*>(inst))
                {
                    size_t ptrR = (size_t)-1;
                    if (!isRegOperand(LI->ptr, ptrR)) continue;

                    bool isPromoted = false;
                    for (auto& var : vars)
                    {
                        if (var.ptrReg == ptrR)
                        {
                            isPromoted = true;
                            break;
                        }
                    }
                    if (!isPromoted) continue;

                    Operand* cur = getTop(ptrR);
                    if (cur)
                    {
                        size_t defReg = 0;
                        if (isRegOperand(LI->res, defReg)) { replaceRegUsesInFunction(func, defReg, cur); }
                        garbage.loads.push_back(LI);
                    }
                }
            }

            if ((size_t)bid < cfg->G_id.size())
            {
                for (size_t succ : cfg->G_id[bid])
                {
                    auto itBPhi = blockPhi.find((int)succ);
                    if (itBPhi == blockPhi.end()) continue;

                    Operand* predLabel = getLabelOperand((size_t)bid);

                    for (auto& kv : itBPhi->second)
                    {
                        size_t   varPtr = kv.first;
                        PhiInst* PHI    = kv.second;
                        Operand* val    = getTop(varPtr);
                        if (!val) val = defaultValueFor(PHI->dt);
                        PHI->addIncoming(val, predLabel);
                    }
                }
            }

            if ((size_t)bid < DomTree.size())
            {
                for (int child : DomTree[bid]) dfs(child);
            }

            for (auto& kv : pushedStoreCnt) popN(kv.first, kv.second);
            for (auto& kv : pushedPhiCnt) popN(kv.first, kv.second);
        };

        // 从入口块 0 开始（若入口 id 不为 0，请改为 cfg 的入口标识）
        dfs(0);

        unordered_set<Instruction*> dead;
        dead.insert(garbage.loads.begin(), garbage.loads.end());
        dead.insert(garbage.stores.begin(), garbage.stores.end());

        for (auto& [bid, block] : cfg->id2block)
        {
            deque<Instruction*> rebuilt;
            rebuilt.resize(0);
            for (auto* inst : block->insts)
            {
                if (dead.count(inst)) continue;
                rebuilt.push_back(inst);
            }
            block->insts.swap(rebuilt);
        }

        auto it0 = cfg->id2block.find(0);
        if (it0 != cfg->id2block.end())
        {
            Block*                entry = it0->second;
            unordered_set<size_t> promotedPtrRegs;
            for (auto& v : vars) promotedPtrRegs.insert(v.ptrReg);

            deque<Instruction*> rebuilt;
            for (auto* inst : entry->insts)
            {
                if (auto* AI = dynamic_cast<AllocaInst*>(inst))
                {
                    size_t pr = (size_t)-1;
                    if (isRegOperand(AI->res, pr) && promotedPtrRegs.count(pr))
                    {
                        bool stillUsed = false;
                        for (auto& [bbid, block] : cfg->id2block)
                        {
                            for (auto* I : block->insts)
                            {
                                size_t tmp = (size_t)-1;
                                if (auto* L = dynamic_cast<LoadInst*>(I))
                                {
                                    if (isRegOperand(L->ptr, tmp) && tmp == pr)
                                    {
                                        stillUsed = true;
                                        break;
                                    }
                                }
                                else if (auto* S = dynamic_cast<StoreInst*>(I))
                                {
                                    if (isRegOperand(S->ptr, tmp) && tmp == pr)
                                    {
                                        stillUsed = true;
                                        break;
                                    }
                                }
                                else if (auto* G = dynamic_cast<GEPInst*>(I))
                                {
                                    if (isRegOperand(G->basePtr, tmp) && tmp == pr)
                                    {
                                        stillUsed = true;
                                        break;
                                    }
                                }
                                else if (auto* C = dynamic_cast<CallInst*>(I))
                                {
                                    for (auto& p : C->args)
                                    {
                                        if (isRegOperand(p.second, tmp) && tmp == pr)
                                        {
                                            stillUsed = true;
                                            break;
                                        }
                                    }
                                }
                                else if (auto* P = dynamic_cast<PhiInst*>(I))
                                {
                                    for (auto& kv : P->incomingVals)
                                    {
                                        if (isRegOperand(kv.second, tmp) && tmp == pr)
                                        {
                                            stillUsed = true;
                                            break;
                                        }
                                    }
                                }
                                if (stillUsed) break;
                            }
                            if (stillUsed) break;
                        }
                        if (!stillUsed) { continue; }
                    }
                }
                rebuilt.push_back(inst);
            }
            entry->insts.swap(rebuilt);
        }
    }

}  // namespace ME