#pragma once

#include <array>
#include <cstdint>

// use a forward declaration of graphics_bus here because graphics_bus.hpp #includes this file
class graphics_bus;

class ppu
{
  public:
    constexpr static int framebuffer_width = 256;
    constexpr static int framebuffer_height = 240;

    struct rgb
    {
      std::uint8_t r,g,b;
    };

    ppu(graphics_bus& gb)
      : bus_{gb},
        palette_{},
        current_scanline_{0},
        current_column_{0},
        control_register_{},
        mask_register_{},
        status_register_{},
        oam_memory_address_register_{},
        oam_memory_data_register_{},
        scroll_register_{},
        address_register_{},
        data_buffer_{},
        address_latch_{}
    {
      for(int row = 0; row < framebuffer_height; ++row)
      {
        for(int col = 0; col < framebuffer_width; ++col)
        {
          framebuffer_[row * framebuffer_width + col] = {0, 255, 0};
        }
      }
    }

    inline std::uint8_t control_register() const
    {
      return control_register_.as_byte;
    }

    inline void set_control_register(std::uint8_t value)
    {
      control_register_.as_byte = value;
    }

    inline std::uint8_t mask_register() const
    {
      return mask_register_.as_byte;
    }

    inline void set_mask_register(std::uint8_t value)
    {
      mask_register_.as_byte = value;
    }

    inline std::uint8_t status_register()
    {
      // the lower bytes of the status register often include bits
      // from the last value of the data register
      std::uint8_t result = (status_register_.as_byte & 0xE0) | (data_buffer_ & 0x1F);

      // reading the status register clears the vertical blank bit
      status_register_.in_vertical_blank_period = false;

      // reading the status register also clears the address latch
      address_latch_ = false;

      return result;
    }

    inline std::uint8_t oam_memory_address_register() const
    {
      return oam_memory_address_register_;
    }

    inline void set_oam_memory_address_register(std::uint8_t value)
    {
      oam_memory_address_register_ = value;
    }

    inline std::uint8_t oam_memory_data_register() const
    {
      return oam_memory_data_register_;
    }

    inline void set_oam_memory_data_register(std::uint8_t value)
    {
      oam_memory_data_register_ = value;
    }

    inline std::uint8_t scroll_register() const
    {
      return scroll_register_;
    }

    inline void set_scroll_register(std::uint8_t value)
    {
      scroll_register_ = value;
    }

    inline void set_address_register(std::uint8_t value)
    {
      if(address_latch_)
      {
        // write to low byte
        address_register_ = (address_register_ & 0xFF00) | value;
      }
      else
      {
        // write to high byte
        address_register_ = (value << 8) | (address_register_ & 0x00FF);
      }

      address_latch_ = !address_latch_;
    }

    inline std::uint8_t data_register()
    {
      std::uint8_t result = read(address_register_);

      // reads are delayed by one read in this range
      if(address_register_ < 0x3F00)
      {
        std::swap(result, data_buffer_);
      }

      // increment address register
      address_register_ += control_register_.vram_address_increment_mode ? 32 : 1;

      return result;
    }

    inline void set_data_register(std::uint8_t value)
    {
      write(address_register_, value);

      // increment address register
      address_register_ += control_register_.vram_address_increment_mode ? 32 : 1;
    }

    inline const rgb* framebuffer_data() const
    {
      return framebuffer_.data();
    }

    inline std::uint8_t palette(std::uint8_t i) const
    {
      return palette_[i];
    }

    inline void set_palette(std::uint8_t i, std::uint8_t value)
    {
      palette_[i] = value;
    }

    inline std::array<rgb, 4> palette_as_image(int palette) const
    {
      return {as_rgb(palette,0), as_rgb(palette,1), as_rgb(palette,2), as_rgb(palette,3)};
    }

    inline rgb as_rgb(int palette, std::uint8_t pixel) const
    {
      return system_palette_[read(palette_base_address_ + 4 * palette + pixel)];
    }

    inline void step_cycle()
    {
      if(current_scanline_ == 241 and current_column_ == 1)
      {
        status_register_.in_vertical_blank_period = true;
        if(control_register_.generate_nmi)
        {
          nmi = true;
        }
      }

      if(current_scanline_ == 261 and current_column_ == 1)
      {
        status_register_.in_vertical_blank_period = false;
      }

      if(0 <= current_scanline_ and current_scanline_ < framebuffer_height and
         0 <= current_column_   and current_column_   < framebuffer_width)
      {
        framebuffer_[current_scanline_ * framebuffer_width + current_column_] = random_rgb();
      }

      ++current_column_;
      if(current_column_ == 341)
      {
        current_column_ = 0;
        ++current_scanline_;

        if(current_scanline_ == 262)
        {
          current_scanline_ = 0;
        }
      }
    }

    bool nmi;

  private:
    static constexpr std::uint16_t palette_base_address_ = 0x3F00;

    // https://www.nesdev.org/wiki/PPU_palettes#2C02
    static constexpr std::array<rgb,64> system_palette_ = {{
      { 84, 84, 84}, {  0, 30,116}, {  8, 16,144}, { 48,  0,136}, { 68,  0,100}, { 92,  0, 48}, { 84,  4,  0}, { 60, 24,  0}, { 32, 42,  0}, {  8, 58,  0}, {  0, 64,  0}, {  0, 60,  0}, {  0, 50, 60}, {  0,  0,  0}, {0,0,0}, {0,0,0},
      {152,150,152}, {  8, 76,196}, { 48, 50,236}, { 92, 30,228}, {136, 20,176}, {160, 20,100}, {152, 34, 32}, {120, 60,  0}, { 84, 90,  0}, { 40,114,  0}, {  8,124,  0}, {  0,118, 40}, {  0,102,120}, {  0,  0,  0}, {0,0,0}, {0,0,0},
      {236,238,236}, { 76,154,236}, {120,124,236}, {176, 98,236}, {228, 84,236}, {236, 88,180}, {236,106,100}, {212,136, 32}, {160,170,  0}, {116,196,  0}, { 76,208, 32}, { 56,204,108}, { 56,180,204}, { 60, 60, 60}, {0,0,0}, {0,0,0},
      {236,238,236}, {168,204,236}, {188,188,236}, {212,178,236}, {236,174,236}, {236,174,212}, {236,180,176}, {228,196,144}, {204,210,120}, {180,222,120}, {168,226,144}, {152,226,180}, {160,214,228}, {160,162,160}, {0,0,0}, {0,0,0}
    }};

    std::uint8_t read(std::uint16_t address) const;

    void write(std::uint16_t address, std::uint8_t value) const;

    graphics_bus& bus_;
    std::array<std::uint8_t, 32> palette_;
    // XXX should the framebuffer should be on the graphics bus?
    std::array<rgb, framebuffer_width * framebuffer_height> framebuffer_;
    int current_scanline_;
    int current_column_;

    rgb random_rgb() const
    {
      std::uint8_t r = (char)rand();
      std::uint8_t g = (char)rand();
      std::uint8_t b = (char)rand();
      return {r,g,b};
    }

    union
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
    } control_register_;

    union
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
    } mask_register_;

    union
    {
      std::uint8_t as_byte;
      struct
      {
        int  unused : 5;
        bool sprite_overflow : 1;
        bool sprite_0_hit : 1;
        bool in_vertical_blank_period : 1;
      };
    } status_register_;

    // register state
    std::uint8_t  oam_memory_address_register_;
    std::uint8_t  oam_memory_data_register_;
    std::uint8_t  scroll_register_;
    std::uint16_t address_register_;

    std::uint8_t data_buffer_;

    // this indicates whether we're writing to the low or high byte of address_register_
    bool address_latch_;
};

