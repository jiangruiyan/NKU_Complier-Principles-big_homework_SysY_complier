#ifndef __MIDDLEEND_MODULE_IR_FUNCTION_H__
#define __MIDDLEEND_MODULE_IR_FUNCTION_H__

#include <middleend/module/ir_block.h>
#include <map>

namespace ME
{
    class Function : public Visitable
    {
      public:
        FuncDefInst*             funcDef;
        std::map<size_t, Block*> blocks;

      private:
        size_t maxLabel;
        size_t maxReg;

      public: /*以下2个变量与循环优化相关，如果你正在做Lab3，可以暂时忽略它们 */
        size_t loopStartLabel;
        size_t loopEndLabel;

      public:
        Function(FuncDefInst* fd);
        ~Function();

      public:
        virtual void accept(Visitor& visitor) override { visitor.visit(*this); }

        Block* createBlock();
        Block* getBlock(size_t label);
        void   setMaxReg(size_t reg);
        size_t getMaxReg();
        void   setMaxLabel(size_t label);
        size_t getMaxLabel();
        size_t getNewRegId();
    };
}  // namespace ME

#endif  // __MIDDLEEND_MODULE_IR_FUNCTION_H__
