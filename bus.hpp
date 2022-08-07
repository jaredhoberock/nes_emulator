#pragma once

#include "cartridge.hpp"
#include "ppu.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <fmt/format.h>
#include <span>
#include <stdexcept>


class bus
{
  private:
    std::span<const std::uint8_t,2> controllers_;
    std::array<std::uint8_t,2> controller_shift_registers_;
    cartridge& cart_;
    std::span<uint8_t, 2048> wram_;
    ppu& ppu_;
    std::uint8_t dma_page_;
    std::uint8_t dma_address_;
    std::uint8_t dma_data_;
    bool dma_in_progress_;
    bool dma_can_begin_;

  public:
    bus(std::span<const std::uint8_t,2> controllers,
        cartridge& cart,
        std::span<std::uint8_t,2048> wram,
        ppu& p)
      : controllers_{controllers},
        cart_{cart},
        wram_{wram},
        ppu_{p},
        dma_page_{},
        dma_address_{},
        dma_data_{},
        dma_in_progress_{},
        dma_can_begin_{}
    {}

    inline bool dma_in_progress() const
    {
      return dma_in_progress_;
    }

    // XXX it might make more sense to make this a member of the cpu type
    //     but i'm leaving it here because mos6502 doesn't currently have
    //     any NES-specific details, such as the interaction with ports
    //     $OAMDATA below
    inline void step_dma_cycle(std::size_t cpu_cycle)
    {
      // we assume that dma_in_progress_ is true
      assert(dma_in_progress_);

      // dma can only begin on an even cycle
      if(not dma_can_begin_)
      {
        if(cpu_cycle % 2 == 0)
        {
          dma_can_begin_ = true;
        }
      }

      if(dma_can_begin_)
      {
        if(cpu_cycle % 2 == 0)
        {
          // on even clock cycles, read
          dma_data_ = read((dma_page_ << 8) + dma_address_);
          ++dma_address_;
        }
        else
        {
          // on odd clock cycles, write to 0x2004
          // XXX 0x2004 is $OAMDATA
          write(0x2004, dma_data_);

          // we're finished once dma address rolls around to zero
          if(dma_address_ == 0)
          {
            dma_in_progress_ = false;
            dma_can_begin_ = false;
          }
        }
      }
    }

    inline std::uint8_t read(std::uint16_t address)
    {
      std::uint8_t result = 0;

      if(0x0000 <= address and address < 0x2000)
      {
        // this bitwise and implements mirroring
        result = wram_[address & 0x07FF];
      }
      else if(0x2000 <= address and address < 0x4000)
      {
        // ppu here, mirrored every 8 bytes

        switch(address & 0x0007)
        {
          case 0: result = ppu_.control_register(); break;
          case 1: result = ppu_.mask_register(); break;
          case 2: result = ppu_.status_register(); break;
          case 3: result = ppu_.oam_address_register(); break;
          case 4: result = ppu_.oam_data_register(); break;
          case 5: result = ppu_.scroll_register(); break;
          case 6: break; // the address register is not ordinarily readable, but the cpu's log function issues reads from every address it stores to, so allow it
          case 7: result = ppu_.data_register(); break;
          default:
          {
            throw std::runtime_error(fmt::format("bus::read: Invalid PPU address: {:04X}", address));
          }
        }
      }
      else if(0x4000 <= address and address < 0x4016)
      {
        // apu and i/o
      }
      else if(0x4016 <= address and address < 0x4018)
      {
        // controller state
        // reading from these addresses reads the msb from one of the shift registers
        result = (controller_shift_registers_[address & 0x0001] & 0b10000000) ? 1 : 0;

        // shift left
        controller_shift_registers_[address & 0x0001] <<= 1;
      }
      else if(0x4018 <= address and address < 0x4020)
      {
        // apu and i/o functionality that is normally disabled
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

    inline void write(std::uint16_t address, std::uint8_t value)
    {
      if(0x0000 <= address and address < 0x2000)
      {
        // this bitwise and implements mirroring
        wram_[address & 0x07FF] = value;
      }
      else if(0x2000 <= address and address < 0x4000)
      {
        // ppu here, mirrored every 8 bytes

        switch(address & 0x0007)
        {
          case 0: ppu_.set_control_register(value); break;
          case 1: ppu_.set_mask_register(value); break;
          case 3: ppu_.set_oam_address_register(value); break;
          case 4: ppu_.set_oam_data_register(value); break;
          case 5: ppu_.set_scroll_register(value); break;
          case 6: ppu_.set_address_register(value); break;
          case 7: ppu_.set_data_register(value); break;
          default:
          {
            throw std::runtime_error(fmt::format("bus::write: Invalid PPU address: {:04X}", address));
          }
        }
      }
      else if(0x4000 <= address and address < 0x4014)
      {
        // apu and i/o
      }
      else if(0x4014 == address)
      {
        // dma
        dma_page_ = value;
        dma_address_ = 0;
        dma_in_progress_ = true;
      }
      else if(0x4015 == address)
      {
        // sound channels enable
      }
      else if(0x4016 <= address and address < 0x4018)
      {
        // writing to these addresses captures the current controller state into the shift registers
        controller_shift_registers_[address & 0x0001] = controllers_[address & 0x0001];
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

