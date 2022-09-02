#pragma once

#include "bounded_array.hpp"
#include "graphics_bus.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <span>


namespace nes
{


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
        current_scanline_{},
        current_scanline_cycle_{},
        background_tile_id_latch_{},
        background_tile_attribute_latch_{},
        background_tile_lsb_latch_{},
        background_tile_msb_latch_{},
        background_pattern_shift_register_low_{},
        background_pattern_shift_register_high_{},
        background_attribute_shift_register_low_{},
        background_attribute_shift_register_high_{},
        active_sprites_{},
        sprite_pattern_shift_register_low_{{}},
        sprite_pattern_shift_register_high_{{}}
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

    union control_register_t
    {
      std::uint8_t as_byte;
      struct
      {
        bool nametable_x : 1;
        bool nametable_y : 1;
        bool vram_address_increment_mode : 1;
        bool sprite_pattern_table_address : 1;
        bool background_pattern_table_address : 1;
        bool sprite_size : 1;
        bool ppu_master_slave_select : 1;
        bool generate_nmi : 1;
      };

      control_register_t() : as_byte{} {}
    };

    static_assert(sizeof(control_register_t) == sizeof(std::uint8_t));

    union status_register_t
    {
      std::uint8_t as_byte;
      struct
      {
        std::uint8_t unused : 5;
        bool sprite_overflow : 1;
        bool sprite_zero_hit : 1;
        bool in_vertical_blank_period : 1;
      };

      status_register_t() : as_byte{} {}
    };

    static_assert(sizeof(status_register_t) == sizeof(std::uint8_t));

    union mask_register_t
    {
      std::uint8_t as_byte;
      struct
      {
        bool greyscale : 1;
        bool show_background_in_leftmost_8_pixels_of_screen : 1;
        bool show_sprites_in_leftmost_8_pixels_of_screen : 1;
        bool show_background : 1;
        bool show_sprites : 1;
        bool emphasize_red : 1;
        bool emphasize_green : 1;
        bool emphasize_blue : 1;
      };

      mask_register_t() : as_byte{} {}
    };

    static_assert(sizeof(mask_register_t) == sizeof(std::uint8_t));

    // executes the next ppu cycle and returns whether or not the ppu has entered the vertical blank period
    // XXX it might be more convenient to just pass this a copy of the control register and a reference to the status register
    bool step_cycle(bool show_background, bool show_sprites,
                    loopy_register& vram_address, loopy_register tram_address,
                    bool background_pattern_table_address, std::uint8_t fine_x,
                    control_register_t control, status_register_t& status, mask_register_t mask);

    struct object_attribute
    {
      std::uint8_t y_position;
      std::uint8_t tile_id;
      std::uint8_t attribute;
      std::uint8_t x_position;

      inline bool flip_vertically() const
      {
        // see https://www.nesdev.org/wiki/PPU_OAM#Byte_2
        return attribute & 0x80;
      }

      inline bool flip_horizontally() const
      {
        // see https://www.nesdev.org/wiki/PPU_OAM#Byte_2
        return attribute & 0x40;
      }

      inline bool tall_sprite_pattern_table() const
      {
        // see https://www.nesdev.org/wiki/PPU_OAM#Byte_1
        return tile_id & 0x01;
      }

      inline std::uint8_t tall_sprite_tile_id() const
      {
        // see https://www.nesdev.org/wiki/PPU_OAM#Byte_1
        return tile_id & 0xFE;
      }

      inline bool prioritize_foreground() const
      {
        // see https://www.nesdev.org/wiki/PPU_OAM#Byte_2
        return (attribute & 0x20) == 0;;
      }

      inline std::uint8_t palette_id() const
      {
        // see https://www.nesdev.org/wiki/PPU_OAM#Byte_2
        return (attribute & 0x03) + 0x04;
      }
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
    std::uint16_t current_scanline_;
    std::uint16_t current_scanline_cycle_;
    std::uint8_t background_tile_id_latch_;
    std::uint8_t background_tile_attribute_latch_;
    std::uint8_t background_tile_lsb_latch_;
    std::uint8_t background_tile_msb_latch_;
    std::uint16_t background_pattern_shift_register_low_;
    std::uint16_t background_pattern_shift_register_high_;
    std::uint16_t background_attribute_shift_register_low_;
    std::uint16_t background_attribute_shift_register_high_;

    // this is an array of (idx into object_attributes_, x_position)
    bounded_array<std::pair<std::uint8_t,std::uint8_t>,8> active_sprites_;
    std::array<std::uint8_t,8> sprite_pattern_shift_register_low_;
    std::array<std::uint8_t,8> sprite_pattern_shift_register_high_;

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

      // load the next tile's attribute bits into the low byte of these two shift registers
      // note that the two bits in background_tile_attribute_latch_ get expanded into full bytes of 1s or 0s
      background_attribute_shift_register_low_  = (background_attribute_shift_register_low_ & 0xFF00)  | ((background_tile_attribute_latch_ & 0b01) ? 0xFF : 0x00);
      background_attribute_shift_register_high_ = (background_attribute_shift_register_high_ & 0xFF00) | ((background_tile_attribute_latch_ & 0b10) ? 0xFF : 0x00);
    }

    inline void maybe_update_background_pattern_shift_registers(bool enabled)
    {
      if(enabled)
      {
        background_pattern_shift_register_low_ <<= 1;
        background_pattern_shift_register_high_ <<= 1;

        background_attribute_shift_register_low_ <<= 1;
        background_attribute_shift_register_high_ <<= 1;
      }
    }

    inline void maybe_update_active_sprites(bool enabled)
    {
      if(enabled)
      {
        for(std::uint8_t i = 0; i < active_sprites_.size(); ++i)
        {
          if(active_sprites_[i].second != 0)
          {
            active_sprites_[i].second--;
          }
          else
          {
            sprite_pattern_shift_register_low_[i]  <<= 1;
            sprite_pattern_shift_register_high_[i] <<= 1;
          }
        }
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
        std::uint16_t mux = 0x8000 >> fine_x;

        // construct the background pixel from two bit planes
        std::uint8_t pixel_plane_0 = (background_pattern_shift_register_low_  & mux) != 0;
        std::uint8_t pixel_plane_1 = (background_pattern_shift_register_high_ & mux) != 0;
        pixel = (pixel_plane_1 << 1) | pixel_plane_0;

        // construct the palette from two bit planes
        std::uint16_t palette_plane_0 = (background_attribute_shift_register_low_  & mux) != 0;
        std::uint16_t palette_plane_1 = (background_attribute_shift_register_high_ & mux) != 0;

        palette_idx = (palette_plane_1 << 1) | palette_plane_0;
      }

      return {palette_idx,pixel};
    }

    inline static std::uint8_t maybe_flip_byte(bool flip, std::uint8_t value)
    {
      std::uint8_t result = value;

      if(flip)
      {
        // see https://stackoverflow.com/a/2602885/722294
        result = (result & 0xF0) >> 4 | (result & 0x0F) << 4;
        result = (result & 0xCC) >> 2 | (result & 0x33) << 2;
        result = (result & 0xAA) >> 1 | (result & 0x55) << 1;
      }

      return result;
    }

    // this computes the address of the byte containing the given sprite's row of pattern data
    inline static std::uint16_t sprite_row_address(control_register_t control, object_attribute sprite, std::uint8_t row)
    {
      assert(row >= 0);

      // 8x16 sprites ignore the control register and select a pattern table from the object attributes
      bool sprite_pattern_table = control.sprite_size ? sprite.tall_sprite_pattern_table() : control.sprite_pattern_table_address;

      // the row of the tile we read from is the row of the sprite mod the height of a tile
      std::uint8_t tile_row = row % 8;

      // figure out if we need to flip the tile row we read from
      if(sprite.flip_vertically())
      {
        tile_row = 7 - tile_row;
      }

      // this index points to the top tile of the sprite
      // note that 8x8 sprites have no bottom tile
      std::uint8_t tile_id = control.sprite_size ? sprite.tall_sprite_tile_id() : sprite.tile_id;

      // figure out if we need to point to the bottom tile of the sprite
      if(control.sprite_size)
      {
        if((row <  8 and sprite.flip_vertically()) or
           (row >= 8 and not sprite.flip_vertically()))
        {
          // we need to read from the bottom tile, which is simply
          // the tile after the top tile
          tile_id++;
        }
      }

      return 4096 * sprite_pattern_table + 16 * tile_id + tile_row;
    }

    inline void evaluate_sprites_for_next_scanline(bool use_tall_sprites, status_register_t& status)
    {
      std::uint8_t sprite_height = use_tall_sprites ? 16 : 8;

      active_sprites_.clear();
      sprite_pattern_shift_register_low_.fill(0);
      sprite_pattern_shift_register_high_.fill(0);

      for(std::uint8_t i = 0; i < object_attributes_.size(); ++i)
      {
        // XXX we need to map scanline 261 -> -1 in this difference or figure out why OLC does not include 
        //std::int16_t diff = ((int16_t)current_scanline_ - (int16_t)object_attributes_[nOAMEntry].y_position);
        // XXX what happens to this when current_scanline_ is 261?
        int temp = (current_scanline_ == 261) ? -1 : current_scanline_;
        std::int16_t diff = ((int16_t)temp - (int16_t)object_attributes_[i].y_position);

        if(0 <= diff and diff < sprite_height)
        {
          if(active_sprites_.size() < active_sprites_.capacity())
          {
            active_sprites_.push_back({i, object_attributes_[i].x_position});
          }
          else
          {
            status.sprite_overflow = true;
            break;
          }
        }
      }
    }

    inline void read_sprites_for_next_scanline(control_register_t control)
    {
      for(std::uint8_t i = 0; i < active_sprites_.size(); ++i)
      {
        // XXX what happens when current_scanline_ is 261 below?
        assert(current_scanline_ != 261);

        // sprite_row is always nonnegative for active sprites
        if(current_scanline_ < active_sprite(i).y_position)
        {
          fmt::print("ppu_renderer::read_sprites_for_next_scanline: sprite_row is negative for current_scanline_: {}\n", current_scanline_);
        }

        assert(current_scanline_ >= active_sprite(i).y_position);
        std::uint8_t sprite_row = current_scanline_ - active_sprite(i).y_position;

        // get the address of the row we need
        std::uint16_t address = sprite_row_address(control, active_sprite(i), sprite_row);

        // the zeroth byte at this address is the low bitplane
        sprite_pattern_shift_register_low_[i] = maybe_flip_byte(active_sprite(i).flip_horizontally(), read(address + 0));

        // eight bytes later is the high bitplane
        sprite_pattern_shift_register_high_[i] = maybe_flip_byte(active_sprite(i).flip_horizontally(), read(address + 8));
      }
    }

    // returns (sprite_idx, prioritize_foreground, (palette_idx, color_idx))
    inline std::tuple<std::uint8_t,bool,std::pair<std::uint8_t,std::uint8_t>> maybe_render_foreground(bool enabled)
    {
      std::uint8_t sprite_idx = 8;
      bool prioritize_foreground = false;
      std::uint8_t palette_idx = 0;
      std::uint8_t color_idx = 0;

      if(enabled)
      {
        for(uint8_t i = 0; i < active_sprites_.size(); ++i)
        {
          if(active_sprite(i).x_position == 0) 
          {
            // combine the bitplanes stored in the shift registers
            uint8_t color_idx_low  = (sprite_pattern_shift_register_low_[i]  & 0x80) != 0;
            uint8_t color_idx_high = (sprite_pattern_shift_register_high_[i] & 0x80) != 0;
            color_idx = (color_idx_high << 1) | color_idx_low;
            
            // stop at the first non-transparent sprite we encounter
            if(color_idx != 0)
            {
              sprite_idx = i;
              prioritize_foreground = active_sprite(i).prioritize_foreground();
              palette_idx = active_sprite(i).palette_id();
              break;
            }				
          }
        }		
      }

      return {sprite_idx, prioritize_foreground, {palette_idx,color_idx}};
    }

    // foreground: (fg_palette_idx, fg_color_idx)
    // background: (bg_palette_idx, bg_color_idx)
    // returns (palette_idx, color_idx)
    inline static std::pair<std::uint8_t,std::uint8_t> composite(bool prioritize_foreground,
                                                                 std::pair<std::uint8_t,std::uint8_t> foreground,
                                                                 std::pair<std::uint8_t,std::uint8_t> background)
    {
      auto result = std::make_pair(0,0);

      if(background.second == 0 and foreground.second != 0)
      {
        result = foreground;
      }
      else if(background.second != 0 and foreground.second == 0)
      {
        result = background;
      }
      else if(background.second != 0 and foreground.second != 0)
      {
        result = prioritize_foreground ? foreground : background;
      }

      return result;
    }

    inline object_attribute active_sprite(std::uint8_t i) const
    {
      object_attribute result = object_attributes_[active_sprites_[i].first];
      result.x_position = active_sprites_[i].second;
      return result;
    }
};


} // end nes

