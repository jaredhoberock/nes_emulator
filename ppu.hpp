#pragma once

#include <array>
#include <cstdint>

class ppu
{
  public:
    constexpr static int framebuffer_width = 256;
    constexpr static int framebuffer_height = 240;

    struct rgb
    {
      std::uint8_t r,g,b;
    };

    ppu()
      : current_scanline_{0},
        current_column_{0}
    {
      for(int row = 0; row < framebuffer_height; ++row)
      {
        for(int col = 0; col < framebuffer_width; ++col)
        {
          framebuffer_[row * framebuffer_width + col] = {0, 255, 0};
        }
      }
    }

    const rgb* framebuffer_data() const
    {
      return framebuffer_.data();
    }

    inline void step_pixel()
    {
      framebuffer_[current_scanline_ * framebuffer_width + current_column_] = random_rgb();

      ++current_column_;
      if(current_column_ == framebuffer_width)
      {
        current_column_ = 0;
        ++current_scanline_;

        if(current_scanline_ == framebuffer_height)
        {
          current_scanline_ = 0;
        }
      }
    }

  private:
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
};

