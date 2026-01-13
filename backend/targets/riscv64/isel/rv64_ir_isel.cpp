#include <backend/targets/riscv64/isel/rv64_ir_isel.h>
#include <backend/targets/riscv64/rv64_defs.h>
#include <backend/mir/m_defs.h>
#include <debug.h>
#include <transfer.h>

namespace BE::RV64
{
    namespace
    {
        thread_local BE::Function* s_cur_func  = nullptr;
        thread_local BE::Block*    s_cur_block = nullptr;

        static inline BE::DataType* mapType(ME::DataType t)
        {
            switch (t)
            {
                case ME::DataType::I1:
                case ME::DataType::I8:
                case ME::DataType::I32: return BE::I32;
                case ME::DataType::I64:
                case ME::DataType::PTR: return BE::I64;
                case ME::DataType::F32: return BE::F32;
                case ME::DataType::DOUBLE: return BE::F64;
                default: ERROR("Unsupported IR data type"); return BE::I32;
            }
        }

        static inline Operator selectLoadOp(BE::DataType* dt)
        {
            if (dt == BE::F32) return Operator::FLW;
            if (dt == BE::F64) return Operator::FLD;
            if (dt == BE::I64 || dt == BE::PTR) return Operator::LD;
            return Operator::LW;
        }

        static inline Operator selectStoreOp(BE::DataType* dt)
        {
            if (dt == BE::F32) return Operator::FSW;
            if (dt == BE::F64) return Operator::FSD;
            if (dt == BE::I64 || dt == BE::PTR) return Operator::SD;
            return Operator::SW;
        }

        static inline Register makeVReg(size_t id, BE::DataType* dt)
        {
            return Register(static_cast<uint32_t>(id), dt, true);
        }
    }  // namespace

    void IRIsel::runImpl() { apply(*this, *ir_module_); }

    void IRIsel::visit(ME::Module& module)
    {
        for (auto* gv : module.globalVars)
        {
            if (!gv) continue;

            auto* be_gv = new BE::GlobalVariable(mapType(gv->dt), gv->name);

            if (!gv->initList.arrayDims.empty())
            {
                be_gv->dims = gv->initList.arrayDims;
                be_gv->initVals.reserve(gv->initList.initList.size());

                for (auto& initVal : gv->initList.initList)
                {
                    if (be_gv->type == BE::F32)
                        be_gv->initVals.push_back(FLOAT_TO_INT_BITS(initVal.getFloat()));
                    else if (be_gv->type == BE::F64)
                        be_gv->initVals.push_back(static_cast<int>(DOUBLE_TO_LONG_BITS(initVal.getFloat())));
                    else if (be_gv->type == BE::I64 || be_gv->type == BE::PTR)
                        be_gv->initVals.push_back(static_cast<int>(initVal.getLL()));
                    else
                        be_gv->initVals.push_back(initVal.getInt());
                }
            }
            else if (gv->init)
            {
                switch (gv->init->getType())
                {
                    case ME::OperandType::IMMEI32:
                        be_gv->initVals.push_back(static_cast<ME::ImmeI32Operand*>(gv->init)->value);
                        break;
                    case ME::OperandType::IMMEF32:
                        be_gv->initVals.push_back(
                            FLOAT_TO_INT_BITS(static_cast<ME::ImmeF32Operand*>(gv->init)->value));
                        break;
                    default: ERROR("Unsupported global initializer operand");
                }
            }

            m_backend_module->globals.push_back(be_gv);
        }

        for (auto* func : module.functions) apply(*this, *func);
    }
    void IRIsel::visit(ME::Function& func)
    {
        BE::ensureVRegBase(static_cast<uint32_t>(func.getMaxReg() + 1));
        auto* m_func = new BE::Function(func.funcDef ? func.funcDef->funcName : "");
        m_backend_module->functions.push_back(m_func);
        s_cur_func  = m_func;
        s_cur_block = nullptr;

        for (auto& [label, ir_block] : func.blocks)
        {
            if (!ir_block) continue;
            uint32_t bid        = static_cast<uint32_t>(label);
            m_func->blocks[bid] = new BE::Block(bid);
        }

        if (func.funcDef && !func.blocks.empty())
        {
            uint32_t entryLabel = static_cast<uint32_t>(func.blocks.begin()->first);
            auto     entryIt    = s_cur_func->blocks.find(entryLabel);
            if (entryIt == s_cur_func->blocks.end() || !entryIt->second)
                ERROR("IR isel function entry block not initialized");

            auto* entryBlock = entryIt->second;
            auto  insertIt   = entryBlock->insts.begin();

            static const Register kIntArgRegs[]   = {PR::a0, PR::a1, PR::a2, PR::a3, PR::a4, PR::a5, PR::a6, PR::a7};
            static const Register kFloatArgRegs[] = {
                PR::fa0, PR::fa1, PR::fa2, PR::fa3, PR::fa4, PR::fa5, PR::fa6, PR::fa7};

            int iregCnt = 0;
            int fregCnt = 0;
            int stackArgCnt = 0;

            for (auto& [argType, argOp] : func.funcDef->argRegs)
            {
                if (!argOp || argOp->getType() != ME::OperandType::REG)
                    ERROR("Function argument must be a register operand");

                BE::DataType* dt      = mapType(argType);
                bool          isFloat = (dt == BE::F32 || dt == BE::F64);

                Register dst = makeVReg(argOp->getRegNum(), dt);

                if (isFloat)
                {
                    if (fregCnt < 8)
                    {
                        Register src = kFloatArgRegs[fregCnt];
                        insertIt     = entryBlock->insts.insert(
                            insertIt, createMove(new RegOperand(dst), new RegOperand(src), LOC_STR));
                        ++insertIt;
                        fregCnt++;
                    }
                    else
                    {
                        // Use negative frame index for incoming stack args
                        int argIdx = -(stackArgCnt + 1);  // -1, -2, -3, ...
                        s_cur_func->frameInfo.createIncomingArgObject(argIdx, stackArgCnt * 8);
                        auto* loadInst = createIInst(selectLoadOp(dt), dst, PR::sp, new BE::FrameIndexOperand(argIdx));
                        insertIt = entryBlock->insts.insert(insertIt, loadInst);
                        ++insertIt;
                        stackArgCnt++;
                    }
                }
                else
                {
                    if (iregCnt < 8)
                    {
                        Register src = kIntArgRegs[iregCnt];
                        insertIt     = entryBlock->insts.insert(
                            insertIt, createMove(new RegOperand(dst), new RegOperand(src), LOC_STR));
                        ++insertIt;
                        iregCnt++;
                    }
                    else
                    {
                        // Use negative frame index for incoming stack args
                        int argIdx = -(stackArgCnt + 1);  // -1, -2, -3, ...
                        s_cur_func->frameInfo.createIncomingArgObject(argIdx, stackArgCnt * 8);
                        auto* loadInst = createIInst(selectLoadOp(dt), dst, PR::sp, new BE::FrameIndexOperand(argIdx));
                        insertIt = entryBlock->insts.insert(insertIt, loadInst);
                        ++insertIt;
                        stackArgCnt++;
                    }
                }
            }
        }

        for (auto& [_, ir_block] : func.blocks)
        {
            if (ir_block) apply(*this, *ir_block);
        }
    }
    void IRIsel::visit(ME::Block& block)
    {
        if (!s_cur_func) ERROR("IR isel block visit without current function");
        auto it = s_cur_func->blocks.find(static_cast<uint32_t>(block.blockId));
        if (it == s_cur_func->blocks.end() || !it->second) ERROR("IR isel block not initialized");
        s_cur_block = it->second;
        for (auto* inst : block.insts) apply(*this, *inst);
    }

    void IRIsel::visit(ME::LoadInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel load without current block");

        if (!inst.res || inst.res->getType() != ME::OperandType::REG) ERROR("Load destination must be a register");
        auto* dstRegOp = static_cast<ME::RegOperand*>(inst.res);

        Register dst    = makeVReg(dstRegOp->getRegNum(), mapType(inst.dt));
        Operator loadOp = selectLoadOp(dst.dt);
        Register baseReg;

        switch (inst.ptr->getType())
        {
            case ME::OperandType::REG:
            {
                baseReg = makeVReg(inst.ptr->getRegNum(), BE::PTR);
                break;
            }
            case ME::OperandType::GLOBAL:
            {
                auto* gop = static_cast<ME::GlobalOperand*>(inst.ptr);
                baseReg   = getVReg(BE::PTR);
                Label symbolLabel(gop->name, false, true);
                s_cur_block->insts.push_back(createUInst(Operator::LA, baseReg, symbolLabel));
                break;
            }
            default: ERROR("Unsupported load pointer operand");
        }

        s_cur_block->insts.push_back(createIInst(loadOp, dst, baseReg, 0));
    }
    void IRIsel::visit(ME::StoreInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel store without current block");

        BE::DataType* valType = mapType(inst.dt);
        Register      valReg;

        switch (inst.val->getType())
        {
            case ME::OperandType::REG:
            {
                valReg = makeVReg(inst.val->getRegNum(), valType);
                break;
            }
            case ME::OperandType::IMMEI32:
            {
                auto* imm = static_cast<ME::ImmeI32Operand*>(inst.val);
                valReg    = getVReg(valType);
                s_cur_block->insts.push_back(createMove(new RegOperand(valReg), imm->value, LOC_STR));
                break;
            }
            case ME::OperandType::IMMEF32:
            {
                auto* imm = static_cast<ME::ImmeF32Operand*>(inst.val);
                valReg    = getVReg(valType);
                s_cur_block->insts.push_back(createMove(new RegOperand(valReg), imm->value, LOC_STR));
                break;
            }
            case ME::OperandType::GLOBAL:
            {
                auto* gop = static_cast<ME::GlobalOperand*>(inst.val);
                valReg    = getVReg(BE::PTR);
                Label symbolLabel(gop->name, false, true);
                s_cur_block->insts.push_back(createUInst(Operator::LA, valReg, symbolLabel));
                break;
            }
            default: ERROR("Unsupported store value operand");
        }

        Register baseReg;
        switch (inst.ptr->getType())
        {
            case ME::OperandType::REG:
            {
                baseReg = makeVReg(inst.ptr->getRegNum(), BE::PTR);
                break;
            }
            case ME::OperandType::GLOBAL:
            {
                auto* gop = static_cast<ME::GlobalOperand*>(inst.ptr);
                baseReg   = getVReg(BE::PTR);
                Label symbolLabel(gop->name, false, true);
                s_cur_block->insts.push_back(createUInst(Operator::LA, baseReg, symbolLabel));
                break;
            }
            default: ERROR("Unsupported store pointer operand");
        }

        s_cur_block->insts.push_back(createSInst(selectStoreOp(valType), valReg, baseReg, 0));
    }
    void IRIsel::visit(ME::ArithmeticInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel arithmetic without current block");

        if (!inst.res || inst.res->getType() != ME::OperandType::REG)
            ERROR("Arithmetic destination must be a register");

        BE::DataType* dstType = mapType(inst.dt);
        Register      dst     = makeVReg(inst.res->getRegNum(), dstType);

        auto materializeOperand = [&](ME::Operand* op, BE::DataType* dt) -> Register {
            switch (op->getType())
            {
                case ME::OperandType::REG: return makeVReg(op->getRegNum(), dt);
                case ME::OperandType::IMMEI32:
                {
                    auto*    imm = static_cast<ME::ImmeI32Operand*>(op);
                    Register reg = getVReg(dt);
                    s_cur_block->insts.push_back(createMove(new RegOperand(reg), imm->value, LOC_STR));
                    return reg;
                }
                case ME::OperandType::IMMEF32:
                {
                    auto*    imm = static_cast<ME::ImmeF32Operand*>(op);
                    Register reg = getVReg(dt);
                    s_cur_block->insts.push_back(createMove(new RegOperand(reg), imm->value, LOC_STR));
                    return reg;
                }
                case ME::OperandType::GLOBAL:
                {
                    auto*    gop = static_cast<ME::GlobalOperand*>(op);
                    Register reg = getVReg(BE::PTR);
                    Label    symbolLabel(gop->name, false, true);
                    s_cur_block->insts.push_back(createUInst(Operator::LA, reg, symbolLabel));
                    return reg;
                }
                default: ERROR("Unsupported arithmetic operand");
            }
        };

        bool isFloat = (dstType == BE::F32 || dstType == BE::F64);
        bool is32bit = (dstType == BE::I32);

        if (isFloat)
        {
            Register lhsReg = materializeOperand(inst.lhs, dstType);
            Register rhsReg = materializeOperand(inst.rhs, dstType);
            Operator op;
            switch (inst.opcode)
            {
                case ME::Operator::FADD: op = Operator::FADD_S; break;
                case ME::Operator::FSUB: op = Operator::FSUB_S; break;
                case ME::Operator::FMUL: op = Operator::FMUL_S; break;
                case ME::Operator::FDIV: op = Operator::FDIV_S; break;
                default: ERROR("Unsupported float arithmetic operator");
            }
            s_cur_block->insts.push_back(createRInst(op, dst, lhsReg, rhsReg));
            return;
        }

        auto getImmI32 = [](ME::Operand* op, int& out) -> bool {
            if (op->getType() != ME::OperandType::IMMEI32) return false;
            out = static_cast<ME::ImmeI32Operand*>(op)->value;
            return true;
        };

        int  lhsImm      = 0;
        int  rhsImm      = 0;
        bool lhsIsImm    = getImmI32(inst.lhs, lhsImm);
        bool rhsIsImm    = getImmI32(inst.rhs, rhsImm);
        bool commutative = (inst.opcode == ME::Operator::ADD || inst.opcode == ME::Operator::MUL ||
                            inst.opcode == ME::Operator::BITAND || inst.opcode == ME::Operator::BITXOR);

        ME::Operand* lhsOp = inst.lhs;
        ME::Operand* rhsOp = inst.rhs;
        if (lhsIsImm && !rhsIsImm && commutative)
        {
            ME::Operand* tmpOp = lhsOp;
            lhsOp              = rhsOp;
            rhsOp              = tmpOp;

            int tmpImm = lhsImm;
            lhsImm     = rhsImm;
            rhsImm     = tmpImm;

            bool tmpIsImm = lhsIsImm;
            lhsIsImm      = rhsIsImm;
            rhsIsImm      = tmpIsImm;
        }

        Operator op;
        switch (inst.opcode)
        {
            case ME::Operator::ADD: op = is32bit ? Operator::ADDW : Operator::ADD; break;
            case ME::Operator::SUB: op = is32bit ? Operator::SUBW : Operator::SUB; break;
            case ME::Operator::MUL: op = is32bit ? Operator::MULW : Operator::MUL; break;
            case ME::Operator::DIV: op = is32bit ? Operator::DIVW : Operator::DIV; break;
            case ME::Operator::MOD: op = is32bit ? Operator::REMW : Operator::REM; break;
            case ME::Operator::BITAND: op = Operator::AND; break;
            case ME::Operator::BITXOR: op = Operator::XOR; break;
            case ME::Operator::SHL: op = Operator::SLL; break;
            case ME::Operator::ASHR: op = Operator::SRA; break;
            case ME::Operator::LSHR: op = Operator::SRL; break;
            default: ERROR("Unsupported integer arithmetic operator");
        }
        
        // 一元负号：0 - x => sub[w] dst, x0, x
        if ((op == Operator::SUB || op == Operator::SUBW) && lhsIsImm && lhsImm == 0)
        {
            Register rhsReg = materializeOperand(rhsOp, dstType);
            s_cur_block->insts.push_back(createRInst(op, dst, PR::x0, rhsReg));
            return;
        }

        if (rhsIsImm)
        {
            Operator iop;
            bool     hasImmForm = true;

            switch (op)
            {
                case Operator::ADD: iop = is32bit ? Operator::ADDIW : Operator::ADDI; break;
                case Operator::SUB:
                    iop    = is32bit ? Operator::ADDIW : Operator::ADDI;
                    rhsImm = -rhsImm;
                    break;
                case Operator::AND: iop = Operator::ANDI; break;
                case Operator::XOR: iop = Operator::XORI; break;
                case Operator::SLL: iop = is32bit ? Operator::SLLIW : Operator::SLLI; break;
                case Operator::SRA: iop = is32bit ? Operator::SRAIW : Operator::SRAI; break;
                case Operator::SRL: iop = is32bit ? Operator::SRLIW : Operator::SRLI; break;
                default: hasImmForm = false; break;
            }

            if (hasImmForm)
            {
                Register lhsReg = materializeOperand(lhsOp, dstType);
                s_cur_block->insts.push_back(createIInst(iop, dst, lhsReg, rhsImm));
                return;
            }
        }

        Register lhsReg = materializeOperand(lhsOp, dstType);
        Register rhsReg = materializeOperand(rhsOp, dstType);
        s_cur_block->insts.push_back(createRInst(op, dst, lhsReg, rhsReg));
    }
    void IRIsel::visit(ME::IcmpInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel icmp without current block");

        if (!inst.res || inst.res->getType() != ME::OperandType::REG) ERROR("Icmp destination must be a register");

        BE::DataType* opType = mapType(inst.dt);
        Register      dst    = makeVReg(inst.res->getRegNum(), BE::I32);

        auto materializeOperand = [&](ME::Operand* op, BE::DataType* dt) -> Register {
            switch (op->getType())
            {
                case ME::OperandType::REG: return makeVReg(op->getRegNum(), dt);
                case ME::OperandType::IMMEI32:
                {
                    auto*    imm = static_cast<ME::ImmeI32Operand*>(op);
                    Register reg = getVReg(dt);
                    s_cur_block->insts.push_back(createMove(new RegOperand(reg), imm->value, LOC_STR));
                    return reg;
                }
                case ME::OperandType::GLOBAL:
                {
                    auto*    gop = static_cast<ME::GlobalOperand*>(op);
                    Register reg = getVReg(BE::PTR);
                    Label    symbolLabel(gop->name, false, true);
                    s_cur_block->insts.push_back(createUInst(Operator::LA, reg, symbolLabel));
                    return reg;
                }
                default: ERROR("Unsupported icmp operand");
            }
        };

        auto getImmI32 = [](ME::Operand* op, int& out) -> bool {
            if (op->getType() != ME::OperandType::IMMEI32) return false;
            out = static_cast<ME::ImmeI32Operand*>(op)->value;
            return true;
        };

        auto imm12 = [](int imm) -> bool { return imm >= -2048 && imm <= 2047; };

        ME::Operand* lhsOp    = inst.lhs;
        ME::Operand* rhsOp    = inst.rhs;
        int          rhsImm   = 0;
        int          lhsImm   = 0;
        bool         rhsIsImm = getImmI32(rhsOp, rhsImm);
        bool         lhsIsImm = getImmI32(lhsOp, lhsImm);

        if ((inst.cond == ME::ICmpOp::EQ || inst.cond == ME::ICmpOp::NE) && lhsIsImm && !rhsIsImm)
        {
            ME::Operand* tmpOp = lhsOp;
            lhsOp              = rhsOp;
            rhsOp              = tmpOp;

            int tmpImm = lhsImm;
            lhsImm     = rhsImm;
            rhsImm     = tmpImm;

            bool tmpIsImm = lhsIsImm;
            lhsIsImm      = rhsIsImm;
            rhsIsImm      = tmpIsImm;
        }

        bool isUnsigned = (inst.cond == ME::ICmpOp::UGT || inst.cond == ME::ICmpOp::UGE ||
                           inst.cond == ME::ICmpOp::ULT || inst.cond == ME::ICmpOp::ULE);

        auto zextIfNeeded = [&](Register reg) -> Register {
            if (!isUnsigned || opType != BE::I32) return reg;
            Register zextReg = getVReg(BE::I64);
            s_cur_block->insts.push_back(createR2Inst(Operator::ZEXT_W, zextReg, reg));
            return zextReg;
        };

        if (rhsIsImm && imm12(rhsImm))
        {
            Register lhsReg = materializeOperand(lhsOp, opType);
            lhsReg          = zextIfNeeded(lhsReg);

            auto imm12_checked_plus1 = [&](int v) -> bool { return v >= -2048 && v <= 2047; };

            switch (inst.cond)
            {
                case ME::ICmpOp::EQ:
                {
                    if (rhsImm == 0)
                        s_cur_block->insts.push_back(createIInst(Operator::SLTIU, dst, lhsReg, 1));
                    else
                    {
                        s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, lhsReg, rhsImm));
                        s_cur_block->insts.push_back(createIInst(Operator::SLTIU, dst, dst, 1));
                    }
                    return;
                }
                case ME::ICmpOp::NE:
                {
                    if (rhsImm == 0)
                        s_cur_block->insts.push_back(createRInst(Operator::SLTU, dst, PR::x0, lhsReg));
                    else
                    {
                        s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, lhsReg, rhsImm));
                        s_cur_block->insts.push_back(createRInst(Operator::SLTU, dst, PR::x0, dst));
                    }
                    return;
                }
                case ME::ICmpOp::SLT:
                    s_cur_block->insts.push_back(createIInst(Operator::SLTI, dst, lhsReg, rhsImm));
                    return;
                case ME::ICmpOp::ULT:
                    s_cur_block->insts.push_back(createIInst(Operator::SLTIU, dst, lhsReg, rhsImm));
                    return;

                // 新增：立即数形式的 <=、>=、>（有符号/无符号）
                case ME::ICmpOp::SLE:
                {
                    int plus1 = rhsImm + 1;
                    if (imm12_checked_plus1(plus1))
                    {
                        s_cur_block->insts.push_back(createIInst(Operator::SLTI, dst, lhsReg, plus1));  // n < (C+1)
                        return;
                    }
                    break;  // 回退到通用路径
                }
                case ME::ICmpOp::SGE:
                {
                    // !(n < C)
                    s_cur_block->insts.push_back(createIInst(Operator::SLTI, dst, lhsReg, rhsImm));
                    s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, dst, 1));
                    return;
                }
                case ME::ICmpOp::SGT:
                {
                    int plus1 = rhsImm + 1;
                    if (imm12_checked_plus1(plus1))
                    {
                        // !(n <= C) 等价于 n > C => !(n < C+1)
                        s_cur_block->insts.push_back(createIInst(Operator::SLTI, dst, lhsReg, plus1));
                        s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, dst, 1));
                        return;
                    }
                    break;
                }
                case ME::ICmpOp::ULE:
                {
                    int plus1 = rhsImm + 1;
                    if (imm12_checked_plus1(plus1))
                    {
                        s_cur_block->insts.push_back(
                            createIInst(Operator::SLTIU, dst, lhsReg, plus1));  // n < (C+1) (unsigned)
                        return;
                    }
                    break;
                }
                case ME::ICmpOp::UGE:
                {
                    s_cur_block->insts.push_back(createIInst(Operator::SLTIU, dst, lhsReg, rhsImm));
                    s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, dst, 1));
                    return;
                }
                case ME::ICmpOp::UGT:
                {
                    int plus1 = rhsImm + 1;
                    if (imm12_checked_plus1(plus1))
                    {
                        s_cur_block->insts.push_back(createIInst(Operator::SLTIU, dst, lhsReg, plus1));
                        s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, dst, 1));
                        return;
                    }
                    break;
                }

                default: break;
            }
        }

        Register lhsReg = materializeOperand(lhsOp, opType);
        Register rhsReg = materializeOperand(rhsOp, opType);
        if (isUnsigned)
        {
            lhsReg = zextIfNeeded(lhsReg);
            rhsReg = zextIfNeeded(rhsReg);
        }

        switch (inst.cond)
        {
            case ME::ICmpOp::EQ:
            {
                Register tmp = getVReg(opType);
                s_cur_block->insts.push_back(createRInst(Operator::XOR, tmp, lhsReg, rhsReg));
                s_cur_block->insts.push_back(createIInst(Operator::SLTIU, dst, tmp, 1));
                break;
            }
            case ME::ICmpOp::NE:
            {
                Register tmp = getVReg(opType);
                s_cur_block->insts.push_back(createRInst(Operator::XOR, tmp, lhsReg, rhsReg));
                s_cur_block->insts.push_back(createRInst(Operator::SLTU, dst, PR::x0, tmp));
                break;
            }
            case ME::ICmpOp::SGT: s_cur_block->insts.push_back(createRInst(Operator::SLT, dst, rhsReg, lhsReg)); break;
            case ME::ICmpOp::SGE:
            {
                Register tmp = getVReg(opType);
                s_cur_block->insts.push_back(createRInst(Operator::SLT, tmp, lhsReg, rhsReg));
                s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, tmp, 1));
                break;
            }
            case ME::ICmpOp::SLT: s_cur_block->insts.push_back(createRInst(Operator::SLT, dst, lhsReg, rhsReg)); break;
            case ME::ICmpOp::SLE:
            {
                Register tmp = getVReg(opType);
                s_cur_block->insts.push_back(createRInst(Operator::SLT, tmp, rhsReg, lhsReg));
                s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, tmp, 1));
                break;
            }
            case ME::ICmpOp::UGT: s_cur_block->insts.push_back(createRInst(Operator::SLTU, dst, rhsReg, lhsReg)); break;
            case ME::ICmpOp::UGE:
            {
                Register tmp = getVReg(opType);
                s_cur_block->insts.push_back(createRInst(Operator::SLTU, tmp, lhsReg, rhsReg));
                s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, tmp, 1));
                break;
            }
            case ME::ICmpOp::ULT: s_cur_block->insts.push_back(createRInst(Operator::SLTU, dst, lhsReg, rhsReg)); break;
            case ME::ICmpOp::ULE:
            {
                Register tmp = getVReg(opType);
                s_cur_block->insts.push_back(createRInst(Operator::SLTU, tmp, rhsReg, lhsReg));
                s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, tmp, 1));
                break;
            }
            default: ERROR("Unsupported integer comparison condition");
        }
    }
    void IRIsel::visit(ME::FcmpInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel fcmp without current block");

        if (!inst.res || inst.res->getType() != ME::OperandType::REG) ERROR("Fcmp destination must be a register");

        BE::DataType* opType = mapType(inst.dt);
        Register      dst    = makeVReg(inst.res->getRegNum(), BE::I32);

        auto materializeOperand = [&](ME::Operand* op) -> Register {
            switch (op->getType())
            {
                case ME::OperandType::REG: return makeVReg(op->getRegNum(), opType);
                case ME::OperandType::IMMEF32:
                {
                    auto*    imm = static_cast<ME::ImmeF32Operand*>(op);
                    Register reg = getVReg(opType);
                    s_cur_block->insts.push_back(createMove(new RegOperand(reg), imm->value, LOC_STR));
                    return reg;
                }
                default: ERROR("Unsupported fcmp operand");
            }
        };

        Register lhsReg = materializeOperand(inst.lhs);
        Register rhsReg = materializeOperand(inst.rhs);

        auto emitOrdered = [&](Register out) {
            Register lhsOrd = getVReg(BE::I32);
            Register rhsOrd = getVReg(BE::I32);
            s_cur_block->insts.push_back(createRInst(Operator::FEQ_S, lhsOrd, lhsReg, lhsReg));
            s_cur_block->insts.push_back(createRInst(Operator::FEQ_S, rhsOrd, rhsReg, rhsReg));
            s_cur_block->insts.push_back(createRInst(Operator::AND, out, lhsOrd, rhsOrd));
        };

        auto emitUnordered = [&](Register out) {
            Register ordered = getVReg(BE::I32);
            emitOrdered(ordered);
            s_cur_block->insts.push_back(createIInst(Operator::XORI, out, ordered, 1));
        };

        switch (inst.cond)
        {
            case ME::FCmpOp::OEQ:
                s_cur_block->insts.push_back(createRInst(Operator::FEQ_S, dst, lhsReg, rhsReg));
                break;
            case ME::FCmpOp::OGT:
                s_cur_block->insts.push_back(createRInst(Operator::FLT_S, dst, rhsReg, lhsReg));
                break;
            case ME::FCmpOp::OGE:
                s_cur_block->insts.push_back(createRInst(Operator::FLE_S, dst, rhsReg, lhsReg));
                break;
            case ME::FCmpOp::OLT:
                s_cur_block->insts.push_back(createRInst(Operator::FLT_S, dst, lhsReg, rhsReg));
                break;
            case ME::FCmpOp::OLE:
                s_cur_block->insts.push_back(createRInst(Operator::FLE_S, dst, lhsReg, rhsReg));
                break;
            case ME::FCmpOp::ONE:
            {
                s_cur_block->insts.push_back(createRInst(Operator::FEQ_S, dst, lhsReg, rhsReg));
                s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, dst, 1));
                Register ordered = getVReg(BE::I32);
                emitOrdered(ordered);
                s_cur_block->insts.push_back(createRInst(Operator::AND, dst, dst, ordered));
                break;
            }
            case ME::FCmpOp::ORD: emitOrdered(dst); break;
            case ME::FCmpOp::UEQ:
            {
                s_cur_block->insts.push_back(createRInst(Operator::FEQ_S, dst, lhsReg, rhsReg));
                Register unordered = getVReg(BE::I32);
                emitUnordered(unordered);
                s_cur_block->insts.push_back(createRInst(Operator::OR, dst, dst, unordered));
                break;
            }
            case ME::FCmpOp::UGT:
            {
                s_cur_block->insts.push_back(createRInst(Operator::FLT_S, dst, rhsReg, lhsReg));
                Register unordered = getVReg(BE::I32);
                emitUnordered(unordered);
                s_cur_block->insts.push_back(createRInst(Operator::OR, dst, dst, unordered));
                break;
            }
            case ME::FCmpOp::UGE:
            {
                s_cur_block->insts.push_back(createRInst(Operator::FLE_S, dst, rhsReg, lhsReg));
                Register unordered = getVReg(BE::I32);
                emitUnordered(unordered);
                s_cur_block->insts.push_back(createRInst(Operator::OR, dst, dst, unordered));
                break;
            }
            case ME::FCmpOp::ULT:
            {
                s_cur_block->insts.push_back(createRInst(Operator::FLT_S, dst, lhsReg, rhsReg));
                Register unordered = getVReg(BE::I32);
                emitUnordered(unordered);
                s_cur_block->insts.push_back(createRInst(Operator::OR, dst, dst, unordered));
                break;
            }
            case ME::FCmpOp::ULE:
            {
                s_cur_block->insts.push_back(createRInst(Operator::FLE_S, dst, lhsReg, rhsReg));
                Register unordered = getVReg(BE::I32);
                emitUnordered(unordered);
                s_cur_block->insts.push_back(createRInst(Operator::OR, dst, dst, unordered));
                break;
            }
            case ME::FCmpOp::UNE:
            {
                s_cur_block->insts.push_back(createRInst(Operator::FEQ_S, dst, lhsReg, rhsReg));
                s_cur_block->insts.push_back(createIInst(Operator::XORI, dst, dst, 1));
                Register unordered = getVReg(BE::I32);
                emitUnordered(unordered);
                s_cur_block->insts.push_back(createRInst(Operator::OR, dst, dst, unordered));
                break;
            }
            case ME::FCmpOp::UNO: emitUnordered(dst); break;
            default: ERROR("Unsupported float comparison condition");
        }
    }
    void IRIsel::visit(ME::AllocaInst& inst)
    {
        if (!s_cur_block || !s_cur_func) ERROR("IR isel alloca without current function/block");
        if (!inst.res || inst.res->getType() != ME::OperandType::REG) ERROR("Alloca destination must be a register");

        auto elemByteSize = [](ME::DataType t) -> int {
            switch (t)
            {
                case ME::DataType::I1:
                case ME::DataType::I8:
                case ME::DataType::I32: return 4;
                case ME::DataType::I64:
                case ME::DataType::PTR: return 8;
                case ME::DataType::F32: return 4;
                case ME::DataType::DOUBLE: return 8;
                default: return 4;
            }
        };

        size_t regId     = inst.res->getRegNum();
        int    elemSize  = elemByteSize(inst.dt);
        int    elemCount = 1;
        for (int d : inst.dims) elemCount *= d;

        s_cur_func->frameInfo.createLocalObject(regId, elemSize * elemCount, elemSize);

        Register dst   = makeVReg(regId, BE::PTR);
        auto* addrInst = createIInst(Operator::ADDI, dst, PR::sp, new BE::FrameIndexOperand(static_cast<int>(regId)));
        s_cur_block->insts.push_back(addrInst);
    }
    void IRIsel::visit(ME::BrCondInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel brcond without current block");

        if (!inst.trueTar || inst.trueTar->getType() != ME::OperandType::LABEL)
            ERROR("BrCond true target must be a label");
        if (!inst.falseTar || inst.falseTar->getType() != ME::OperandType::LABEL)
            ERROR("BrCond false target must be a label");

        auto* trueLabelOp  = static_cast<ME::LabelOperand*>(inst.trueTar);
        auto* falseLabelOp = static_cast<ME::LabelOperand*>(inst.falseTar);
        Label trueLabel(static_cast<int>(trueLabelOp->lnum));
        Label falseLabel(static_cast<int>(falseLabelOp->lnum));

        switch (inst.cond->getType())
        {
            case ME::OperandType::REG:
            {
                Register condReg = makeVReg(inst.cond->getRegNum(), BE::I32);
                s_cur_block->insts.push_back(createBInst(Operator::BNE, condReg, PR::x0, trueLabel));
                s_cur_block->insts.push_back(createJInst(Operator::JAL, PR::x0, falseLabel));
                break;
            }
            case ME::OperandType::IMMEI32:
            {
                auto* imm         = static_cast<ME::ImmeI32Operand*>(inst.cond);
                Label targetLabel = (imm->value != 0) ? trueLabel : falseLabel;
                s_cur_block->insts.push_back(createJInst(Operator::JAL, PR::x0, targetLabel));
                break;
            }
            default: ERROR("Unsupported brcond condition operand");
        }
    }
    void IRIsel::visit(ME::BrUncondInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel bruncond without current block");

        if (!inst.target || inst.target->getType() != ME::OperandType::LABEL) ERROR("BrUncond target must be a label");

        auto* targetLabelOp = static_cast<ME::LabelOperand*>(inst.target);
        Label targetLabel(static_cast<int>(targetLabelOp->lnum));
        s_cur_block->insts.push_back(createJInst(Operator::JAL, PR::x0, targetLabel));
    }
    void IRIsel::visit(ME::CallInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel call without current block");

        // Handle LLVM intrinsics by redirecting to C library functions
        std::string actualFuncName = inst.funcName;
        size_t actualArgCount = inst.args.size();
        
        // Map LLVM intrinsics to C library functions
        // LLVM intrinsics have extra parameters (isvolatile, alignment) that C library functions don't need
        if (inst.funcName.find("llvm.memset") == 0)
        {
            // llvm.memset.p0.i32(i8* dest, i8 val, i32 len, i1 isvolatile) -> memset(dest, val, len)
            actualFuncName = "memset";
            actualArgCount = 3;  // Only use first 3 arguments
        }
        else if (inst.funcName.find("llvm.memcpy") == 0)
        {
            // llvm.memcpy.p0.p0.i32(i8* dest, i8* src, i32 len, i1 isvolatile) -> memcpy(dest, src, len)
            actualFuncName = "memcpy";
            actualArgCount = 3;  // Only use first 3 arguments
        }
        else if (inst.funcName.find("llvm.memmove") == 0)
        {
            // llvm.memmove.p0.p0.i32(i8* dest, i8* src, i32 len, i1 isvolatile) -> memmove(dest, src, len)
            actualFuncName = "memmove";
            actualArgCount = 3;  // Only use first 3 arguments
        }

        // Normal function call handling (using actualFuncName instead of inst.funcName)
        // 先把所有参数计算到临时虚拟寄存器（不碰 a0..a7/fa0..fa7）
        struct ArgTmp
        {
            BE::DataType* dt;
            bool          isFloat;
            Register      tmp;
        };
        std::vector<ArgTmp> prepared;
        prepared.reserve(actualArgCount);

        auto materializeArg = [&](ME::Operand* op, BE::DataType* dt) -> Register {
            switch (op->getType())
            {
                case ME::OperandType::REG: return makeVReg(op->getRegNum(), dt);
                case ME::OperandType::IMMEI32:
                {
                    auto*    imm = static_cast<ME::ImmeI32Operand*>(op);
                    Register r   = getVReg(dt);
                    s_cur_block->insts.push_back(createMove(new RegOperand(r), imm->value, LOC_STR));
                    return r;
                }
                case ME::OperandType::IMMEF32:
                {
                    auto*    imm = static_cast<ME::ImmeF32Operand*>(op);
                    Register r   = getVReg(dt);
                    s_cur_block->insts.push_back(createMove(new RegOperand(r), imm->value, LOC_STR));
                    return r;
                }
                case ME::OperandType::GLOBAL:
                {
                    auto*    gop = static_cast<ME::GlobalOperand*>(op);
                    Label    symbolLabel(gop->name, false, true);
                    Register r = getVReg(BE::PTR);
                    s_cur_block->insts.push_back(createUInst(Operator::LA, r, symbolLabel));
                    // 若目标为浮点形参，后续会用 faX 从 r 搬过去；整数则直接用 aX 搬
                    return r;
                }
                default: ERROR("Unsupported call argument operand"); return PR::x0;
            }
        };

        // Only process actualArgCount arguments (for intrinsics, skip extra isvolatile/alignment params)
        for (size_t i = 0; i < actualArgCount && i < inst.args.size(); ++i)
        {
            auto& [argType, argOp] = inst.args[i];
            BE::DataType* dt      = mapType(argType);
            bool          isFloat = (dt == BE::F32 || dt == BE::F64);

            Register tmp = materializeArg(argOp, dt);
            // i32 参数在临时寄存器上做一次 32 位归一化，避免 64 位污染
            if (dt == BE::I32) { s_cur_block->insts.push_back(createIInst(Operator::ADDIW, tmp, tmp, 0)); }

            prepared.push_back({dt, isFloat, tmp});
        }

        // 统一搬到 a0..a7 / fa0..fa7，避免覆盖
        static const Register kIntArgRegs[]   = {PR::a0, PR::a1, PR::a2, PR::a3, PR::a4, PR::a5, PR::a6, PR::a7};
        static const Register kFloatArgRegs[] = {
            PR::fa0, PR::fa1, PR::fa2, PR::fa3, PR::fa4, PR::fa5, PR::fa6, PR::fa7};

        int iregCnt = 0;
        int fregCnt = 0;
        std::vector<ArgTmp> stackArgs;

        for (auto& a : prepared)
        {
            if (a.isFloat)
            {
                if (fregCnt < 8)
                {
                    Register dst = kFloatArgRegs[fregCnt];
                    s_cur_block->insts.push_back(createMove(new RegOperand(dst), new RegOperand(a.tmp), LOC_STR));
                }
                else
                {
                    stackArgs.push_back(a);
                }
                fregCnt++;
            }
            else
            {
                if (iregCnt < 8)
                {
                    Register dst = kIntArgRegs[iregCnt];
                    s_cur_block->insts.push_back(createMove(new RegOperand(dst), new RegOperand(a.tmp), LOC_STR));
                    if (a.dt == BE::I32)
                    {
                        s_cur_block->insts.push_back(createIInst(Operator::ADDIW, dst, dst, 0));
                    }
                }
                else
                {
                    stackArgs.push_back(a);
                }
                iregCnt++;
            }
        }

        // Store stack args at sp+0, sp+8, sp+16...
        for (size_t i = 0; i < stackArgs.size(); ++i)
        {
            int stackOffset = static_cast<int>(i * 8);
            if (stackOffset >= -2048 && stackOffset <= 2047)
            {
                s_cur_block->insts.push_back(createSInst(selectStoreOp(stackArgs[i].dt), stackArgs[i].tmp, PR::sp, stackOffset));
            }
            else
            {
                // Offset out of imm12 range
                // Use t6 as address temp to avoid conflict with stackArgs[i].tmp which might be allocated to t0-t5
                s_cur_block->insts.push_back(createUInst(Operator::LI, PR::t6, stackOffset));
                s_cur_block->insts.push_back(createRInst(Operator::ADD, PR::t6, PR::sp, PR::t6));
                s_cur_block->insts.push_back(createSInst(selectStoreOp(stackArgs[i].dt), stackArgs[i].tmp, PR::t6, 0));
            }
        }

        // Update the function's max outgoing arg area size
        if (!stackArgs.empty())
        {
            int requiredArgSpace = static_cast<int>(stackArgs.size() * 8);
            s_cur_func->frameInfo.setParamAreaSize(requiredArgSpace);
        }

        s_cur_block->insts.push_back(createCallInst(Operator::CALL, actualFuncName, iregCnt, fregCnt));

        if (inst.res)
        {
            if (inst.res->getType() != ME::OperandType::REG) ERROR("Call destination must be a register");
            BE::DataType* retType = mapType(inst.retType);
            Register      dst     = makeVReg(inst.res->getRegNum(), retType);
            Register      src     = (retType == BE::F32 || retType == BE::F64) ? PR::fa0 : PR::a0;
            s_cur_block->insts.push_back(createMove(new RegOperand(dst), new RegOperand(src), LOC_STR));
            if (retType == BE::I32) { s_cur_block->insts.push_back(createIInst(Operator::ADDIW, dst, dst, 0)); }
        }
    }
    void IRIsel::visit(ME::RetInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel ret without current block");

        if (inst.res)
        {
            BE::DataType* retType = mapType(inst.rt);
            Register      destReg = (retType == BE::F32 || retType == BE::F64) ? PR::fa0 : PR::a0;

            switch (inst.res->getType())
            {
                case ME::OperandType::REG:
                {
                    Register src = makeVReg(inst.res->getRegNum(), retType);
                    s_cur_block->insts.push_back(createMove(new RegOperand(destReg), new RegOperand(src), LOC_STR));
                    break;
                }
                case ME::OperandType::IMMEI32:
                {
                    auto* imm = static_cast<ME::ImmeI32Operand*>(inst.res);
                    s_cur_block->insts.push_back(createMove(new RegOperand(destReg), imm->value, LOC_STR));
                    break;
                }
                case ME::OperandType::IMMEF32:
                {
                    auto* imm = static_cast<ME::ImmeF32Operand*>(inst.res);
                    s_cur_block->insts.push_back(createMove(new RegOperand(destReg), imm->value, LOC_STR));
                    break;
                }
                case ME::OperandType::GLOBAL:
                {
                    auto* gop = static_cast<ME::GlobalOperand*>(inst.res);
                    Label symbolLabel(gop->name, false, true);
                    if (destReg.dt == BE::F32 || destReg.dt == BE::F64)
                    {
                        Register tmp = getVReg(BE::PTR);
                        s_cur_block->insts.push_back(createUInst(Operator::LA, tmp, symbolLabel));
                        s_cur_block->insts.push_back(createMove(new RegOperand(destReg), new RegOperand(tmp), LOC_STR));
                    }
                    else
                    {
                        s_cur_block->insts.push_back(createUInst(Operator::LA, destReg, symbolLabel));
                    }
                    break;
                }
                default: ERROR("Unsupported return operand type");
            }
        }

        s_cur_block->insts.push_back(createIInst(Operator::JALR, PR::x0, PR::ra, 0));
    }
    void IRIsel::visit(ME::GEPInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel gep without current block");
        if (!inst.res || inst.res->getType() != ME::OperandType::REG) ERROR("GEP destination must be a register");

        auto elemByteSize = [](ME::DataType t) -> int {
            switch (t)
            {
                case ME::DataType::I1:
                case ME::DataType::I8:
                case ME::DataType::I32: return 4;
                case ME::DataType::I64:
                case ME::DataType::PTR: return 8;
                case ME::DataType::F32: return 4;
                case ME::DataType::DOUBLE: return 8;
                default: return 4;
            }
        };

        auto imm12  = [](int64_t imm) -> bool { return imm >= -2048 && imm <= 2047; };
        auto isPow2 = [](int64_t v) -> bool { return v > 0 && (v & (v - 1)) == 0; };
        auto log2i  = [](int64_t v) -> int {
            int s = 0;
            while (v > 1)
            {
                v >>= 1;
                ++s;
            }
            return s;
        };

        Register baseReg;
        switch (inst.basePtr->getType())
        {
            case ME::OperandType::REG: baseReg = makeVReg(inst.basePtr->getRegNum(), BE::PTR); break;
            case ME::OperandType::GLOBAL:
            {
                auto* gop = static_cast<ME::GlobalOperand*>(inst.basePtr);
                baseReg   = getVReg(BE::PTR);
                Label symbolLabel(gop->name, false, true);
                s_cur_block->insts.push_back(createUInst(Operator::LA, baseReg, symbolLabel));
                break;
            }
            default: ERROR("Unsupported GEP base pointer operand");
        }

        Register dst = makeVReg(inst.res->getRegNum(), BE::PTR);

        if (inst.idxs.empty())
        {
            s_cur_block->insts.push_back(createMove(new RegOperand(dst), new RegOperand(baseReg), LOC_STR));
            return;
        }

        const int elemSize   = elemByteSize(inst.dt);
        const int dimCount   = static_cast<int>(inst.dims.size());
        const int indexCount = static_cast<int>(inst.idxs.size());

        auto strideForIndex = [&](int idxPos) -> int64_t {
            if (dimCount == 0) return 1;
            if (indexCount == dimCount + 1)
            {
                if (idxPos == 0)
                {
                    int64_t fullStride = 1;
                    for (int d : inst.dims) fullStride *= d;
                    return fullStride;
                }
                idxPos -= 1;
            }
            if (idxPos < 0 || idxPos >= dimCount) return 1;
            int64_t stride = 1;
            for (int i = idxPos + 1; i < dimCount; ++i) stride *= inst.dims[i];
            return stride;
        };

        int64_t  constOffset  = 0;
        bool     hasRegOffset = false;
        Register offsetReg;

        for (int i = 0; i < indexCount; ++i)
        {
            int64_t stride     = strideForIndex(i);
            int64_t byteStride = stride * elemSize;
            if (byteStride == 0) continue;

            auto* idx = inst.idxs[i];
            if (idx->getType() == ME::OperandType::IMMEI32)
            {
                auto* imm = static_cast<ME::ImmeI32Operand*>(idx);
                constOffset += static_cast<int64_t>(imm->value) * byteStride;
                continue;
            }

            Register idxReg;
            switch (idx->getType())
            {
                case ME::OperandType::REG: idxReg = makeVReg(idx->getRegNum(), mapType(inst.idxType)); break;
                default: ERROR("Unsupported GEP index operand");
            }

            Register idx64 = idxReg;
            if (idxReg.dt == BE::I32)
            {
                Register zextReg = getVReg(BE::I64);
                s_cur_block->insts.push_back(createR2Inst(Operator::ZEXT_W, zextReg, idxReg));
                idx64 = zextReg;
            }

            Register scaledReg = idx64;
            if (byteStride != 1)
            {
                if (isPow2(byteStride))
                {
                    Register shReg = getVReg(BE::I64);
                    s_cur_block->insts.push_back(createIInst(Operator::SLLI, shReg, idx64, log2i(byteStride)));
                    scaledReg = shReg;
                }
                else
                {
                    Register strideReg = getVReg(BE::I64);
                    s_cur_block->insts.push_back(
                        createMove(new RegOperand(strideReg), static_cast<int>(byteStride), LOC_STR));
                    Register mulReg = getVReg(BE::I64);
                    s_cur_block->insts.push_back(createRInst(Operator::MUL, mulReg, idx64, strideReg));
                    scaledReg = mulReg;
                }
            }

            if (!hasRegOffset)
            {
                offsetReg    = scaledReg;
                hasRegOffset = true;
            }
            else
            {
                Register sumReg = getVReg(BE::I64);
                s_cur_block->insts.push_back(createRInst(Operator::ADD, sumReg, offsetReg, scaledReg));
                offsetReg = sumReg;
            }
        }

        if (constOffset != 0)
        {
            if (hasRegOffset)
            {
                if (imm12(constOffset))
                {
                    s_cur_block->insts.push_back(
                        createIInst(Operator::ADDI, offsetReg, offsetReg, static_cast<int>(constOffset)));
                }
                else
                {
                    Register immReg = getVReg(BE::I64);
                    s_cur_block->insts.push_back(
                        createMove(new RegOperand(immReg), static_cast<int>(constOffset), LOC_STR));
                    Register sumReg = getVReg(BE::I64);
                    s_cur_block->insts.push_back(createRInst(Operator::ADD, sumReg, offsetReg, immReg));
                    offsetReg = sumReg;
                }
            }
            else
            {
                if (imm12(constOffset))
                {
                    s_cur_block->insts.push_back(
                        createIInst(Operator::ADDI, dst, baseReg, static_cast<int>(constOffset)));
                    return;
                }

                Register immReg = getVReg(BE::I64);
                s_cur_block->insts.push_back(
                    createMove(new RegOperand(immReg), static_cast<int>(constOffset), LOC_STR));
                s_cur_block->insts.push_back(createRInst(Operator::ADD, dst, baseReg, immReg));
                return;
            }
        }

        if (hasRegOffset)
            s_cur_block->insts.push_back(createRInst(Operator::ADD, dst, baseReg, offsetReg));
        else
            s_cur_block->insts.push_back(createMove(new RegOperand(dst), new RegOperand(baseReg), LOC_STR));
    }
    void IRIsel::visit(ME::FP2SIInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel fp2si without current block");

        if (!inst.dest || inst.dest->getType() != ME::OperandType::REG) ERROR("FP2SI destination must be a register");

        Register srcReg;
        switch (inst.src->getType())
        {
            case ME::OperandType::REG: srcReg = makeVReg(inst.src->getRegNum(), BE::F32); break;
            case ME::OperandType::IMMEF32:
            {
                auto* imm = static_cast<ME::ImmeF32Operand*>(inst.src);
                srcReg    = getVReg(BE::F32);
                s_cur_block->insts.push_back(createMove(new RegOperand(srcReg), imm->value, LOC_STR));
                break;
            }
            default: ERROR("Unsupported fp2si source operand");
        }

        Register dst = makeVReg(inst.dest->getRegNum(), BE::I32);
        s_cur_block->insts.push_back(createR2Inst(Operator::FCVT_W_S, dst, srcReg));
    }
    void IRIsel::visit(ME::SI2FPInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel si2fp without current block");

        if (!inst.dest || inst.dest->getType() != ME::OperandType::REG) ERROR("SI2FP destination must be a register");

        Register srcReg;
        switch (inst.src->getType())
        {
            case ME::OperandType::REG: srcReg = makeVReg(inst.src->getRegNum(), BE::I32); break;
            case ME::OperandType::IMMEI32:
            {
                auto* imm = static_cast<ME::ImmeI32Operand*>(inst.src);
                srcReg    = getVReg(BE::I32);
                s_cur_block->insts.push_back(createMove(new RegOperand(srcReg), imm->value, LOC_STR));
                break;
            }
            default: ERROR("Unsupported si2fp source operand");
        }

        Register dst = makeVReg(inst.dest->getRegNum(), BE::F32);
        s_cur_block->insts.push_back(createR2Inst(Operator::FCVT_S_W, dst, srcReg));
    }
    void IRIsel::visit(ME::ZextInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel zext without current block");

        if (!inst.dest || inst.dest->getType() != ME::OperandType::REG) ERROR("Zext destination must be a register");

        BE::DataType* srcType = mapType(inst.from);
        BE::DataType* dstType = mapType(inst.to);
        if (srcType == BE::F32 || srcType == BE::F64 || dstType == BE::F32 || dstType == BE::F64)
            ERROR("Zext only supports integer types");

        Register srcReg;
        bool     srcIsImm = false;
        int      immVal   = 0;
        switch (inst.src->getType())
        {
            case ME::OperandType::REG: srcReg = makeVReg(inst.src->getRegNum(), srcType); break;
            case ME::OperandType::IMMEI32:
            {
                auto* imm = static_cast<ME::ImmeI32Operand*>(inst.src);
                srcIsImm  = true;
                immVal    = imm->value;
                break;
            }
            default: ERROR("Unsupported zext source operand");
        }

        Register dst = makeVReg(inst.dest->getRegNum(), dstType);

        if (srcType == dstType)
        {
            if (srcIsImm)
                s_cur_block->insts.push_back(createMove(new RegOperand(dst), immVal, LOC_STR));
            else
                s_cur_block->insts.push_back(createMove(new RegOperand(dst), new RegOperand(srcReg), LOC_STR));
            return;
        }

        if (dstType == BE::I64 && srcType == BE::I32)
        {
            if (srcIsImm)
            {
                srcReg = getVReg(srcType);
                s_cur_block->insts.push_back(createMove(new RegOperand(srcReg), immVal, LOC_STR));
            }
            s_cur_block->insts.push_back(createR2Inst(Operator::ZEXT_W, dst, srcReg));
            return;
        }

        ERROR("Unsupported zext conversion");
    }
    void IRIsel::visit(ME::PhiInst& inst)
    {
        if (!s_cur_block) ERROR("IR isel phi without current block");

        if (!inst.res || inst.res->getType() != ME::OperandType::REG) ERROR("Phi destination must be a register");

        BE::DataType* dstType = mapType(inst.dt);
        Register      dst     = makeVReg(inst.res->getRegNum(), dstType);
        auto*         phiInst = new BE::PhiInst(dst);

        for (auto& [labelOp, valOp] : inst.incomingVals)
        {
            if (!labelOp || labelOp->getType() != ME::OperandType::LABEL)
                ERROR("Phi incoming label must be a label operand");
            if (!valOp) ERROR("Phi incoming value is null");

            auto*    label   = static_cast<ME::LabelOperand*>(labelOp);
            uint32_t labelId = static_cast<uint32_t>(label->lnum);

            BE::Operand* srcOp = nullptr;
            switch (valOp->getType())
            {
                case ME::OperandType::REG: srcOp = new RegOperand(makeVReg(valOp->getRegNum(), dstType)); break;
                case ME::OperandType::IMMEI32:
                    srcOp = new I32Operand(static_cast<ME::ImmeI32Operand*>(valOp)->value);
                    break;
                case ME::OperandType::IMMEF32:
                    srcOp = new F32Operand(static_cast<ME::ImmeF32Operand*>(valOp)->value);
                    break;
                default: ERROR("Unsupported phi incoming operand");
            }

            phiInst->incomingVals[labelId] = srcOp;
        }

        s_cur_block->insts.push_back(phiInst);
    }

    void IRIsel::visit(ME::GlbVarDeclInst& inst)
    {
        ERROR("Global variable declarations should not appear in IR during instruction selection.");
    }
    void IRIsel::visit(ME::FuncDeclInst& inst)
    {
        ERROR("Function declarations should not appear in IR during instruction selection.");
    }
    void IRIsel::visit(ME::FuncDefInst& inst)
    {
        ERROR("Function definitions should not appear in IR during instruction selection.");
    }
}  // namespace BE::RV64
