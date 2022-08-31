#pragma once

#include "apu.hpp"
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
    apu& apu_;
    std::uint8_t dma_page_;
    std::uint8_t dma_address_;
    std::uint8_t dma_data_;
    bool dma_in_progress_;
    bool dma_can_begin_;

  public:
    bus(std::span<const std::uint8_t,2> controllers,
        cartridge& cart,
        std::span<std::uint8_t,2048> wram,
        ppu& p,
        apu& a)
      : controllers_{controllers},
        cart_{cart},
        wram_{wram},
        ppu_{p},
        apu_{a},
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
      else if(address == 0x4015)
      {
        bool dmc_interrupt = false;
        bool frame_interrupt = apu_.frame_interrupt_flag();
        bool dmc_active = false;
        bool noise_length_counter_status = false;
        bool triangle_length_counter_status = apu_.triangle_length_counter_status();
        bool pulse_1_length_counter_status = apu_.pulse_1_length_counter_status();
        bool pulse_0_length_counter_status = apu_.pulse_0_length_counter_status();

        result = 0;
        result |= (dmc_interrupt << 7);
        result |= (frame_interrupt << 6);
        result |= (dmc_active << 4);
        result |= (noise_length_counter_status << 3);
        result |= (triangle_length_counter_status << 2);
        result |= (pulse_1_length_counter_status << 1);
        result |= (pulse_0_length_counter_status << 0);
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
      else if(0x4000 == address)
      {
        std::uint8_t duty_cycle = value >> 6;
        bool loop_volume = (0b00100000 & value) != 0;
        bool constant_volume = (0b00010000 & value) != 0;
        std::uint8_t volume_period = 0b00001111 & value;

        apu_.set_pulse_0_duty_cycle_and_volume_envelope(duty_cycle, loop_volume, constant_volume, volume_period);
      }
      else if(0x4001 == address)
      {
        bool enabled             = (0b10000000 & value) != 0;
        std::uint8_t period      = (0b01110000 & value) >> 4;
        bool negated             = (0b00001000 & value) != 0;
        std::uint8_t shift_count =  0b00000111 & value;

        apu_.set_pulse_0_sweep(enabled, period, negated, shift_count);
      }
      else if(0x4002 == address)
      {
        apu_.set_pulse_0_timer_low_bits(value);
      }
      else if(0x4003 == address)
      {
        std::uint8_t index = value >> 3;
        std::uint8_t timer_bits = (0b00000111 & value);

        apu_.set_pulse_0_length_counter_and_timer_high_bits(index, timer_bits);
      }
      else if(0x4004 == address)
      {
        std::uint8_t duty_cycle = value >> 6;
        bool loop_volume = (0b00100000 & value) != 0;
        bool constant_volume = (0b00010000 & value) != 0;
        std::uint8_t volume_period = 0b00001111 & value;

        apu_.set_pulse_1_duty_cycle_and_volume_envelope(duty_cycle, loop_volume, constant_volume, volume_period);
      }
      else if(0x4005 == address)
      {
        bool enabled             = (0b10000000 & value) != 0;
        std::uint8_t period      = (0b01110000 & value) >> 4;
        bool negated             = (0b00001000 & value) != 0;
        std::uint8_t shift_count =  0b00000111 & value;

        apu_.set_pulse_1_sweep(enabled, period, negated, shift_count);
      }
      else if(0x4006 == address)
      {
        apu_.set_pulse_1_timer_low_bits(value);
      }
      else if(0x4007 == address)
      {
        std::uint8_t index = value >> 3;
        std::uint8_t timer_bits = (0b00000111 & value);

        apu_.set_pulse_1_length_counter_and_timer_high_bits(index, timer_bits);
      }
      else if(0x4008 == address)
      {
        bool control        = (0b10000000 & value) != 0;
        std::uint8_t period = (0b01111111 & value);

        apu_.set_triangle_linear_counter(control, period);
      }
      else if(0x400A == address)
      {
        apu_.set_triangle_timer_low_bits(value);
      }
      else if(0x400B == address)
      {
        std::uint8_t index = value >> 3;
        std::uint8_t timer_bits = (0b00000111 & value);

        apu_.set_triangle_length_counter_and_timer_high_bits(index, timer_bits);
      }
      else if(0x400C == address)
      {
        bool halt_length_counter   = (0b00100000 & value) != 0;
        bool constant_volume       = (0b00010000 & value) != 0;
        std::uint8_t volume_period = (0b00001111 & value);

        apu_.set_noise_length_counter_halt_and_volume_envelope(halt_length_counter, constant_volume, volume_period);
      }
      else if(0x400E == address)
      {
        bool mode          = (0b10000000 & value) != 0;
        std::uint8_t index = (0b00001111 & value);

        apu_.set_noise_mode_and_timer_period(mode, index);
      }
      else if(0x400F == address)
      {
        std::uint8_t index = value >> 3;

        apu_.set_noise_length_counter(index);
      }
      else if(0x400F < address and address < 0x4014)
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
        bool dmc_enabled      = (0b00010000 & value) != 0;
        bool noise_enabled    = (0b00001000 & value) != 0;
        bool triangle_enabled = (0b00000100 & value) != 0;
        bool pulse_1_enabled  = (0b00000010 & value) != 0;
        bool pulse_0_enabled  = (0b00000001 & value) != 0;

        apu_.enable_channels(dmc_enabled, noise_enabled, triangle_enabled, pulse_1_enabled, pulse_0_enabled);
      }
      else if(0x4016 == address)
      {
        // writing to this address captures the current controller 0 state into the shift registers
        // XXX OLC's code mapped address 0x4017 to this as well for some reason
        //     we may need to more carefully emulate the controller ports
        controller_shift_registers_[0] = controllers_[0];
      }
      else if(0x4017 == address)
      {
        // see https://www.nesdev.org/wiki/APU#Frame_Counter_($4017)
        bool mode               = (value & 0b10000000) != 0;
        bool inhibit_interrupts = (value & 0b01000000) != 0;

        apu_.set_frame_counter_mode_and_interrupts(mode, inhibit_interrupts);
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

