#pragma once

#include "cartridge.hpp"
#include <cstdint>
#include <fmt/format.h>
#include <iostream>
#include <stdexcept>

class graphics_bus
{
  private:
    cartridge& cart_;
    // XXX the palette should maybe be moved to the ppu
    std::array<std::uint8_t, 32> palette_;
    // XXX these should go into a single array called vram
    std::array<std::uint8_t, 1024> nametable_zero_;
    std::array<std::uint8_t, 1024> nametable_one_;

  public:
    graphics_bus(cartridge& cart)
      : cart_{cart},
        palette_{}
    {}

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

        result = palette_[address];
      }
      else
      {
        throw std::runtime_error(fmt::format("graphics_bus::read: Bad adddress: {:04X}", address));
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

        palette_[address] = data;
      }
      else
      {
        throw std::runtime_error(fmt::format("graphics_bus::write: Bad address: {:04X}", address));
      }
    }

    struct rgb
    {
      std::uint8_t r, g, b;
    };

    static constexpr std::uint16_t palette_base_address = 0x3F00;

    // https://www.nesdev.org/wiki/PPU_palettes#2C02
    static constexpr std::array<rgb,64> system_palette = {{
      { 84, 84, 84}, {  0, 30,116}, {  8, 16,144}, { 48,  0,136}, { 68,  0,100}, { 92,  0, 48}, { 84,  4,  0}, { 60, 24,  0}, { 32, 42,  0}, {  8, 58,  0}, {  0, 64,  0}, {  0, 60,  0}, {  0, 50, 60}, {  0,  0,  0}, {0,0,0}, {0,0,0},
      {152,150,152}, {  8, 76,196}, { 48, 50,236}, { 92, 30,228}, {136, 20,176}, {160, 20,100}, {152, 34, 32}, {120, 60,  0}, { 84, 90,  0}, { 40,114,  0}, {  8,124,  0}, {  0,118, 40}, {  0,102,120}, {  0,  0,  0}, {0,0,0}, {0,0,0},
      {236,238,236}, { 76,154,236}, {120,124,236}, {176, 98,236}, {228, 84,236}, {236, 88,180}, {236,106,100}, {212,136, 32}, {160,170,  0}, {116,196,  0}, { 76,208, 32}, { 56,204,108}, { 56,180,204}, { 60, 60, 60}, {0,0,0}, {0,0,0},
      {236,238,236}, {168,204,236}, {188,188,236}, {212,178,236}, {236,174,236}, {236,174,212}, {236,180,176}, {228,196,144}, {204,210,120}, {180,222,120}, {168,226,144}, {152,226,180}, {160,214,228}, {160,162,160}, {0,0,0}, {0,0,0}
    }};

    inline rgb as_rgb(int palette, std::uint8_t pixel) const
    {
      return system_palette[read(palette_base_address + 4 * palette + pixel)];
    }

    constexpr static int pattern_table_dim = 128;

    inline auto pattern_table_as_image(int i, int palette) const
    {
      std::array<rgb, pattern_table_dim*pattern_table_dim> result;

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

              rgb color = as_rgb(palette, pixel);

              result[pattern_table_dim*y + x] = color;
            }
          }
        }
      }

      return result;
    }

    inline std::array<rgb, 4> palette_as_image(int palette) const
    {
      return {as_rgb(palette,0), as_rgb(palette,1), as_rgb(palette,2), as_rgb(palette,3)};
    }
};

