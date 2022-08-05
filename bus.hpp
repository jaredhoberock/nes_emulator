#pragma once

#include "cartridge.hpp"
#include "ppu.hpp"
#include <array>
#include <cstdint>
#include <fmt/format.h>
#include <stdexcept>


class bus
{
  private:
    // XXX maybe all of these should be references
    std::array<uint8_t, 2048> internal_ram_;
    cartridge& cart_;
    ppu& ppu_;

  public:
    bus(cartridge& cart, ppu& p)
      : internal_ram_{{}}, cart_{cart}, ppu_{p}
    {}

    inline std::array<std::uint8_t,256> zero_page() const
    {
      std::array<std::uint8_t,256> result;
      std::copy_n(internal_ram_.begin(), result.size(), result.begin());
      return result;
    }

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

        switch(address & 0x0007)
        {
          case 0: result = ppu_.control_register(); break;
          case 1: result = ppu_.mask_register(); break;
          case 2: result = ppu_.status_register(); break;
          case 3: result = ppu_.oam_memory_address_register(); break;
          case 4: result = ppu_.oam_memory_data_register(); break;
          case 5: result = ppu_.scroll_register(); break;
          case 6: break; // the address register is not ordinarily readable, but the cpu's log function issues reads from every address it stores to, so allow it
          case 7: result = ppu_.data_register(); break;
          default:
          {
            throw std::runtime_error(fmt::format("bus::read: Invalid PPU address: {:04X}", address));
          }
        }
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

        switch(address & 0x0007)
        {
          case 0: ppu_.set_control_register(value); break;
          case 1: ppu_.set_mask_register(value); break;
          case 3: ppu_.set_oam_memory_address_register(value); break;
          case 4: ppu_.set_oam_memory_data_register(value); break;
          case 5: ppu_.set_scroll_register(value); break;
          case 6: ppu_.set_address_register(value); break;
          case 7: ppu_.set_data_register(value); break;
          default:
          {
            throw std::runtime_error(fmt::format("bus::write: Invalid PPU address: {:04X}", address));
          }
        }
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

