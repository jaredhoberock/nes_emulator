#pragma once

#include "cartridge.hpp"
#include <cstdint>
#include <fmt/format.h>
#include <span>
#include <stdexcept>


namespace nes
{


class graphics_bus
{
  private:
    constexpr static std::uint16_t nametable_size = 1024;

    cartridge& cart_;
    std::span<std::uint8_t, 2*nametable_size> vram_;

    inline std::uint16_t map_nametable_address(std::uint16_t address) const
    {
      // this bitwise AND does address mirroring
      address &= 0x0FFF;

      // XXX it feels like the following calculation should be done by the mapper

      if(cart_.nametable_mirroring() != cartridge::horizontal and cart_.nametable_mirroring() != cartridge::vertical)
      {
        throw std::runtime_error("graphics_bus::map_nametable_address: Unimplemented nametable mirroring kind");
      }

      // to find the index of the logical nametable, just divide the address by the nametable size
      int logical_nametable_idx = address / nametable_size;

      // map the logical index in [0,4) to the physical index in [0,2) based on the nametable mirroring mode
      int physical_nametable_idx = (cart_.nametable_mirroring() == cartridge::horizontal) ?
        logical_nametable_idx / 2 :
        logical_nametable_idx % 2
      ;

      // the index of the byte in vram is simply the address modulo nametable size
      int byte_idx = address % nametable_size;

      return physical_nametable_idx * nametable_size + byte_idx;
    }

  public:
    graphics_bus(cartridge& cart, std::span<std::uint8_t, 2*nametable_size> vram)
      : cart_{cart},
        vram_{vram}
    {}

    inline std::uint8_t read(std::uint16_t address) const
    {
      std::uint8_t result = 0;

      if(0x0000 <= address and address < 0x2000)
      {
        // cartridge CHR memory
        result = cart_.graphics_read(address);
      }
      else if(0x2000 <= address and address < 0x3F00)
      {
        // nametables
        result = vram_[map_nametable_address(address)];
      }
      else
      {
        throw std::runtime_error(fmt::format("graphics_bus::read: Bad address: {:04X}", address));
      }

      return result;
    }

    inline void write(std::uint16_t address, std::uint8_t data)
    {
      if(0x2000 <= address and address < 0x3F00)
      {
        // nametables
        vram_[map_nametable_address(address)] = data;
      }
      else
      {
        throw std::runtime_error(fmt::format("graphics_bus::write: Bad address: {:04X}", address));
      }
    }
};


} // end nes

