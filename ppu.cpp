#include "ppu.hpp"
#include <cstdint>

std::uint8_t ppu::read(std::uint16_t address) const
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


void ppu::write(std::uint16_t address, std::uint8_t value)
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

