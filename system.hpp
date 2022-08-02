#pragma once

#include "bus.hpp"
#include "cartridge.hpp"
#include "mos6502.hpp"
#include "ppu.hpp"

class system
{
  public:
    system(const char* rom_filename)
      : cpu_{bus_},
        ppu_{},
        bus_{cartridge{rom_filename}, ppu_}
    {}

    inline class bus& bus()
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
    mos6502 cpu_;
    class ppu ppu_;
    class bus bus_;
};

