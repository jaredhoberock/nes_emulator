#pragma once

#include "bus.hpp"
#include "cartridge.hpp"
#include "mos6502.hpp"
#include "ppu.hpp"

class system
{
  public:
    system(const char* rom_filename)
      : bus_{{}, cartridge{rom_filename}},
        cpu_{bus_}
    {}

    inline struct bus& bus()
    {
      return bus_;
    }

    inline mos6502& cpu()
    {
      return cpu_;
    }

    inline const ppu& ppu() const
    {
      return ppu_;
    }

    inline class ppu& ppu()
    {
      return ppu_;
    }

  private:
    struct bus bus_;
    mos6502 cpu_;
    class ppu ppu_;
};

