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
    std::uint8_t read(std::uint16_t address) const;

    void write(std::uint16_t address, std::uint8_t value) const;

    graphics_bus& bus_;
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

