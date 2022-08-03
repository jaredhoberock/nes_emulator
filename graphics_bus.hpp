#pragma once

#include "cartridge.hpp"
#include <cstdint>
#include <fmt/format.h>
#include <stdexcept>

class graphics_bus
{
  private:
    cartridge& cart_;

  public:
    graphics_bus(cartridge& cart)
      : cart_{cart}
    {}

    inline std::uint8_t read(std::uint16_t address) const
    {
      std::uint8_t result = 0;

      if(0x0000 <= address and address < 0x2000)
      {
        result = cart_.graphics_read(address);
      }
      else
      {
        throw std::runtime_error(fmt::format("graphics_bus::read: Bad adddress: {:04x}", address));
      }

      return result;
    }
};

