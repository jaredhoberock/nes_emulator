#pragma once

#include "bus.hpp"
#include "cartridge.hpp"
#include "graphics_bus.hpp"
#include "mos6502.hpp"
#include "ppu.hpp"
#include <array>
#include <span>

class system
{
  public:
    constexpr static int framebuffer_width  = 256;
    constexpr static int framebuffer_height = 240;

    // see https://www.nesdev.org/wiki/Standard_controller#Report
    constexpr static std::uint8_t a_button_bitmask      = 0b10000000;
    constexpr static std::uint8_t b_button_bitmask      = 0b01000000;
    constexpr static std::uint8_t select_button_bitmask = 0b00100000;
    constexpr static std::uint8_t start_button_bitmask  = 0b00010000;
    constexpr static std::uint8_t up_button_bitmask     = 0b00001000;
    constexpr static std::uint8_t down_button_bitmask   = 0b00000100;
    constexpr static std::uint8_t left_button_bitmask   = 0b00000010;
    constexpr static std::uint8_t right_button_bitmask  = 0b00000001;

    system(const char* rom_filename)
      : cpu_{bus_},
        ppu_{graphics_bus_, framebuffer_},
        controllers_{{}},
        wram_{{}},
        cart_{rom_filename},
        bus_{controllers_, cart_, wram_, ppu_},
        vram_{},
        graphics_bus_{cart_, vram_}
    {
      for(int row = 0; row < framebuffer_height; ++row)
      {
        for(int col = 0; col < framebuffer_width; ++col)
        {
          framebuffer_[row * framebuffer_width + col] = {0, 255, 0};
        }
      }
    }

    inline void set_controller(std::uint8_t idx, std::uint8_t state)
    {
      controllers_[idx] = state;
    }

    inline mos6502& cpu()
    {
      return cpu_;
    }

    inline const mos6502& cpu() const
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

    inline const class bus& bus() const
    {
      return bus_;
    }

    inline const class graphics_bus& graphics_bus() const
    {
      return graphics_bus_;
    }

    inline std::array<std::uint8_t,256> zero_page() const
    {
      std::array<std::uint8_t,256> result;
      std::copy_n(wram_.begin(), result.size(), result.begin());
      return result;
    }

    inline std::span<const ppu::rgb, framebuffer_width*framebuffer_height> framebuffer() const
    {
      return framebuffer_;
    }

    constexpr static std::uint16_t nametable_size = 1024;

    inline std::span<const std::uint8_t, nametable_size> nametable(int i) const
    {
      std::span<const std::uint8_t, 2*nametable_size> all = vram_;
      return i == 0 ? all.first<nametable_size>() : all.last<nametable_size>();
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
            uint8_t tile_lsb = graphics_bus_.read(i * 0x1000 + offset + row + 0);
            uint8_t tile_msb = graphics_bus_.read(i * 0x1000 + offset + row + 8);

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

    std::array<ppu::rgb,4> palette_as_image(int palette_idx) const
    {
      return {ppu_.as_rgb(palette_idx,0), ppu_.as_rgb(palette_idx,1), ppu_.as_rgb(palette_idx,2), ppu_.as_rgb(palette_idx,3)};
    }

  private:
    std::array<ppu::rgb, framebuffer_width * framebuffer_height> framebuffer_;

    mos6502 cpu_;
    class ppu ppu_;
    std::array<std::uint8_t,2> controllers_;
    std::array<std::uint8_t,2048> wram_;
    cartridge cart_;
    class bus bus_;
    std::array<std::uint8_t, 2*nametable_size> vram_;
    class graphics_bus graphics_bus_;
};

