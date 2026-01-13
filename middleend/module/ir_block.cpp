#include <middleend/module/ir_block.h>

namespace ME
{
    Block::~Block()
    {
        for (auto inst : insts)
        {
            delete inst;
            inst = nullptr;
        }
        insts.clear();
    }

    void Block::insertFront(Instruction* inst) { insts.push_front(inst); }
    void Block::insertBack(Instruction* inst) { insts.push_back(inst); }
}  // namespace ME
