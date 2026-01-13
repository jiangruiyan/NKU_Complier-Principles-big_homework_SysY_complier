#include <backend/targets/riscv64/passes/lowering/phi_elimination.h>
#include <debug.h>
#include <algorithm>
#include <set>

namespace BE::RV64::Passes::Lowering
{
    using namespace BE;
    using namespace BE::RV64;

    namespace
    {
        struct EdgeKey
        {
            uint32_t pred;
            uint32_t succ;

            bool operator<(const EdgeKey& other) const
            {
                if (pred != other.pred) return pred < other.pred;
                return succ < other.succ;
            }
        };

        static bool isBranchOp(Operator op)
        {
            switch (op)
            {
                case Operator::BEQ:
                case Operator::BNE:
                case Operator::BLT:
                case Operator::BGE:
                case Operator::BLTU:
                case Operator::BGEU:
                case Operator::BGT:
                case Operator::BLE:
                case Operator::BGTU:
                case Operator::BLEU:
                case Operator::JAL: return true;
                default: return false;
            }
        }

        static bool getBranchTarget(MInstruction* inst, uint32_t& out)
        {
            auto* ri = dynamic_cast<Instr*>(inst);
            if (!ri || !ri->use_label) return false;
            if (!isBranchOp(ri->op)) return false;
            if (ri->label.jmp_label < 0) return false;
            out = static_cast<uint32_t>(ri->label.jmp_label);
            return true;
        }

        static std::vector<uint32_t> getSuccessors(Block* block)
        {
            std::set<uint32_t> uniq;
            for (auto* inst : block->insts)
            {
                uint32_t target = 0;
                if (getBranchTarget(inst, target)) uniq.insert(target);
            }
            return std::vector<uint32_t>(uniq.begin(), uniq.end());
        }

        static std::deque<MInstruction*>::iterator findBranchTo(Block* block, uint32_t target)
        {
            for (auto it = block->insts.begin(); it != block->insts.end(); ++it)
            {
                uint32_t t = 0;
                if (getBranchTarget(*it, t) && t == target) return it;
            }
            return block->insts.end();
        }

        static std::vector<MoveInst*> buildParallelMoves(const std::vector<std::pair<Register, Operand*>>& copies)
        {
            std::map<Register, Operand*> pending;
            for (const auto& [dst, src] : copies)
            {
                if (!src) continue;
                if (auto* srcReg = dynamic_cast<RegOperand*>(src))
                {
                    if (srcReg->reg == dst) continue;
                }
                pending[dst] = src;
            }

            std::set<Register> dests;
            for (const auto& [dst, _] : pending) dests.insert(dst);

            std::vector<MoveInst*> moves;
            while (!pending.empty())
            {
                bool progress = false;
                for (auto it = pending.begin(); it != pending.end();)
                {
                    Register dst = it->first;
                    Operand* src = it->second;
                    auto*    srcReg = dynamic_cast<RegOperand*>(src);
                    bool     srcInDests = srcReg && dests.count(srcReg->reg);
                    if (!srcInDests)
                    {
                        moves.push_back(createMove(new RegOperand(dst), src));
                        dests.erase(dst);
                        it = pending.erase(it);
                        progress = true;
                    }
                    else
                    {
                        ++it;
                    }
                }
                if (progress) continue;

                auto it = pending.begin();
                Register dst = it->first;
                Operand* src = it->second;
                auto*    srcReg = dynamic_cast<RegOperand*>(src);
                if (!srcReg)
                {
                    moves.push_back(createMove(new RegOperand(dst), src));
                    dests.erase(dst);
                    pending.erase(it);
                    continue;
                }

                Register tmp = getVReg(dst.dt);
                moves.push_back(createMove(new RegOperand(tmp), src));
                it->second = new RegOperand(tmp);
            }
            return moves;
        }
    }  // namespace

    void PhiEliminationPass::runOnModule(BE::Module& module, const BE::Targeting::TargetInstrAdapter* adapter)
    {
        if (module.functions.empty()) return;
        for (auto* func : module.functions) runOnFunction(func, adapter);
    }

    void PhiEliminationPass::runOnFunction(BE::Function* func, const BE::Targeting::TargetInstrAdapter* adapter)
    {
        (void)adapter;
        if (!func || func->blocks.empty()) return;

        std::map<EdgeKey, std::vector<std::pair<Register, Operand*>>> edgeCopies;
        for (auto& [bid, block] : func->blocks)
        {
            for (auto* inst : block->insts)
            {
                auto* phi = dynamic_cast<PhiInst*>(inst);
                if (!phi) continue;

                for (auto& [predId, srcOp] : phi->incomingVals)
                {
                    if (!srcOp) continue;
                    if (func->blocks.find(predId) == func->blocks.end()) continue;
                    edgeCopies[{predId, bid}].push_back({phi->resReg, srcOp});
                }
            }
        }

        std::map<uint32_t, std::vector<uint32_t>> succs;
        for (auto& [bid, block] : func->blocks) succs[bid] = getSuccessors(block);

        uint32_t maxId = 0;
        for (auto& [bid, _] : func->blocks)
        {
            if (bid > maxId) maxId = bid;
        }
        uint32_t nextId = maxId + 1;

        for (auto& [edge, copies] : edgeCopies)
        {
            auto itPred = func->blocks.find(edge.pred);
            auto itSucc = func->blocks.find(edge.succ);
            if (itPred == func->blocks.end() || itSucc == func->blocks.end()) continue;

            auto moves = buildParallelMoves(copies);
            if (moves.empty()) continue;

            bool needSplit = succs[edge.pred].size() > 1;
            if (!needSplit)
            {
                auto it = findBranchTo(itPred->second, edge.succ);
                auto insertIt = (it == itPred->second->insts.end()) ? itPred->second->insts.end() : it;
                for (auto* mv : moves)
                {
                    insertIt = itPred->second->insts.insert(insertIt, mv);
                    ++insertIt;
                }
                continue;
            }

            auto it = findBranchTo(itPred->second, edge.succ);
            if (it == itPred->second->insts.end()) continue;
            auto* br = dynamic_cast<Instr*>(*it);
            if (!br) continue;

            uint32_t newId = nextId++;
            auto*    newBlock = new Block(newId);
            for (auto* mv : moves) newBlock->insts.push_back(mv);
            newBlock->insts.push_back(createJInst(Operator::JAL, PR::x0, Label(static_cast<int>(edge.succ))));
            func->blocks[newId] = newBlock;

            br->label.jmp_label = static_cast<int>(newId);
            br->label.lnum = newId;
        }

        for (auto& [bid, block] : func->blocks)
        {
            for (auto it = block->insts.begin(); it != block->insts.end();)
            {
                if (dynamic_cast<PhiInst*>(*it))
                {
                    MInstruction::delInst(*it);
                    it = block->insts.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
}  // namespace BE::RV64::Passes::Lowering
