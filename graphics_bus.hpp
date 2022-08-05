#pragma once

#include "cartridge.hpp"
#include "ppu.hpp"
#include <cstdint>
#include <fmt/format.h>
#include <iostream>
#include <stdexcept>

class graphics_bus
{
  private:
    cartridge& cart_;
    ppu& ppu_;
    // XXX these should go into a single array called vram
    std::array<std::uint8_t, 1024> nametable_zero_;
    std::array<std::uint8_t, 1024> nametable_one_;

  public:
    graphics_bus(cartridge& cart, ppu& p)
      : cart_{cart},
        ppu_{p},
        nametable_zero_{{}},
        nametable_one_{{}}
    {
    }

    inline std::uint8_t read(std::uint16_t address) const
    {
      std::uint8_t result = 0;

      if(0x0000 <= address and address < 0x2000)
      {
        result = cart_.graphics_read(address);
      }
      else if(0x2000 <= address and address < 0x3F00)
      {
        // nametables
        
        // this bitwise AND does address mirroring
        address &= 0x0FFF;
        
        // XXX this calculation should be done by the mapper
        if(cart_.nametable_mirroring() == cartridge::horizontal)
        {
          if(0x0000 <= address and address < 0x0400)
          {
            // XXX what is this AND for?
            result = nametable_zero_[address & 0x03FF];
          }
          else if(0x0400 <= address and address < 0x0800)
          {
            result = nametable_zero_[address & 0x03FF];
          }
          else if(0x0800 <= address and address < 0x0C00)
          {
            result = nametable_one_[address & 0x03FF];
          }
          else if(0x0C00 <= address and address < 0x1000)
          {
            result = nametable_one_[address & 0x3FF];
          }
        }
        else if(cart_.nametable_mirroring() == cartridge::vertical)
        {
          if(0x0000 <= address and address < 0x0400)
          {
            // XXX what is this AND for?
            result = nametable_zero_[address & 0x03FF];
          }
          else if(0x0400 <= address and address < 0x0800)
          {
            result = nametable_one_[address & 0x03FF];
          }
          else if(0x0800 <= address and address < 0x0C00)
          {
            result = nametable_zero_[address & 0x03FF];
          }
          else if(0x0C00 <= address and address < 0x1000)
          {
            result = nametable_one_[address & 0x3FF];
          }
        }
        else
        {
          throw std::runtime_error("graphics_bus::read: Illegal nametable_mirroring_kind");
        }
      }
      else if(0x3F00 <= address and address < 0x4000)
      {
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

        // this bitwise AND does address mirroring
        address &= 0x0FFF;

        // XXX this calculation should be done by the mapper
        // we should first calculate the logical nt index
        // and then map that to a physical nt index
        // the logical nt index is just address / nametable_size_in_bytes

        // XXX try this simpler calculation once we can see stuff on the screen
        //int logical_nametable_idx = address / 1024;
        //int physical_nametable_idx = (cart_.nametable_mirroring() == cartridge::horizontal) ?
        //  logical_nametable_idx / 2 :
        //  logical_nametable_idx % 2
        //;

        //// the offset into the nametable is just the address % 1024
        //int nametable_offset address % 1024;

        // XXX this calculation should be done by the mapper
        if(cart_.nametable_mirroring() == cartridge::horizontal)
        {
          if(0x0000 <= address and address < 0x0400)
          {
            // logical nt 0 maps to physical nt 0

            // XXX these ANDs compute an offset into each nametable, as above
            nametable_zero_[address & 0x03FF] = data;
          }
          else if(0x0400 <= address and address < 0x0800)
          {
            // logical nt 1 maps to physical nt 0
            nametable_zero_[address & 0x03FF] = data;
          }
          else if(0x0800 <= address and address < 0x0C00)
          {
            // logical nt 2 maps to physical nt 1
            nametable_one_[address & 0x03FF] = data;
          }
          else if(0x0C00 <= address and address < 0x1000)
          {
            // logical nt 3 maps to physical nt 1
            nametable_one_[address & 0x03FF] = data;
          }
        }
        else if(cart_.nametable_mirroring() == cartridge::vertical)
        {
          if(0x0000 <= address and address < 0x0400)
          {
            // XXX what is this AND for?
            nametable_zero_[address & 0x03FF] = data;
          }
          else if(0x0400 <= address and address < 0x0800)
          {
            nametable_one_[address & 0x03FF] = data;
          }
          else if(0x0800 <= address and address < 0x0C00)
          {
            nametable_zero_[address & 0x03FF] = data;
          }
          else if(0x0C00 <= address and address < 0x1000)
          {
            nametable_one_[address & 0x03FF] = data;
          }
        }
        else
        {
          throw std::runtime_error("graphics_bus::read: Illegal nametable_mirroring_kind");
        }
      }
      else if(0x3F00 <= address and address < 0x4000)
      {
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

    inline const std::array<std::uint8_t,1024>& nametable(int i) const
    {
      return (i == 0) ? nametable_zero_ : nametable_one_;
    }
};

