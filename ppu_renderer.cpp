#include "ppu_renderer.hpp"


// see https://www.nesdev.org/wiki/PPU_rendering
// and https://www.nesdev.org/wiki/File:Ntsc_timing.png
std::optional<bool> ppu_renderer::step_cycle(bool show_background, bool show_sprites,
                                             loopy_register& vram_address, loopy_register tram_address,
                                             bool background_pattern_table_address, std::uint8_t fine_x)
{
  constexpr std::uint16_t num_cycles_per_scanline = 341;
  constexpr std::uint16_t num_scanlines_per_frame = 262;

  // if we're within the visible frame or within the prerender scanline
  if(current_scanline_ < 240 or current_scanline_ == 261)
  {
    if(current_scanline_ == 0 and current_col_ == 0)
    {
      // odd frame column skip
      current_col_ = 1;
    }

    if((2 <= current_col_ and current_col_ < 256) or (321 <= current_col_ and current_col_ < 338))
    {
      maybe_update_background_pattern_shift_registers(show_background);

      switch((current_col_ - 1) % 8)
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

    if(current_col_ == 256)
    {
      maybe_increment_y(show_background or show_sprites, vram_address);
    }

    if(current_col_ == 257)
    {
      load_background_shift_registers();
      maybe_copy_x(show_background or show_sprites, vram_address, tram_address);
    }

    if(current_col_ == 338 or current_col_ == 340)
    {
      // this is a superfluous read at the end of a scanline
      read_next_background_tile_id(vram_address);
    }

    if(current_scanline_ == 261 and 280 <= current_col_ and current_col_ < 305)
    {
      maybe_copy_y(show_background or show_sprites, vram_address, tram_address);
    }
  }

  // decide whether to set the vertical blank flag
  std::optional<bool> result;
  if(current_col_ == 1)
  {
    if(current_scanline_ == 261)
    {
      result = false;
    }
    else if(current_scanline_ == 241)
    {
      result = true;
    }
  }

  // render the pixel color
  auto [background_palette, background_pixel] = maybe_render_background(show_background, fine_x);
  
  // maybe write to the framebuffer
  // the PPU idles on row 0, so we subtract 1 from current_col_ to find the pixel's x coordintae
  if(0 <= current_scanline_ and current_scanline_ <  framebuffer_height and
     0 <  current_col_      and current_col_      <= framebuffer_width)
  {
    std::uint16_t pixel_idx = current_scanline_ * framebuffer_width + current_col_ - 1;
    framebuffer_[pixel_idx] = as_rgb(background_palette, background_pixel);
  }

  current_col_++;
  if(current_col_ == num_cycles_per_scanline)
  {
    current_col_ = 0;

    current_scanline_++;
    if(current_scanline_ == num_scanlines_per_frame)
    {
      current_scanline_ = 0;
    }
  }

  return result;
}

