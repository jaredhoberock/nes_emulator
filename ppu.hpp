#pragma once

#include "graphics_bus.hpp"
#include "ppu_renderer.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>

class ppu
{
  public:
    constexpr static int framebuffer_width = 256;
    constexpr static int framebuffer_height = 240;

    using rgb = ppu_renderer::rgb;

    ppu(graphics_bus& gb, std::span<rgb, framebuffer_width*framebuffer_height> framebuffer)
      : nmi{},
        bus_{gb},
        renderer_{bus_,framebuffer},
        control_register_{},
        mask_register_{},
        status_register_{},
        oam_memory_address_register_{},
        oam_memory_data_register_{},
        data_buffer_{},
        address_latch_{},
        vram_address_{},
        tram_address_{},
        fine_x_{}
    {}

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
      return 0;
    }

    inline void set_scroll_register(std::uint8_t value)
    {
      if(address_latch_)
      {
        tram_address_.fine_y = value & 0x07;
        tram_address_.coarse_y = value >> 3;
      }
      else
      {
        fine_x_ = value & 0x07;
        tram_address_.coarse_x = value >> 3;
      }

      address_latch_ = !address_latch_;
    }

    inline void set_address_register(std::uint8_t value)
    {
      if(address_latch_)
      {
        // write to low byte
        tram_address_.as_uint16 = (tram_address_.as_uint16 & 0xFF00) | value;
        vram_address_ = tram_address_;
      }
      else
      {
        // write to high byte
        tram_address_.as_uint16 = (value << 8) | (tram_address_.as_uint16 & 0x00FF);
      }

      address_latch_ = !address_latch_;
    }

    inline std::uint8_t data_register()
    {
      std::uint8_t result = read(vram_address_.as_uint16);

      // reads are delayed by one read in this range
      if(vram_address_.as_uint16 < 0x3F00)
      {
        std::swap(result, data_buffer_);
      }

      // increment address register
      vram_address_.as_uint16 += control_register_.vram_address_increment_mode ? 32 : 1;

      return result;
    }

    inline void set_data_register(std::uint8_t value)
    {
      write(vram_address_.as_uint16, value);

      // increment address register
      vram_address_.as_uint16 += control_register_.vram_address_increment_mode ? 32 : 1;
    }

    // XXX eliminate this function
    inline std::array<rgb, 4> palette_as_image(int palette) const
    {
      return renderer_.palette_as_image(palette);
    }

    // XXX eliminate this function
    inline rgb as_rgb(int palette, std::uint8_t pixel) const
    {
      return renderer_.as_rgb(palette, pixel);
    }

    void step_cycle()
    {
      std::optional vertical_blank = renderer_.step_cycle(mask_register_.show_background, mask_register_.show_sprites,
                                                          vram_address_, tram_address_,
                                                          control_register_.background_pattern_table_address, fine_x_);

      if(vertical_blank)
      {
        status_register_.in_vertical_blank_period = vertical_blank.value();
        if(control_register_.generate_nmi and status_register_.in_vertical_blank_period)
        {
          nmi = true;
        }
      }
    }

    bool nmi;

  private:
    inline std::uint8_t read(std::uint16_t address) const
    {
      std::uint8_t result = 0;

      if(0x3F00 <= address and address < 0x4000)
      {
        // handle palette reads ourself
        address &= 0x001F;
        if(address == 0x0010) address = 0x0000;
        if(address == 0x0014) address = 0x0004;
        if(address == 0x0018) address = 0x0008;
        if(address == 0x001C) address = 0x000C;

        result = renderer_.palette(address);
      }
      else
      {
        result = bus_.read(address);
      }

      return result;
    }

    void write(std::uint16_t address, std::uint8_t value)
    {
      if(0x3F00 <= address and address < 0x4000)
      {
        // handle palette writes ourself
        address &= 0x001F;
        if(address == 0x0010) address = 0x0000;
        if(address == 0x0014) address = 0x0004;
        if(address == 0x0018) address = 0x0008;
        if(address == 0x001C) address = 0x000C;

        renderer_.set_palette(address, value);
      }
      else
      {
        bus_.write(address, value);
      }
    }

    graphics_bus& bus_;
    ppu_renderer renderer_;

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

    union status_register_t
    {
      std::uint8_t as_byte;
      struct
      {
        std::uint8_t unused : 5;
        bool sprite_overflow : 1;
        bool sprite_0_hit : 1;
        bool in_vertical_blank_period : 1;
      };

      status_register_t() : as_byte{} {}
    };

    static_assert(sizeof(status_register_t) == sizeof(std::uint8_t));

    // register state
    control_register_t control_register_;
    mask_register_t mask_register_;
    status_register_t status_register_;
    std::uint8_t oam_memory_address_register_;
    std::uint8_t oam_memory_data_register_;
    std::uint8_t data_buffer_;
    bool address_latch_;
    ppu_renderer::loopy_register vram_address_;
    ppu_renderer::loopy_register tram_address_;
    std::uint8_t fine_x_;
};

