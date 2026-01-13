#include <middleend/module/ir_module.h>

namespace ME
{
    Module::Module() : globalVars(), funcDecls(), functions() {}
    Module::~Module()
    {
        for (auto& p : globalVars)
        {
            delete p;
            p = nullptr;
        }
        for (auto& p : funcDecls)
        {
            delete p;
            p = nullptr;
        }
        for (auto& p : functions)
        {
            delete p;
            p = nullptr;
        }
    }
}  // namespace ME
