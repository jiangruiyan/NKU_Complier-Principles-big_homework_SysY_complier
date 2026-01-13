#include <middleend/module/ir_function.h>

namespace ME
{
    Function::Function(FuncDefInst* fd)
        : funcDef(fd), blocks(), maxLabel(0), maxReg(0), loopStartLabel(0), loopEndLabel(0)
    {}
    Function::~Function()
    {
        if (funcDef)
        {
            delete funcDef;
            funcDef = nullptr;
        }
        for (auto& [label, block] : blocks)
        {
            delete block;
            block = nullptr;
        }
    }

    Block* Function::createBlock()
    {
        Block* newBlock  = new Block(maxLabel);
        blocks[maxLabel] = newBlock;

        maxLabel++;
        return newBlock;
    }
    Block* Function::getBlock(size_t label)
    {
        if (blocks.find(label) != blocks.end()) return blocks[label];
        return nullptr;
    }
    void   Function::setMaxReg(size_t reg) { maxReg = reg; }
    size_t Function::getMaxReg() { return maxReg; }
    void   Function::setMaxLabel(size_t label) { maxLabel = label; }
    size_t Function::getMaxLabel() { return maxLabel; }
    size_t Function::getNewRegId() { return ++maxReg; }
}  // namespace ME
