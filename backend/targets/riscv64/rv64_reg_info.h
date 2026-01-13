#ifndef __BACKEND_TARGETS_RISCV64_RV64_REG_INFO_H__
#define __BACKEND_TARGETS_RISCV64_RV64_REG_INFO_H__

#include <backend/target/target_reg_info.h>
#include <backend/targets/riscv64/rv64_defs.h>
#include <map>

namespace BE::Targeting::RV64
{
    namespace PR = BE::RV64::PR;

    class RegInfo : public TargetRegInfo
    {
      public:
        RegInfo() = default;

        int spRegId() const override { return static_cast<int>(PR::Reg::x2); }
        int raRegId() const override { return static_cast<int>(PR::Reg::x1); }
        int zeroRegId() const override { return static_cast<int>(PR::Reg::x0); }

        const std::vector<int>& intArgRegs() const override { 
          static const std::vector<int> regs = {
              static_cast<int>(PR::Reg::x10),
              static_cast<int>(PR::Reg::x11),
              static_cast<int>(PR::Reg::x12),
              static_cast<int>(PR::Reg::x13),
              static_cast<int>(PR::Reg::x14),
              static_cast<int>(PR::Reg::x15),
              static_cast<int>(PR::Reg::x16),
              static_cast<int>(PR::Reg::x17),
          };
          return regs;
        }
        const std::vector<int>& floatArgRegs() const override { 
          static const std::vector<int> regs = {
              static_cast<int>(PR::Reg::f10),
              static_cast<int>(PR::Reg::f11),
              static_cast<int>(PR::Reg::f12),
              static_cast<int>(PR::Reg::f13),
              static_cast<int>(PR::Reg::f14),
              static_cast<int>(PR::Reg::f15),
              static_cast<int>(PR::Reg::f16),
              static_cast<int>(PR::Reg::f17),
          };
          return regs;
        }

        const std::vector<int>& calleeSavedIntRegs() const override { 
          static const std::vector<int> regs = {
              static_cast<int>(PR::Reg::x8),
              static_cast<int>(PR::Reg::x9),
              static_cast<int>(PR::Reg::x18),
              static_cast<int>(PR::Reg::x19),
              static_cast<int>(PR::Reg::x20),
              static_cast<int>(PR::Reg::x21),
              static_cast<int>(PR::Reg::x22),
              static_cast<int>(PR::Reg::x23),
              static_cast<int>(PR::Reg::x24),
              static_cast<int>(PR::Reg::x25),
              static_cast<int>(PR::Reg::x26),
              static_cast<int>(PR::Reg::x27),
          };
          return regs;
        }
        const std::vector<int>& calleeSavedFloatRegs() const override { 
          static const std::vector<int> regs = {
              static_cast<int>(PR::Reg::f8),
              static_cast<int>(PR::Reg::f9),
              static_cast<int>(PR::Reg::f18),
              static_cast<int>(PR::Reg::f19),
              static_cast<int>(PR::Reg::f20),
              static_cast<int>(PR::Reg::f21),
              static_cast<int>(PR::Reg::f22),
              static_cast<int>(PR::Reg::f23),
              static_cast<int>(PR::Reg::f24),
              static_cast<int>(PR::Reg::f25),
              static_cast<int>(PR::Reg::f26),
              static_cast<int>(PR::Reg::f27),
          };
          return regs;
        }

        const std::vector<int>& reservedRegs() const override { 
          static const std::vector<int> regs = {
              static_cast<int>(PR::Reg::x0),
              static_cast<int>(PR::Reg::x1),
              static_cast<int>(PR::Reg::x2),
              static_cast<int>(PR::Reg::x3),
              static_cast<int>(PR::Reg::x4),
              static_cast<int>(PR::Reg::x5),
              // Reserve argument registers to avoid call-arg copy clobbering.
              static_cast<int>(PR::Reg::x10),
              static_cast<int>(PR::Reg::x11),
              static_cast<int>(PR::Reg::x12),
              static_cast<int>(PR::Reg::x13),
              static_cast<int>(PR::Reg::x14),
              static_cast<int>(PR::Reg::x15),
              static_cast<int>(PR::Reg::x16),
              static_cast<int>(PR::Reg::x17),
              static_cast<int>(PR::Reg::f10),
              static_cast<int>(PR::Reg::f11),
              static_cast<int>(PR::Reg::f12),
              static_cast<int>(PR::Reg::f13),
              static_cast<int>(PR::Reg::f14),
              static_cast<int>(PR::Reg::f15),
              static_cast<int>(PR::Reg::f16),
              static_cast<int>(PR::Reg::f17),
          };
          return regs;
        }
        const std::vector<int>& intRegs() const override { 
          static const std::vector<int> regs = {
              static_cast<int>(PR::Reg::x0),
              static_cast<int>(PR::Reg::x1),
              static_cast<int>(PR::Reg::x2),
              static_cast<int>(PR::Reg::x3),
              static_cast<int>(PR::Reg::x4),
              static_cast<int>(PR::Reg::x5),
              static_cast<int>(PR::Reg::x6),
              static_cast<int>(PR::Reg::x7),
              static_cast<int>(PR::Reg::x8),
              static_cast<int>(PR::Reg::x9),
              static_cast<int>(PR::Reg::x10),
              static_cast<int>(PR::Reg::x11),
              static_cast<int>(PR::Reg::x12),
              static_cast<int>(PR::Reg::x13),
              static_cast<int>(PR::Reg::x14),
              static_cast<int>(PR::Reg::x15),
              static_cast<int>(PR::Reg::x16),
              static_cast<int>(PR::Reg::x17),
              static_cast<int>(PR::Reg::x18),
              static_cast<int>(PR::Reg::x19),
              static_cast<int>(PR::Reg::x20),
              static_cast<int>(PR::Reg::x21),
              static_cast<int>(PR::Reg::x22),
              static_cast<int>(PR::Reg::x23),
              static_cast<int>(PR::Reg::x24),
              static_cast<int>(PR::Reg::x25),
              static_cast<int>(PR::Reg::x26),
              static_cast<int>(PR::Reg::x27),
              static_cast<int>(PR::Reg::x28),
              static_cast<int>(PR::Reg::x29),
              static_cast<int>(PR::Reg::x30),
              static_cast<int>(PR::Reg::x31),
          };
          return regs;
        }
        const std::vector<int>& floatRegs() const override { 
          static const std::vector<int> regs = {
              static_cast<int>(PR::Reg::f0),
              static_cast<int>(PR::Reg::f1),
              static_cast<int>(PR::Reg::f2),
              static_cast<int>(PR::Reg::f3),
              static_cast<int>(PR::Reg::f4),
              static_cast<int>(PR::Reg::f5),
              static_cast<int>(PR::Reg::f6),
              static_cast<int>(PR::Reg::f7),
              static_cast<int>(PR::Reg::f8),
              static_cast<int>(PR::Reg::f9),
              static_cast<int>(PR::Reg::f10),
              static_cast<int>(PR::Reg::f11),
              static_cast<int>(PR::Reg::f12),
              static_cast<int>(PR::Reg::f13),
              static_cast<int>(PR::Reg::f14),
              static_cast<int>(PR::Reg::f15),
              static_cast<int>(PR::Reg::f16),
              static_cast<int>(PR::Reg::f17),
              static_cast<int>(PR::Reg::f18),
              static_cast<int>(PR::Reg::f19),
              static_cast<int>(PR::Reg::f20),
              static_cast<int>(PR::Reg::f21),
              static_cast<int>(PR::Reg::f22),
              static_cast<int>(PR::Reg::f23),
              static_cast<int>(PR::Reg::f24),
              static_cast<int>(PR::Reg::f25),
              static_cast<int>(PR::Reg::f26),
              static_cast<int>(PR::Reg::f27),
              static_cast<int>(PR::Reg::f28),
              static_cast<int>(PR::Reg::f29),
              static_cast<int>(PR::Reg::f30),
              static_cast<int>(PR::Reg::f31),
          };
          return regs;
        }
    };
}  // namespace BE::Targeting::RV64

#endif  // __BACKEND_TARGETS_RISCV64_RV64_REG_INFO_H__
