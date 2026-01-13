#include <backend/targets/riscv64/rv64_target.h>
#include <backend/target/registry.h>

#include <backend/targets/riscv64/isel/rv64_dag_isel.h>
#include <backend/targets/riscv64/isel/rv64_ir_isel.h>
#include <backend/targets/riscv64/passes/lowering/frame_lowering.h>
#include <backend/targets/riscv64/passes/lowering/stack_lowering.h>
#include <backend/targets/riscv64/passes/lowering/phi_elimination.h>
#include <backend/targets/riscv64/rv64_codegen.h>

#include <backend/common/cfg_builder.h>
#include <backend/ra/linear_scan.h>
#include <backend/targets/riscv64/rv64_reg_info.h>
#include <backend/targets/riscv64/rv64_instr_adapter.h>
#include <backend/targets/riscv64/rv64_defs.h>
#include <backend/dag/dag_builder.h>
#include <backend/targets/riscv64/dag/rv64_dag_legalize.h>
#include <backend/mir/m_defs.h>

#include <debug.h>

#include <map>
#include <vector>
#include <cstring>
namespace BE
{
    namespace RA
    {
        void setTargetInstrAdapter(const BE::Targeting::TargetInstrAdapter* adapter);
    }
}  // namespace BE

namespace BE::Targeting::RV64
{
    namespace
    {
        struct AutoRegister
        {
            AutoRegister()
            {
                BE::Targeting::TargetRegistry::registerTargetFactory("riscv64", []() { return new Target(); });
                BE::Targeting::TargetRegistry::registerTargetFactory("riscv", []() { return new Target(); });
                BE::Targeting::TargetRegistry::registerTargetFactory("rv64", []() { return new Target(); });
            }
        } s_auto_register;
    }  // namespace

    namespace
    {
        static bool isFloatType(BE::DataType* dt) { return dt == BE::F32 || dt == BE::F64; }

        static void lowerPseudoMoves(BE::Module& m)
        {
            for (auto* func : m.functions)
            {
                for (auto& kv : func->blocks)
                {
                    auto* block = kv.second;
                    for (auto it = block->insts.begin(); it != block->insts.end();)
                    {
                        auto* mv = dynamic_cast<BE::MoveInst*>(*it);
                        if (!mv || mv->dest->ot != BE::Operand::Type::REG) { ++it; continue; }

                        auto dstReg = static_cast<BE::RegOperand*>(mv->dest)->reg;
                        bool dstFlt = isFloatType(dstReg.dt);

                        std::vector<BE::MInstruction*> replace;
                        bool                            remove = false;

                        switch (mv->src->ot)
                        {
                            case BE::Operand::Type::REG:
                            {
                                auto srcReg = static_cast<BE::RegOperand*>(mv->src)->reg;
                                if (srcReg == dstReg) { remove = true; break; }

                                if (dstFlt)
                                {
                                    auto op = (dstReg.dt == BE::F32) ? BE::RV64::Operator::FMV_S : BE::RV64::Operator::FMV_D;
                                    replace.push_back(BE::RV64::createR2Inst(op, dstReg, srcReg));
                                }
                                else
                                {
                                    auto op = (dstReg.dt == BE::I32) ? BE::RV64::Operator::ADDIW : BE::RV64::Operator::ADDI;
                                    replace.push_back(BE::RV64::createIInst(op, dstReg, srcReg, 0));
                                }
                                break;
                            }
                            case BE::Operand::Type::IMMI32:
                            {
                                int imm = static_cast<BE::I32Operand*>(mv->src)->val;
                                if (dstFlt)
                                {
                                    BE::Register tmp = BE::getVReg(BE::I32);
                                    replace.push_back(BE::RV64::createUInst(BE::RV64::Operator::LI, tmp, imm));
                                    replace.push_back(BE::RV64::createR2Inst(BE::RV64::Operator::FMV_W_X, dstReg, tmp));
                                }
                                else
                                {
                                    replace.push_back(BE::RV64::createUInst(BE::RV64::Operator::LI, dstReg, imm));
                                }
                                break;
                            }
                            case BE::Operand::Type::IMMF32:
                            {
                                float fval = static_cast<BE::F32Operand*>(mv->src)->val;
                                int   bits = 0;
                                std::memcpy(&bits, &fval, sizeof(bits));
                                BE::Register tmp = BE::getVReg(BE::I32);
                                replace.push_back(BE::RV64::createUInst(BE::RV64::Operator::LI, tmp, bits));
                                replace.push_back(BE::RV64::createR2Inst(BE::RV64::Operator::FMV_W_X, dstReg, tmp));
                                break;
                            }
                            default: break;
                        }

                        if (replace.empty() && !remove) { ++it; continue; }

                        BE::MInstruction::delInst(*it);
                        it = block->insts.erase(it);
                        for (auto* inst : replace)
                        {
                            it = block->insts.insert(it, inst);
                            ++it;
                        }
                    }
                }
            }
        }

        static void runPreRAPasses(BE::Module& m, const BE::Targeting::TargetInstrAdapter* adapter)
        {
            BE::RV64::Passes::Lowering::FrameLoweringPass frameLowering;
            frameLowering.runOnModule(m);

            // 对实现了 mem2reg 优化的同学，还需完成 Phi Elimination
            BE::RV64::Passes::Lowering::PhiEliminationPass phiElim;
            phiElim.runOnModule(m, adapter);

            lowerPseudoMoves(m);
        }
        static void runRAPipeline(BE::Module& m, const BE::Targeting::RV64::RegInfo& regInfo)
        {
            // TODO("使用你实现的寄存器分配器进行寄存器分配");
            BE::RA::LinearScanRA ls;
            ls.allocate(m, regInfo);
        }
        static void runPostRAPasses(BE::Module& m)
        {
            BE::RV64::Passes::Lowering::StackLoweringPass stackLowering;
            stackLowering.runOnModule(m);
        }
    }  // namespace

    void Target::runPipeline(ME::Module* ir, BE::Module* backend, std::ostream* out)
    {
        static BE::Targeting::RV64::InstrAdapter s_adapter;
        static BE::Targeting::RV64::RegInfo      s_regInfo;
        BE::Targeting::setTargetInstrAdapter(&s_adapter);

        // TODO("选择一种 Instruction Selector 实现，并完成指令选择");
        // BE::RV64::DAGIsel isel(ir, backend, this);
        BE::RV64::IRIsel isel(ir, backend, this);
        isel.run();

        runPreRAPasses(*backend, &s_adapter);
        runRAPipeline(*backend, s_regInfo);
        runPostRAPasses(*backend);

        BE::RV64::CodeGen codegen(backend, *out);
        codegen.generateAssembly();
    }
}  // namespace BE::Targeting::RV64
