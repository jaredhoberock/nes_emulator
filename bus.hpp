#pragma once

#include "cartridge.hpp"
#include "ppu.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>


class bus
{
  private:
    // XXX maybe all of these should be references
    std::array<uint8_t, 0x800> internal_ram_;
    cartridge cart_;
    ppu& ppu_;

  public:
    bus(cartridge cart, ppu& p)
      : cart_{cart}, ppu_{p}
    {}

    std::uint8_t read(std::uint16_t address) const
    {
      std::uint8_t result = 0;

      if(0x0000 <= address and address < 0x2000)
      {
        // this bitwise and implements mirroring
        result = internal_ram_[address & 0x07FF];
      }
      else if(0x2000 <= address and address < 0x4000)
      {
        // ppu here, mirrored every 8 bytes
        // this bitwise and implements mirroring
        result = ppu_.read_register(address & 0x0007);
      }
      else if(0x4000 <= address and address < 0x4018)
      {
        // apu and i/o
        // XXX force this result to be 0xFF to match nestest while debugging
        result = 0xFF;
      }
      else if(0x4018 <= address and address < 0x4020)
      {
        // apu and i/o functionality that is normally disabled
        // XXX force this result to be 0xFF to match nestest while debugging
        result = 0xFF;
      }
      else if(0x4020 <= address)
      {
        // cartridge
        result = cart_.read(address);
      }
      else
      {
        throw std::runtime_error("bus::read: Bad address");
      }

      return result;
    }

    void write(std::uint16_t address, std::uint8_t value)
    {
      if(0x0000 <= address and address < 0x2000)
      {
        // this bitwise and implements mirroring
        internal_ram_[address & 0x07FF] = value;
      }
      else if(0x2000 <= address and address < 0x4000)
      {
        // ppu here, mirrored every 8 bytes
        // this bitwise and implements mirroring
        ppu_.write_register(address & 0x0007, value);
      }
      else if(0x4000 <= address and address < 0x4018)
      {
        // apu and i/o
      }
      else if(0x4018 <= address and address < 0x4020)
      {
        // apu and i/o functionality that is normally disabled
      }
      else if(0x4020 <= address)
      {
        // cartridge
        cart_.write(address, value);
      }
      else
      {
        throw std::runtime_error("bus::write: Bad address");
      }
    }
};

