#include <middleend/ir_defs.h>

std::ostream& operator<<(std::ostream& os, ME::DataType dt)
{
    switch (dt)
    {
#define X(name, str, val) \
    case ME::DataType::name: return os << #str;
        IR_DATATYPE
#undef X
        default: return os << "unknown";
    }
}

std::ostream& operator<<(std::ostream& os, ME::Operator op)
{
    switch (op)
    {
#define X(name, str, val) \
    case ME::Operator::name: return os << #str;
        IR_OPCODE
#undef X
        default: return os << "unknown";
    }
}

std::ostream& operator<<(std::ostream& os, ME::ICmpOp cop)
{
    switch (cop)
    {
#define X(name, str, val) \
    case ME::ICmpOp::name: return os << #str;
        IR_ICMP
#undef X
        default: return os << "unknown";
    }
}

std::ostream& operator<<(std::ostream& os, ME::FCmpOp cop)
{
    switch (cop)
    {
#define X(name, str, val) \
    case ME::FCmpOp::name: return os << #str;
        IR_FCMP
#undef X
        default: return os << "unknown";
    }
}
