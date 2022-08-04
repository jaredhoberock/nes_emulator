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

