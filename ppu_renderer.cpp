#include "ppu_renderer.hpp"
#include <cassert>


// see https://www.nesdev.org/wiki/PPU_rendering
// and https://www.nesdev.org/wiki/File:Ntsc_timing.png
bool ppu_renderer::step_cycle(bool show_background, bool show_sprites,
                              loopy_register& vram_address, loopy_register tram_address,
                              bool background_pattern_table_address, std::uint8_t fine_x,
                              control_register_t control, status_register_t& status, mask_register_t mask)
{
  constexpr std::uint16_t num_cycles_per_scanline = 341;
  constexpr std::uint16_t num_scanlines_per_frame = 262;

  // if we're within the visible frame or within the prerender scanline
  if(current_scanline_ < 240 or current_scanline_ == 261)
  {
    if(current_scanline_ == 0 and current_scanline_cycle_ == 0)
    {
      // odd frame cycle skip
      current_scanline_cycle_ = 1;
    }

    if((2 <= current_scanline_cycle_ and current_scanline_cycle_ < 258) or (321 <= current_scanline_cycle_ and current_scanline_cycle_ < 338))
    {
      maybe_update_background_pattern_shift_registers(show_background);
      maybe_update_active_sprites(show_sprites and (current_scanline_cycle_ < 258));

      switch((current_scanline_cycle_ - 1) % 8)
      {
        case 0:
        {
          load_background_shift_registers();
          read_next_background_tile_id(vram_address);
          break;
        }

        case 2:
        {
          read_next_background_tile_attribute(vram_address);
          break;
        }

        case 4:
        {
          read_next_background_tile_lsb(vram_address, background_pattern_table_address);
          break;
        }

        case 6:
        {
          read_next_background_tile_msb(vram_address, background_pattern_table_address);
          break;
        }

        case 7:
        {
          maybe_increment_x(show_background or show_sprites, vram_address);
          break;
        }
      }
    }

    if(current_scanline_cycle_ == 256)
    {
      maybe_increment_y(show_background or show_sprites, vram_address);
    }

    if(current_scanline_cycle_ == 257)
    {
      load_background_shift_registers();
      maybe_copy_x(show_background or show_sprites, vram_address, tram_address);
    }

    if(current_scanline_cycle_ == 338 or current_scanline_cycle_ == 340)
    {
      // this is a superfluous read at the end of a scanline
      read_next_background_tile_id(vram_address);
    }

    if(current_scanline_ == 261 and 280 <= current_scanline_cycle_ and current_scanline_cycle_ < 305)
    {
      maybe_copy_y(show_background or show_sprites, vram_address, tram_address);
    }
  }

  // evaluate sprites at the end of the scanline
  if(current_scanline_cycle_ == 257 and (current_scanline_ < 239 or current_scanline_ == 261))
  {
    evaluate_sprites_for_next_scanline(control.sprite_size, status);
  }

  // on the last cycle of a scanline, read the next scanline's sprite data
  if(current_scanline_cycle_ == 340 and (current_scanline_ < 239 or current_scanline_ == 261))
  {
    read_sprites_for_next_scanline(control);
  }

  // render the foreground color
  auto [sprite_idx, prioritize_foreground, foreground] = maybe_render_foreground(show_sprites);

  // render the background color
  auto background = maybe_render_background(show_background, fine_x);

  // composite foreground and background
  auto [palette_idx, color_idx] = composite(prioritize_foreground, foreground, background);

  // determine whether sprite zero was "hit"
  if(active_sprites_[0].first == 0 and sprite_idx == 0)
  {
    // the scanline intersected sprite zero

    if(background.second != 0 and foreground.second != 0)
    {
      // both background and foreground were non-transparent

      if(show_background and show_sprites)
      {
        // both background and sprites were enabled

        // The left edge of the screen has specific switches to control
        // its appearance. This is used to smooth inconsistencies when
        // scrolling (since sprites x coord must be >= 0)
        if(not (mask.show_background_in_leftmost_8_pixels_of_screen or mask.show_sprites_in_leftmost_8_pixels_of_screen))
        {
          if(9 <= current_scanline_cycle_ and current_scanline_cycle_ < 258)
          {
            status.sprite_zero_hit = 1;
          }
        }
        else
        {
          if(1 <= current_scanline_cycle_ and current_scanline_cycle_ < 258)
          {
            status.sprite_zero_hit = 1;
          }
        }
      }
    }
  }
  
  // maybe write to the framebuffer
  // the PPU idles on row 0, so we subtract 1 from current_scanline_cycle_ to find the pixel's x coordintae
  if(0 <= current_scanline_       and current_scanline_       <  framebuffer_height and
     0 <  current_scanline_cycle_ and current_scanline_cycle_ <= framebuffer_width)
  {
    std::uint16_t pixel_idx = current_scanline_ * framebuffer_width + current_scanline_cycle_ - 1;
    framebuffer_[pixel_idx] = as_rgb(palette_idx, color_idx);
  }

  // decide the result and whether to update the status register
  bool entered_vertical_blank_period = false;
  if(current_scanline_cycle_ == 1)
  {
    if(current_scanline_ == 241)
    {
      status.in_vertical_blank_period = true;
      entered_vertical_blank_period = true;
    }
    else if(current_scanline_ == 261)
    {
      status.sprite_overflow = false;
      status.sprite_zero_hit = false;
      status.in_vertical_blank_period = false;
    }
  }
  
  // update the current scanline state
  current_scanline_cycle_++;
  if(current_scanline_cycle_ == num_cycles_per_scanline)
  {
    current_scanline_cycle_ = 0;

    current_scanline_++;
    if(current_scanline_ == num_scanlines_per_frame)
    {
      current_scanline_ = 0;
    }
  }

  return entered_vertical_blank_period;
}

