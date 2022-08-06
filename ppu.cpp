#include "graphics_bus.hpp"
#include "ppu.hpp"
#include <cstdint>

std::uint8_t ppu::read(std::uint16_t address) const
{
  return bus_.read(address);
}


void ppu::write(std::uint16_t address, std::uint8_t value) const
{
  bus_.write(address, value);
}


void ppu::step_cycle()
{
  std::optional vertical_blank = renderer_.step_cycle(mask_register_.show_background, mask_register_.show_sprites,
                                                      vram_address_, tram_address_,
                                                      control_register_.background_pattern_table_address, fine_x_);

  if(vertical_blank)
  {
    status_register_.in_vertical_blank_period = *vertical_blank;
    if(control_register_.generate_nmi and status_register_.in_vertical_blank_period)
    {
      nmi = true;
    }
  }
}

