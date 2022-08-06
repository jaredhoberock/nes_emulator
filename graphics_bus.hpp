#pragma once

#include "cartridge.hpp"
#include "ppu.hpp"
#include <cstdint>
#include <fmt/format.h>
#include <iostream>
#include <span>
#include <stdexcept>

class graphics_bus
{
  private:
    cartridge& cart_;
    ppu& ppu_;

    // XXX put this in system
    constexpr static std::uint16_t nametable_size = 1024;
    std::array<std::uint8_t, 2*nametable_size> vram_;

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
    graphics_bus(cartridge& cart, ppu& p)
      : cart_{cart},
        ppu_{p},
        vram_{}
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
      else if(0x3F00 <= address and address < 0x4000)
      {
        // palette
        address &= 0x001F;
        if(address == 0x0010) address = 0x0000;
        if(address == 0x0014) address = 0x0004;
        if(address == 0x0018) address = 0x0008;
        if(address == 0x001C) address = 0x000C;

        result = ppu_.palette(address);
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
      else if(0x3F00 <= address and address < 0x4000)
      {
        // palette
        address &= 0x001F;
        if(address == 0x0010) address = 0x0000;
        if(address == 0x0014) address = 0x0004;
        if(address == 0x0018) address = 0x0008;
        if(address == 0x001C) address = 0x000C;

        ppu_.set_palette(address, data);
      }
      else
      {
        throw std::runtime_error(fmt::format("graphics_bus::write: Bad address: {:04X}", address));
      }
    }

    constexpr static int pattern_table_dim = 128;

    inline auto pattern_table_as_image(int i, int palette) const
    {
      std::array<ppu::rgb, pattern_table_dim*pattern_table_dim> result;

      // a pattern table is 16 * 16 tiles
      // each tile is 8 * 8 pixels

      // XXX can't we just do this linearly?
      for(uint16_t tile_y = 0; tile_y < 16; ++tile_y)
      {
        for(uint16_t tile_x = 0; tile_x < 16; ++tile_x)
        {
          // 2 * pattern_table_dim because there are two bytes which describe each tile
          int offset = tile_y * (2 * pattern_table_dim) + tile_x * 16;

          for(uint16_t row = 0; row < 8; ++row)
          {
            uint8_t tile_lsb = read(i * 0x1000 + offset + row + 0);
            uint8_t tile_msb = read(i * 0x1000 + offset + row + 8);

            for(uint16_t col = 0; col < 8; ++col)
            {
              uint8_t pixel = (tile_lsb & 0x01) + (tile_msb & 0x01);
              tile_lsb >>= 1;
              tile_msb >>= 1;

              int x = 8 * tile_x + (7 - col);
              int y = 8 * tile_y + row;

              ppu::rgb color = ppu_.as_rgb(palette, pixel);

              result[pattern_table_dim*y + x] = color;
            }
          }
        }
      }

      return result;
    }

    inline std::span<const std::uint8_t, nametable_size> nametable(int i) const
    {
      std::span<const std::uint8_t, 2*nametable_size> all = vram_;
      return i == 0 ? all.first<nametable_size>() : all.last<nametable_size>();
    }
};

