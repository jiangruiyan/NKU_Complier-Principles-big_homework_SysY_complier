#include <middleend/visitor/printer/module_printer.h>

namespace ME
{
    void IRPrinter::visit(Module& module, std::ostream& os)
    {
        os << "; Function Declarations\n";
        for (auto& fdecl : module.funcDecls)
        {
            apply(*this, *fdecl, os);
            if (&fdecl != &module.funcDecls.back()) os << "\n";
        }
        os << "\n\n";

        os << "; Global Variable Declarations\n";
        for (auto& gdef : module.globalVars)
        {
            apply(*this, *gdef, os);
            if (&gdef != &module.globalVars.back()) os << "\n";
        }
        os << "\n\n";

        os << "; Function Definitions\n";
        for (auto& func : module.functions)
        {
            apply(*this, *func, os);
            if (&func != &module.functions.back()) os << "\n";
        }
    }
    void IRPrinter::visit(Function& func, std::ostream& os)
    {
        apply(*this, *func.funcDef, os);
        os << "\n{\n";
        for (auto& [id, block] : func.blocks) apply(*this, *block, os);
        os << "}\n";
    }
    void IRPrinter::visit(Block& block, std::ostream& os)
    {
        os << "Block" << block.blockId << ":" << block.getComment() << "\n";
        for (auto& inst : block.insts)
        {
            os << "\t";
            apply(*this, *inst, os);
            os << "\n";
        }
    }
}  // namespace ME
