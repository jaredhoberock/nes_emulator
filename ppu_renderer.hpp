#pragma once

#include "graphics_bus.hpp"
#include <cstdint>
#include <span>

// ppu_render implements the PPU's rendering algorithm
class ppu_renderer
{
  public:
    struct rgb
    {
      std::uint8_t r,g,b;
    };

    constexpr static int framebuffer_width  = 256;
    constexpr static int framebuffer_height = 240;

    ppu_renderer(graphics_bus& bus, std::span<rgb, framebuffer_width*framebuffer_height> framebuffer)
      : bus_{bus},
        palette_{},
        object_attributes_{{}},
        framebuffer_{framebuffer},
        background_tile_id_latch_{},
        background_tile_attribute_latch_{},
        background_tile_lsb_latch_{},
        background_tile_msb_latch_{},
        background_pattern_shift_register_low_{},
        background_pattern_shift_register_high_{},
        background_attribute_shift_register_low_{},
        background_attribute_shift_register_high_{},
        current_scanline_{},
        current_col_{}
    {}

    inline std::uint8_t palette(std::uint8_t i) const
    {
      return palette_[i];
    }

    inline void set_palette(std::uint8_t i, std::uint8_t value)
    {
      palette_[i] = value;
    }

    inline rgb as_rgb(int palette_idx, std::uint8_t color_idx) const
    {
      return system_palette_[palette_[4*palette_idx + color_idx]];
    }

    union loopy_register
    {
      uint16_t as_uint16;
      struct
      {
        std::uint16_t coarse_x : 5;
        std::uint16_t coarse_y : 5;
        std::uint16_t nametable_x : 1;
        std::uint16_t nametable_y : 1;
        std::uint16_t fine_y : 3;
        std::uint16_t unused : 1;
      };

      loopy_register() : as_uint16{} {}
    };

    static_assert(sizeof(loopy_register) == sizeof(std::uint16_t));

    // executes the next ppu cycle and returns whether or not the ppu should update the vertical blank flag
    std::optional<bool> step_cycle(bool show_background, bool show_sprites,
                                   loopy_register& vram_address, loopy_register tram_address,
                                   bool background_pattern_table_address, std::uint8_t fine_x);

    struct object_attribute
    {
      std::uint8_t y_position;
      std::uint8_t tile_id;
      std::uint8_t attribute;
      std::uint8_t x_position;
    };

    inline std::span<const object_attribute,64> object_attributes() const
    {
      return object_attributes_;
    }

  private:
    // read is called by step_cycle's helper functions
    inline std::uint8_t read(std::uint16_t address) const
    {
      return bus_.read(address);
    }

    // https://www.nesdev.org/wiki/PPU_palettes#2C02
    static constexpr std::array<rgb,64> system_palette_ = {{
      { 84, 84, 84}, {  0, 30,116}, {  8, 16,144}, { 48,  0,136}, { 68,  0,100}, { 92,  0, 48}, { 84,  4,  0}, { 60, 24,  0}, { 32, 42,  0}, {  8, 58,  0}, {  0, 64,  0}, {  0, 60,  0}, {  0, 50, 60}, {  0,  0,  0}, {0,0,0}, {0,0,0},
      {152,150,152}, {  8, 76,196}, { 48, 50,236}, { 92, 30,228}, {136, 20,176}, {160, 20,100}, {152, 34, 32}, {120, 60,  0}, { 84, 90,  0}, { 40,114,  0}, {  8,124,  0}, {  0,118, 40}, {  0,102,120}, {  0,  0,  0}, {0,0,0}, {0,0,0},
      {236,238,236}, { 76,154,236}, {120,124,236}, {176, 98,236}, {228, 84,236}, {236, 88,180}, {236,106,100}, {212,136, 32}, {160,170,  0}, {116,196,  0}, { 76,208, 32}, { 56,204,108}, { 56,180,204}, { 60, 60, 60}, {0,0,0}, {0,0,0},
      {236,238,236}, {168,204,236}, {188,188,236}, {212,178,236}, {236,174,236}, {236,174,212}, {236,180,176}, {228,196,144}, {204,210,120}, {180,222,120}, {168,226,144}, {152,226,180}, {160,214,228}, {160,162,160}, {0,0,0}, {0,0,0}
    }};

    graphics_bus& bus_;
    std::array<std::uint8_t, 32> palette_;
    std::array<object_attribute, 64> object_attributes_;
    std::span<rgb, framebuffer_width*framebuffer_height> framebuffer_;

    // this state is mutated by step_cycle and controls its behavior
    std::uint8_t background_tile_id_latch_;
    std::uint8_t background_tile_attribute_latch_;
    std::uint8_t background_tile_lsb_latch_;
    std::uint8_t background_tile_msb_latch_;
    std::uint16_t background_pattern_shift_register_low_;
    std::uint16_t background_pattern_shift_register_high_;
    std::uint8_t background_attribute_shift_register_low_;
    std::uint8_t background_attribute_shift_register_high_;
    std::uint16_t current_scanline_;
    std::uint16_t current_col_;

    // the following functions are called by step_cycle and mutate the state above

    inline void read_next_background_tile_id(loopy_register vram_address)
    {
      // only take the low 14 bits of vram_address
      // 0x2000 is the base address of vram
      background_tile_id_latch_ = read(0x2000 + (vram_address.as_uint16 & 0x0FFF));
    }

    inline void read_next_background_tile_attribute(loopy_register vram_address)
    {
      // XXX all of these bitwise | would make more sense as +
      std::uint16_t address = 
        0x23C0 | 
        (vram_address.nametable_y << 11) |
        (vram_address.nametable_x << 10) |
        ((vram_address.coarse_y >> 2) << 3) |
        (vram_address.coarse_x >> 2)
      ;

      background_tile_attribute_latch_ = read(address);

      // see https://www.nesdev.org/wiki/PPU_attribute_tables
      // the following shifts unpack two bits from the
      // byte depending on which of four tiles we're
      // rendering, based on the low two bits of
      // coarse_x and coarse_y

      if(vram_address.coarse_y & 0b10)
      {
        background_tile_attribute_latch_ >>= 4;
      }

      if(vram_address.coarse_x & 0b10)
      {
        background_tile_attribute_latch_ >>= 2;
      }

      // keep the low two bits of whatever is left
      // after the above shifts
      background_tile_attribute_latch_ &= 0b11;
    }


    inline void read_next_background_tile_lsb(loopy_register vram_address, bool background_pattern_table_address)
    {
      std::uint16_t address = 
        (background_pattern_table_address << 12) +
        (static_cast<std::uint16_t>(background_tile_id_latch_) << 4) +
        (vram_address.fine_y + 0)
      ;

      background_tile_lsb_latch_ = read(address);
    }


    inline void read_next_background_tile_msb(loopy_register vram_address, bool background_pattern_table_address)
    {
      std::uint16_t address = 
        (background_pattern_table_address << 12) +
        (static_cast<std::uint16_t>(background_tile_id_latch_) << 4) +
        (vram_address.fine_y + 8)
      ;

      background_tile_msb_latch_ = read(address);
    }


    inline void maybe_increment_x(bool enabled, loopy_register& vram_address)
    {
      // XXX is this if necessary?
      if(enabled)
      {
        if(vram_address.coarse_x == 31)
        {
          vram_address.coarse_x = 0;
          vram_address.nametable_x = !vram_address.nametable_x;
        }
        else
        {
          vram_address.coarse_x++;
        }
      }
    }

    inline void maybe_increment_y(bool enabled, loopy_register& vram_address)
    {
      // XXX is this if necessary?
      if(enabled)
      {
        if(vram_address.fine_y == 7)
        {
          vram_address.fine_y = 0;

          if(vram_address.coarse_y == 29)
          {
            vram_address.coarse_y = 0;
            vram_address.nametable_y = !vram_address.nametable_y;
          }
          else if(vram_address.coarse_y == 31)
          {
            vram_address.coarse_y = 0;
          }
          else
          {
            vram_address.coarse_y++;
          }
        }
        else
        {
          vram_address.fine_y++;
        }
      }
    }

    inline void maybe_copy_x(bool enabled, loopy_register& vram_address, loopy_register tram_address)
    {
      // XXX is this if necessary?
      if(enabled)
      {
        vram_address.nametable_x = tram_address.nametable_x;
        vram_address.coarse_x = tram_address.coarse_x;
      }
    }

    inline void maybe_copy_y(bool enabled, loopy_register& vram_address, loopy_register tram_address)
    {
      // XXX is this if necessary?
      if(enabled)
      {
        vram_address.fine_y = tram_address.fine_y;
        vram_address.nametable_y = tram_address.nametable_y;
        vram_address.coarse_y = tram_address.coarse_y;
      }
    }

    inline void load_background_shift_registers()
    {
      // load the next tile's pattern byte into the low byte of these two shift registers
      background_pattern_shift_register_low_  = (background_pattern_shift_register_low_  & 0xFF00) | background_tile_lsb_latch_;
      background_pattern_shift_register_high_ = (background_pattern_shift_register_high_ & 0xFF00) | background_tile_msb_latch_;

      // expand the two bits in background_tile_attribute_latch_ into two shift registers
      // note that these are either entirely 1s or 0s
      background_attribute_shift_register_low_  = (background_tile_attribute_latch_ & 0b01) ? 0xFF : 0x00;
      background_attribute_shift_register_high_ = (background_tile_attribute_latch_ & 0b10) ? 0xFF : 0x00;
    }

    inline void maybe_update_background_pattern_shift_registers(bool enabled)
    {
      if(enabled)
      {
        background_pattern_shift_register_low_ <<= 1;
        background_pattern_shift_register_high_ <<= 1;
      }
    }

    // renders (palette_idx, pixel) using the current state of the renderer and fine_x
    inline std::pair<std::uint8_t,std::uint8_t> maybe_render_background(bool enabled, std::uint8_t fine_x) const
    {
      std::uint8_t palette_idx = 0;
      std::uint8_t pixel = 0;

      if(enabled)
      {
        // fine x selects a bit from the bit planes represented in the shift registers
        std::uint16_t mux = 0b10000000 >> fine_x;

        // construct the background pixel from two bit planes
        std::uint8_t pixel_plane_0 = (background_pattern_shift_register_low_  & mux) != 0;
        std::uint8_t pixel_plane_1 = (background_pattern_shift_register_high_ & mux) != 0;
        pixel = (pixel_plane_1 << 1) | pixel_plane_0;

        // construct the palette from two bit planes
        std::uint8_t palette_plane_0 = (background_attribute_shift_register_low_  & mux) != 0;
        std::uint8_t palette_plane_1 = (background_attribute_shift_register_high_ & mux) != 0;
        palette_idx = (palette_plane_1 << 1) | palette_plane_0;
      }

      return {palette_idx,pixel};
    }
};

