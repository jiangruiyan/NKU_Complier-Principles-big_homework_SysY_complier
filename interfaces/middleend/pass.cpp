#include <middleend/pass.h>
#include <middleend/module/ir_module.h>
#include <middleend/module/ir_function.h>

namespace ME
{
    void FunctionPass::runOnModule(Module& module)
    {
        for (auto* function : module.functions) runOnFunction(*function);
    }
}  // namespace ME
