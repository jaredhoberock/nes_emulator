#pragma once

#include "bus.hpp"
#include "cartridge.hpp"
#include "graphics_bus.hpp"
#include "mos6502.hpp"
#include "ppu.hpp"

class system
{
  public:
    system(const char* rom_filename)
      : cpu_{bus_},
        ppu_{graphics_bus_},
        cart_{rom_filename},
        bus_{cart_, ppu_},
        graphics_bus_{cart_, ppu_}
    {}

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

    inline class bus& bus()
    {
      return bus_;
    }

    inline const class graphics_bus& graphics_bus() const
    {
      return graphics_bus_;
    }

  private:
    mos6502 cpu_;
    class ppu ppu_;
    cartridge cart_;
    class bus bus_;
    class graphics_bus graphics_bus_;
};

