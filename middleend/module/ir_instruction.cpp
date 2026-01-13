#include <middleend/module/ir_instruction.h>
#include <debug.h>
#include <numeric>
#include <sstream>

namespace ME
{
    std::string LoadInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = load " << dt << ", ptr " << ptr << getComment();
        return ss.str();
    }

    std::string StoreInst::toString() const
    {
        std::stringstream ss;
        ss << "store " << dt << " " << val << ", ptr " << ptr << getComment();
        return ss.str();
    }

    std::string ArithmeticInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = " << opcode << " " << dt << " " << lhs << ", " << rhs << getComment();
        return ss.str();
    }

    std::string IcmpInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = icmp " << cond << " " << dt << " " << lhs << ", " << rhs << getComment();
        return ss.str();
    }

    std::string FcmpInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = fcmp " << cond << " " << dt << " " << lhs << ", " << rhs << getComment();
        return ss.str();
    }

    std::string AllocaInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = alloca ";
        if (dims.empty()) { ss << dt << getComment(); }
        else
        {
            for (int dim : dims) ss << "[" << dim << " x ";
            ss << dt << std::string(dims.size(), ']') << getComment();
        }
        return ss.str();
    }

    std::string BrCondInst::toString() const
    {
        std::stringstream ss;
        ss << "br i1 " << cond << ", label " << trueTar << ", label " << falseTar << getComment();
        return ss.str();
    }

    std::string BrUncondInst::toString() const
    {
        std::stringstream ss;
        ss << "br label " << target << getComment();
        return ss.str();
    }

    void initArrayGlb(
        std::ostream& s, DataType type, const FE::AST::VarAttr& v, size_t dimDph, size_t beginPos, size_t endPos)
    {
        if (dimDph == 0)
        {
            bool allZero = true;
            for (auto& initVal : v.initList)
            {
                if (initVal.type == FE::AST::boolType || initVal.type == FE::AST::intType ||
                    initVal.type == FE::AST::llType)
                {
                    int iv = initVal.getInt();
                    if (iv != 0) allZero = false;
                }
                if (initVal.type == FE::AST::floatType)
                {
                    float fv = initVal.getFloat();
                    if (fv != 0.0f) allZero = false;
                }
                if (!allZero) break;
            }

            if (allZero)
            {
                for (size_t i = 0; i < v.arrayDims.size(); ++i) s << "[" << v.arrayDims[i] << " x ";
                s << type << std::string(v.arrayDims.size(), ']') << " zeroinitializer";
                return;
            }
        }

        if (beginPos == endPos)
        {
            switch (type)
            {
                case DataType::I1:
                case DataType::I32:
                case DataType::I64: s << type << " " << v.initList[beginPos].getInt(); break;
                case DataType::F32:
                    s << type << " 0x" << std::hex << FLOAT_TO_DOUBLE_BITS(v.initList[beginPos].getFloat()) << std::dec;
                    break;
                default: ERROR("Unsupported data type in global array init");
            }
            return;
        }

        for (size_t i = dimDph; i < v.arrayDims.size(); ++i) s << "[" << v.arrayDims[i] << " x ";
        s << type << std::string(v.arrayDims.size() - dimDph, ']') << " [";

        int step = std::accumulate(v.arrayDims.begin() + dimDph + 1, v.arrayDims.end(), 1, std::multiplies<int>());
        for (int i = 0; i < v.arrayDims[dimDph]; ++i)
        {
            if (i != 0) s << ",";
            initArrayGlb(s, type, v, dimDph + 1, beginPos + i * step, beginPos + (i + 1) * step - 1);
        }

        s << "]";
    }
    std::string GlbVarDeclInst::toString() const
    {
        std::stringstream ss;
        ss << "@" << name << " = global ";
        if (initList.arrayDims.empty())
        {
            ss << dt << " ";
            if (init)
                ss << init;
            else
                ss << "zeroinitializer";
        }
        else
        {
            size_t step = 1;
            for (int dim : initList.arrayDims) step *= dim;
            initArrayGlb(ss, dt, initList, 0, 0, step - 1);
        }
        ss << getComment();
        return ss.str();
    }

    std::string CallInst::toString() const
    {
        std::stringstream ss;
        if (retType != DataType::VOID) ss << res << " = ";
        ss << "call " << retType << " @" << funcName << "(";

        for (auto it = args.begin(); it != args.end(); ++it)
        {
            ss << it->first << " " << it->second;
            if (std::next(it) != args.end()) ss << ", ";
        }
        ss << ")" << getComment();
        return ss.str();
    }

    std::string RetInst::toString() const
    {
        std::stringstream ss;
        ss << "ret " << rt;
        if (res) ss << " " << res;
        ss << getComment();
        return ss.str();
    }

    std::string FuncDeclInst::toString() const
    {
        std::stringstream ss;
        ss << "declare " << retType << " @" << funcName << "(";
        for (auto it = argTypes.begin(); it != argTypes.end(); ++it)
        {
            ss << *it;
            if (std::next(it) != argTypes.end()) ss << ", ";
        }
        if (isVarArg) ss << ", ...";
        ss << ")" << getComment();
        return ss.str();
    }

    std::string FuncDefInst::toString() const
    {
        std::stringstream ss;
        ss << "define " << retType << " @" << funcName << "(";

        for (auto it = argRegs.begin(); it != argRegs.end(); ++it)
        {
            ss << it->first << " " << it->second;
            if (std::next(it) != argRegs.end()) ss << ", ";
        }
        ss << ")" << getComment();
        return ss.str();
    }

    std::string GEPInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = getelementptr ";
        if (dims.empty())
            ss << dt;
        else
        {
            for (int dim : dims) ss << "[" << dim << " x ";
            ss << dt << std::string(dims.size(), ']');
        }

        ss << ", ptr " << basePtr;
        for (auto& idx : idxs) ss << ", " << idxType << " " << idx;
        ss << getComment();
        return ss.str();
    }

    std::string SI2FPInst::toString() const
    {
        std::stringstream ss;
        ss << dest << " = sitofp i32 " << src << " to float" << getComment();
        return ss.str();
    }

    std::string FP2SIInst::toString() const
    {
        std::stringstream ss;
        ss << dest << " = fptosi float " << src << " to i32" << getComment();
        return ss.str();
    }

    std::string ZextInst::toString() const
    {
        std::stringstream ss;
        ss << dest << " = zext " << from << " " << src << " to " << to << getComment();
        return ss.str();
    }

    std::string PhiInst::toString() const
    {
        std::stringstream ss;
        ss << res << " = phi " << dt << " ";

        for (auto it = incomingVals.begin(); it != incomingVals.end(); ++it)
        {
            ss << "[ " << it->second << ", " << it->first << " ]";
            if (std::next(it) != incomingVals.end()) ss << ", ";
        }
        ss << getComment();
        return ss.str();
    }
}  // namespace ME
