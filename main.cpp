#include "cartridge.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>


struct my_bus
{
  std::array<uint8_t, 0x800> internal_ram;
  cartridge cart;

  std::uint8_t read(std::uint16_t address) const
  {
    std::uint8_t result = 0;

    if(0x0000 <= address and address < 0x2000)
    {
      // this bitwise and implements mirroring
      result = internal_ram[address & 0x07FF];
    }
    else if(0x2000 <= address and address < 0x4000)
    {
      // ppu here, mirrored every 8 bytes
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
      result = cart.read(address);
    }
    else
    {
      throw std::runtime_error("my_bus::read: Bad address");
    }

    return result;
  }

  void write(std::uint16_t address, std::uint8_t value)
  {
    if(0x0000 <= address and address < 0x2000)
    {
      // this bitwise and implements mirroring
      internal_ram[address & 0x07FF] = value;
    }
    else if(0x2000 <= address and address < 0x4000)
    {
      // ppu here, mirrored every 8 bytes
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
      cart.write(address, value);
    }
    else
    {
      throw std::runtime_error("my_bus::write: Bad address");
    }
  }
};


enum operation
{
  // legal instructions
  ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC,
  CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP,
  JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI,
  RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA,

  // "illegal" instructions 
  DCP, Illegal_NOP, Illegal_SBC, ISC, LAX, RLA, RRA, SAX, SLO, SRE
};


enum address_mode
{
  accumulator, absolute, absolute_x_indexed, absolute_y_indexed, immediate,
  implied, indexed_indirect, indirect, indirect_indexed, relative,
  zero_page, zero_page_x_indexed, zero_page_y_indexed
};


int calculate_extra_cycles(std::uint8_t opcode, bool page_boundary_crossed, bool branch_taken)
{
  int result = 0;

  switch(opcode)
  {
    case 0x11:
    case 0x1C:
    case 0x1D:
    case 0x19:
    case 0x31:
    case 0x39:
    case 0x3C:
    case 0x3D:
    case 0x51:
    case 0x5C:
    case 0x59:
    case 0x5D:
    case 0x71:
    case 0x7C:
    case 0x79:
    case 0x7D:
    case 0xB1:
    case 0xB3:
    case 0xB9:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
    case 0xD1:
    case 0xDC:
    case 0xD9:
    case 0xDD:
    case 0xF1:
    case 0xF9:
    case 0xFC:
    case 0xFD:
    {
      result = page_boundary_crossed;
      break;
    }

    case 0x10:
    case 0x30:
    case 0x50:
    case 0x70:
    case 0x90:
    case 0xB0:
    case 0xD0:
    case 0xF0:
    {
      result = branch_taken;
      if(branch_taken)
      {
        result += page_boundary_crossed;
      }

      break;
    }
  }

  return result;
}


struct instruction_info
{
  // the mnemonic used is whatever matches nestest.log
  // and may differ from the enumeration for illegal instructions
  const char* mnemonic;
  operation op;
  address_mode mode;
  int num_cycles;
};


std::array<instruction_info,256> initialize_instruction_info_table()
{
  std::array<instruction_info,256> result = {};

  result[0x00] = {"BRK", BRK,         implied,             7};
  result[0x01] = {"ORA", ORA,         indexed_indirect,    6};
  result[0x03] = {"SLO", SLO,         indexed_indirect,    8};
  result[0x04] = {"NOP", Illegal_NOP, zero_page,           3};
  result[0x05] = {"ORA", ORA,         zero_page,           3};
  result[0x06] = {"ASL", ASL,         zero_page,           5};
  result[0x07] = {"SLO", SLO,         zero_page,           5};
  result[0x08] = {"PHP", PHP,         implied,             3};
  result[0x09] = {"ORA", ORA,         immediate,           2};
  result[0x0A] = {"ASL", ASL,         accumulator,         2};
  result[0x0C] = {"NOP", Illegal_NOP, absolute,            4};
  result[0x0D] = {"ORA", ORA,         absolute,            4};
  result[0x0E] = {"ASL", ASL,         absolute,            6};
  result[0x0F] = {"SLO", SLO,         absolute,            6};
  result[0x10] = {"BPL", BPL,         relative,            2};
  result[0x11] = {"ORA", ORA,         indirect_indexed,    5};
  result[0x13] = {"SLO", SLO,         indirect_indexed,    8};
  result[0x14] = {"NOP", Illegal_NOP, zero_page_x_indexed, 4};
  result[0x15] = {"ORA", ORA,         zero_page_x_indexed, 4};
  result[0x16] = {"ASL", ASL,         zero_page_x_indexed, 6};
  result[0x17] = {"SLO", SLO,         zero_page_x_indexed, 6};
  result[0x18] = {"CLC", CLC,         implied,             2};
  result[0x19] = {"ORA", ORA,         absolute_y_indexed,  4};
  result[0x1A] = {"NOP", Illegal_NOP, implied,             2};
  result[0x1B] = {"SLO", SLO,         absolute_y_indexed,  7};
  result[0x1C] = {"NOP", Illegal_NOP, absolute_x_indexed,  4};
  result[0x1D] = {"ORA", ORA,         absolute_x_indexed,  4};
  result[0x1E] = {"ASL", ASL,         absolute_x_indexed,  7};
  result[0x1F] = {"SLO", SLO,         absolute_x_indexed,  7};
  result[0x20] = {"JSR", JSR,         absolute,            6};
  result[0x21] = {"AND", AND,         indexed_indirect,    6};
  result[0x23] = {"RLA", RLA,         indexed_indirect,    8};
  result[0x24] = {"BIT", BIT,         zero_page,           3};
  result[0x25] = {"AND", AND,         zero_page,           3};
  result[0x26] = {"ROL", ROL,         zero_page,           5};
  result[0x27] = {"RLA", RLA,         zero_page,           5};
  result[0x28] = {"PLP", PLP,         implied,             4};
  result[0x29] = {"AND", AND,         immediate,           2};
  result[0x2A] = {"ROL", ROL,         accumulator,         2};
  result[0x2C] = {"BIT", BIT,         absolute,            4};
  result[0x2D] = {"AND", AND,         absolute,            4};
  result[0x2E] = {"ROL", ROL,         absolute,            6};
  result[0x2F] = {"RLA", RLA,         absolute,            6};
  result[0x30] = {"BMI", BMI,         relative,            2};
  result[0x31] = {"AND", AND,         indirect_indexed,    5};
  result[0x33] = {"RLA", RLA,         indirect_indexed,    8};
  result[0x34] = {"NOP", Illegal_NOP, zero_page_x_indexed, 4};
  result[0x35] = {"AND", AND,         zero_page_x_indexed, 4};
  result[0x36] = {"ROL", ROL,         zero_page_x_indexed, 6};
  result[0x37] = {"RLA", RLA,         zero_page_x_indexed, 6};
  result[0x38] = {"SEC", SEC,         implied,             2};
  result[0x39] = {"AND", AND,         absolute_y_indexed,  4};
  result[0x3A] = {"NOP", Illegal_NOP, implied,             2};
  result[0x3B] = {"RLA", RLA,         absolute_y_indexed,  7};
  result[0x3C] = {"NOP", Illegal_NOP, absolute_x_indexed,  4};
  result[0x3D] = {"AND", AND,         absolute_x_indexed,  4};
  result[0x3E] = {"ROL", ROL,         absolute_x_indexed,  7};
  result[0x3F] = {"RLA", RLA,         absolute_x_indexed,  7};
  result[0x40] = {"RTI", RTI,         implied,             6};
  result[0x41] = {"EOR", EOR,         indexed_indirect,    6};
  result[0x43] = {"SRE", SRE,         indexed_indirect,    8};
  result[0x44] = {"NOP", Illegal_NOP, zero_page,           3};
  result[0x45] = {"EOR", EOR,         zero_page,           3};
  result[0x46] = {"LSR", LSR,         zero_page,           5};
  result[0x47] = {"SRE", SRE,         zero_page,           5};
  result[0x48] = {"PHA", PHA,         implied,             3};
  result[0x49] = {"EOR", EOR,         immediate,           2};
  result[0x4A] = {"LSR", LSR,         accumulator,         2};
  result[0x4C] = {"JMP", JMP,         absolute,            3};
  result[0x4D] = {"EOR", EOR,         absolute,            4};
  result[0x4E] = {"LSR", LSR,         absolute,            6};
  result[0x4F] = {"SRE", SRE,         absolute,            6};
  result[0x50] = {"BVC", BVC,         relative,            2};
  result[0x51] = {"EOR", EOR,         indirect_indexed,    5};
  result[0x53] = {"SRE", SRE,         indirect_indexed,    8};
  result[0x54] = {"NOP", Illegal_NOP, zero_page_x_indexed, 4};
  result[0x55] = {"EOR", EOR,         zero_page_x_indexed, 4};
  result[0x56] = {"LSR", LSR,         zero_page_x_indexed, 6};
  result[0x57] = {"SRE", SRE,         zero_page_x_indexed, 6};
  result[0x59] = {"EOR", EOR,         absolute_y_indexed,  4};
  result[0x5A] = {"NOP", Illegal_NOP, implied,             2};
  result[0x5B] = {"SRE", SRE,         absolute_y_indexed,  7};
  result[0x5C] = {"NOP", Illegal_NOP, absolute_x_indexed,  4};
  result[0x5D] = {"EOR", EOR,         absolute_x_indexed,  4};
  result[0x5E] = {"LSR", LSR,         absolute_x_indexed,  7};
  result[0x5F] = {"SRE", SRE,         absolute_x_indexed,  7};
  result[0x60] = {"RTS", RTS,         implied,             6};
  result[0x61] = {"ADC", ADC,         indexed_indirect,    6};
  result[0x63] = {"RRA", RRA,         indexed_indirect,    8};
  result[0x64] = {"NOP", Illegal_NOP, zero_page,           3};
  result[0x65] = {"ADC", ADC,         zero_page,           3};
  result[0x66] = {"ROR", ROR,         zero_page,           5};
  result[0x67] = {"RRA", RRA,         zero_page,           5};
  result[0x68] = {"PLA", PLA,         implied,             4};
  result[0x69] = {"ADC", ADC,         immediate,           2};
  result[0x6A] = {"ROR", ROR,         accumulator,         2};
  result[0x6C] = {"JMP", JMP,         indirect,            5};
  result[0x6D] = {"ADC", ADC,         absolute,            4};
  result[0x6E] = {"ROR", ROR,         absolute,            6};
  result[0x6F] = {"RRA", RRA,         absolute,            6};
  result[0x70] = {"BVS", BVS,         relative,            2};
  result[0x71] = {"ADC", ADC,         indirect_indexed,    5};
  result[0x73] = {"RRA", RRA,         indirect_indexed,    8};
  result[0x74] = {"NOP", Illegal_NOP, zero_page_x_indexed, 4};
  result[0x75] = {"ADC", ADC,         zero_page_x_indexed, 4};
  result[0x76] = {"ROR", ROR,         zero_page_x_indexed, 6};
  result[0x77] = {"RRA", RRA,         zero_page_x_indexed, 6};
  result[0x78] = {"SEI", SEI,         implied,             2};
  result[0x79] = {"ADC", ADC,         absolute_y_indexed,  4};
  result[0x7A] = {"NOP", Illegal_NOP, implied,             2};
  result[0x7B] = {"RRA", RRA,         absolute_y_indexed,  7};
  result[0x7C] = {"NOP", Illegal_NOP, absolute_x_indexed,  4};
  result[0x7D] = {"ADC", ADC,         absolute_x_indexed,  4};
  result[0x7E] = {"ROR", ROR,         absolute_x_indexed,  7};
  result[0x7F] = {"RRA", RRA,         absolute_x_indexed,  7};
  result[0x80] = {"NOP", Illegal_NOP, immediate,           2};
  result[0x81] = {"STA", STA,         indexed_indirect,    6};
  result[0x83] = {"SAX", SAX,         indexed_indirect,    6};
  result[0x84] = {"STY", STY,         zero_page,           3};
  result[0x85] = {"STA", STA,         zero_page,           3};
  result[0x86] = {"STX", STX,         zero_page,           3};
  result[0x87] = {"SAX", SAX,         zero_page,           3};
  result[0x88] = {"DEY", DEY,         implied,             2};
  result[0x8A] = {"TXA", TXA,         implied,             2};
  result[0x8C] = {"STY", STY,         absolute,            4};
  result[0x8D] = {"STA", STA,         absolute,            4};
  result[0x8E] = {"STX", STX,         absolute,            4};
  result[0x8F] = {"SAX", SAX,         absolute,            4};
  result[0x90] = {"BCC", BCC,         relative,            2};
  result[0x91] = {"STA", STA,         indirect_indexed,    6};
  result[0x94] = {"STY", STY,         zero_page_x_indexed, 4};
  result[0x95] = {"STA", STA,         zero_page_x_indexed, 4};
  result[0x96] = {"STX", STX,         zero_page_y_indexed, 4};
  result[0x97] = {"SAX", SAX,         zero_page_y_indexed, 4};
  result[0x98] = {"TYA", TYA,         implied,             2};
  result[0x99] = {"STA", STA,         absolute_y_indexed,  5};
  result[0x9A] = {"TXS", TXS,         implied,             2};
  result[0x9D] = {"STA", STA,         absolute_x_indexed,  5};
  result[0xA0] = {"LDY", LDY,         immediate,           2};
  result[0xA1] = {"LDA", LDA,         indexed_indirect,    6};
  result[0xA2] = {"LDX", LDX,         immediate,           2};
  result[0xA3] = {"LAX", LAX,         indexed_indirect,    6};
  result[0xA4] = {"LDY", LDY,         zero_page,           3};
  result[0xA5] = {"LDA", LDA,         zero_page,           3};
  result[0xA6] = {"LDX", LDX,         zero_page,           3};
  result[0xA7] = {"LAX", LAX,         zero_page,           3};
  result[0xA8] = {"TAY", TAY,         implied,             2};
  result[0xA9] = {"LDA", LDA,         immediate,           2};
  result[0xAA] = {"TAX", TAX,         implied,             2};
  result[0xAC] = {"LDY", LDY,         absolute,            4};
  result[0xAD] = {"LDA", LDA,         absolute,            4};
  result[0xAE] = {"LDX", LDX,         absolute,            4};
  result[0xAF] = {"LAX", LAX,         absolute,            4};
  result[0xB0] = {"BCS", BCS,         relative,            2};
  result[0xB1] = {"LDA", LDA,         indirect_indexed,    5};
  result[0xB3] = {"LAX", LAX,         indirect_indexed,    5};
  result[0xB4] = {"LDY", LDY,         zero_page_x_indexed, 4};
  result[0xB5] = {"LDA", LDA,         zero_page_x_indexed, 4};
  result[0xB6] = {"LDX", LDX,         zero_page_y_indexed, 4};
  result[0xB7] = {"LAX", LAX,         zero_page_y_indexed, 4};
  result[0xB8] = {"CLV", CLV,         implied,             2};
  result[0xB9] = {"LDA", LDA,         absolute_y_indexed,  4};
  result[0xBA] = {"TSX", TSX,         implied,             2};
  result[0xBC] = {"LDY", LDY,         absolute_x_indexed,  4};
  result[0xBD] = {"LDA", LDA,         absolute_x_indexed,  4};
  result[0xBE] = {"LDX", LDX,         absolute_y_indexed,  4};
  result[0xBF] = {"LAX", LAX,         absolute_y_indexed,  4};
  result[0xC0] = {"CPY", CPY,         immediate,           2};
  result[0xC1] = {"CMP", CMP,         indexed_indirect,    6};
  result[0xC3] = {"DCP", DCP,         indexed_indirect,    8};
  result[0xC4] = {"CPY", CPY,         zero_page,           3};
  result[0xC5] = {"CMP", CMP,         zero_page,           3};
  result[0xC6] = {"DEC", DEC,         zero_page,           5};
  result[0xC7] = {"DCP", DCP,         zero_page,           5};
  result[0xC8] = {"INY", INY,         implied,             2};
  result[0xC9] = {"CMP", CMP,         immediate,           2};
  result[0xCA] = {"DEX", DEX,         implied,             2};
  result[0xCC] = {"CPY", CPY,         absolute,            4};
  result[0xCD] = {"CMP", CMP,         absolute,            4};
  result[0xCE] = {"DEC", DEC,         absolute,            6};
  result[0xCF] = {"DCP", DCP,         absolute,            6};
  result[0xD0] = {"BNE", BNE,         relative,            2};
  result[0xD1] = {"CMP", CMP,         indirect_indexed,    5};
  result[0xD3] = {"DCP", DCP,         indirect_indexed,    8};
  result[0xD4] = {"NOP", Illegal_NOP, zero_page_x_indexed, 4};
  result[0xD5] = {"CMP", CMP,         zero_page_x_indexed, 4};
  result[0xD6] = {"DEC", DEC,         zero_page_x_indexed, 6};
  result[0xD7] = {"DCP", DCP,         zero_page_x_indexed, 6};
  result[0xD8] = {"CLD", CLD,         implied,             2};
  result[0xD9] = {"CMP", CMP,         absolute_y_indexed,  4};
  result[0xDA] = {"NOP", Illegal_NOP, implied,             2};
  result[0xDB] = {"DCP", DCP,         absolute_y_indexed,  7};
  result[0xDC] = {"NOP", Illegal_NOP, absolute_x_indexed,  4};
  result[0xDD] = {"CMP", CMP,         absolute_x_indexed,  4};
  result[0xDE] = {"DEC", DEC,         absolute_x_indexed,  7};
  result[0xDF] = {"DCP", DCP,         absolute_x_indexed,  7};
  result[0xE0] = {"CPX", CPX,         immediate,           2};
  result[0xE1] = {"SBC", SBC,         indexed_indirect,    6};
  result[0xE3] = {"ISB", ISC,         indexed_indirect,    8};
  result[0xE4] = {"CPX", CPX,         zero_page,           3};
  result[0xE5] = {"SBC", SBC,         zero_page,           3};
  result[0xE6] = {"INC", INC,         zero_page,           5};
  result[0xE7] = {"ISB", ISC,         zero_page,           5};
  result[0xE8] = {"INX", INX,         implied,             2};
  result[0xE9] = {"SBC", SBC,         immediate,           2};
  result[0xEA] = {"NOP", NOP,         implied,             2};
  result[0xEB] = {"SBC", Illegal_SBC, immediate,           2};
  result[0xEC] = {"CPX", CPX,         absolute,            4};
  result[0xED] = {"SBC", SBC,         absolute,            4};
  result[0xEE] = {"INC", INC,         absolute,            6};
  result[0xEF] = {"ISB", ISC,         absolute,            6};
  result[0xF0] = {"BEQ", BEQ,         relative,            2};
  result[0xF1] = {"SBC", SBC,         indirect_indexed,    5};
  result[0xF3] = {"ISB", ISC,         indirect_indexed,    8};
  result[0xF4] = {"NOP", Illegal_NOP, zero_page_x_indexed, 4};
  result[0xF5] = {"SBC", SBC,         zero_page_x_indexed, 4};
  result[0xF6] = {"INC", INC,         zero_page_x_indexed, 6};
  result[0xF7] = {"ISB", ISC,         zero_page_x_indexed, 6};
  result[0xF8] = {"SED", SED,         implied,             2};
  result[0xF9] = {"SBC", SBC,         absolute_y_indexed,  4};
  result[0xFA] = {"NOP", Illegal_NOP, implied,             2};
  result[0xFB] = {"ISB", ISC,         absolute_y_indexed,  7};
  result[0xFC] = {"NOP", Illegal_NOP, absolute_x_indexed,  4};
  result[0xFD] = {"SBC", SBC,         absolute_x_indexed,  4};
  result[0xFE] = {"INC", INC,         absolute_x_indexed,  7};
  result[0xFF] = {"ISB", ISC,         absolute_x_indexed,  7};

  return result;
}

// XXX make this a static class member
std::array<instruction_info,256> instruction_info_table = initialize_instruction_info_table();


bool is_legal(std::uint8_t opcode)
{
  return instruction_info_table[opcode].op < DCP;
}




struct my_6502
{
  static constexpr std::uint16_t interrupt_request_vector_location = 0xFFFE;
  static constexpr std::uint16_t reset_vector_location = 0xFFFC;
  static constexpr std::uint8_t  initial_stack_pointer_value = 0xFD;
  static constexpr std::uint8_t  initial_accumulator_value = 0x00;
  static constexpr std::uint8_t  initial_index_register_x_value = 0x00;
  static constexpr std::uint8_t  initial_index_register_y_value = 0x00;

  struct instruction
  {
    std::uint8_t opcode;
    std::uint8_t byte1;
    std::uint8_t byte2;

    int num_bytes() const
    {
      if(instruction_info_table[opcode].mnemonic == 0)
      {
        throw std::runtime_error(fmt::format("instruction::num_bytes: Unknown opcode {:02X}", opcode));
      }

      int result = 0;
      switch(instruction_info_table[opcode].mode)
      {
        case accumulator:
        case implied:
        {
          result = 1;
          break;
        }

        case immediate:
        case indexed_indirect:
        case indirect_indexed:
        case relative:
        case zero_page:
        case zero_page_x_indexed:
        case zero_page_y_indexed:
        {
          result = 2;
          break;
        }

        case absolute:
        case absolute_x_indexed:
        case absolute_y_indexed:
        case indirect:
        {
          result = 3;
          break;
        }
      }

      return result;
    }
  };

  // state
  std::uint16_t program_counter_;
  std::uint8_t  stack_pointer_;
  std::uint8_t  accumulator_;
  std::uint8_t  index_register_x_;
  std::uint8_t  index_register_y_;

  bool negative_flag_;
  bool overflow_flag_;
  bool decimal_mode_flag_;
  bool interrupt_request_disable_flag_;
  bool zero_flag_;
  bool carry_flag_;

  my_bus& bus_;

  my_6502(my_bus& bus)
    : program_counter_{}, stack_pointer_{}, accumulator_{}, index_register_x_{}, index_register_y_{},
      negative_flag_{}, overflow_flag_{}, decimal_mode_flag_{}, interrupt_request_disable_flag_{}, zero_flag_{}, carry_flag_{},
      bus_{bus}
  {}

  std::uint8_t read(std::uint16_t address) const
  {
    return bus_.read(address);
  }

  void write(std::uint16_t address, std::uint8_t value) const
  {
    bus_.write(address, value);
  }

  instruction read_current_instruction() const
  {
    instruction result{};

    result.opcode = read(program_counter_);

    if(result.num_bytes() > 1)
    {
      result.byte1 = read(program_counter_ + 1);
    }

    if(result.num_bytes() > 2)
    {
      result.byte2 = read(program_counter_ + 2);
    }

    return result;
  }

  // program_counter is the pc of instruction i
  // this function exists for nestest_log's benefit
  std::uint16_t calculate_address(std::uint16_t program_counter, instruction i) const
  {
    std::uint16_t result = 0;

    switch(instruction_info_table[i.opcode].mode)
    {
      case absolute:
      {
        std::uint8_t low_byte_of_result = i.byte1;
        std::uint8_t high_byte_of_result = i.byte2;

        result = (high_byte_of_result << 8) | low_byte_of_result;
        break;
      }

      case absolute_x_indexed:
      {
        std::uint8_t low_byte_of_result = i.byte1;
        std::uint8_t high_byte_of_result = i.byte2;

        result = (high_byte_of_result << 8) | low_byte_of_result;
        result += index_register_x_;

        break;
      }

      case absolute_y_indexed:
      {
        std::uint8_t low_byte_of_result = i.byte1;
        std::uint8_t high_byte_of_result = i.byte2;

        result = (high_byte_of_result << 8) | low_byte_of_result;
        result += index_register_y_;

        break;
      }

      case accumulator:
      {
        break;
      }

      case immediate:
      {
        result = program_counter + 1;
        break;
      }

      case implied:
      {
        break;
      }

      case indexed_indirect:
      {
        // X + the immediate byte wraps around, so the initial address is one byte
        std::uint8_t zero_page_address = index_register_x_ + i.byte1;
        std::uint8_t low_byte_of_result = read(zero_page_address);
        ++zero_page_address;
        // note that the address of the high byte may wrap around to the beginning of the zero page
        std::uint8_t high_byte_of_result = read(zero_page_address);
        result = (high_byte_of_result << 8) | low_byte_of_result;
        break;
      }

      case indirect:
      {
        std::uint8_t low_byte_of_ptr = i.byte1;
        std::uint8_t high_byte_of_ptr = i.byte2;

        std::uint8_t low_byte_of_result = read((high_byte_of_ptr << 8) | low_byte_of_ptr);

        // the 6502 has a bug that prevents this addition from crossing a page boundary
        // IOW, adding 1 + 0x##FF wraps around to 0x##00
        // we simulate this by simply incrementing the low byte of the pointer
        ++low_byte_of_ptr;

        std::uint8_t high_byte_of_result = read((high_byte_of_ptr << 8) | low_byte_of_ptr);

        result = (high_byte_of_result << 8) | low_byte_of_result;
        break;
      }

      case indirect_indexed:
      {
        std::uint8_t zero_page_address = i.byte1;
        std::uint8_t low_byte_of_result = read(zero_page_address);
        ++zero_page_address;
        // note that the address of the high byte may wrap around to the beginning of the zero page
        std::uint8_t high_byte_of_result = read(zero_page_address);
        result = (high_byte_of_result << 8) + low_byte_of_result;
        // add in the value stored in Y
        result += index_register_y_;
        break;
      }

      case relative:
      {
        //result = program_counter + 1 + i.byte1;
        // the offset is interpreted as signed data
        std::int8_t offset = *reinterpret_cast<std::int8_t*>(&i.byte1);
        result = program_counter + 1 + offset;
        break;
      }

      case zero_page:
      {
        result = i.byte1;
        break;
      }

      case zero_page_x_indexed:
      {
        result = i.byte1 + index_register_x_;
        break;
      }

      case zero_page_y_indexed:
      {
        result = i.byte1 + index_register_y_;
        break;
      }

      default:
      {
        throw std::runtime_error("calculate_address: Unimplemented address mode");
      }
    }

    return result;
  }

  std::string nestest_instruction_log(instruction i) const
  {
    std::string arg;

    auto info = instruction_info_table[i.opcode];
    switch(info.mode)
    {
      case accumulator:
      {
        arg = fmt::format("A");
        break;
      }

      case absolute:
      {
        arg = fmt::format("${:02X}{:02X}", i.byte2, i.byte1);
        break;
      }

      case absolute_x_indexed:
      {
        arg = fmt::format("${:02X}{:02X},X", i.byte2, i.byte1);
        break;
      }

      case absolute_y_indexed:
      {
        arg = fmt::format("${:02X}{:02X},Y", i.byte2, i.byte1);
        break;
      }

      case immediate:
      {
        arg = fmt::format("#${:02X}", i.byte1);
        break;
      }

      case implied:
      {
        break;
      }

      case indexed_indirect:
      {
        arg = fmt::format("(${:02X},X)", i.byte1);
        break;
      }

      case indirect:
      {
        arg = fmt::format("(${:02X}{:02X})", i.byte2, i.byte1);
        break;
      }

      case indirect_indexed:
      {
        arg = fmt::format("(${:02X}),Y", i.byte1);
        break;
      }

      case relative:
      {
        std::uint16_t address = calculate_address(program_counter_ + 1, i);
        arg = fmt::format("${:04X}", address);
        break;
      }

      case zero_page:
      {
        arg = fmt::format("${:02X}", i.byte1);
        break;
      }

      case zero_page_x_indexed:
      {
        arg = fmt::format("${:02X},X", i.byte1);
        break;
      }

      case zero_page_y_indexed:
      {
        arg = fmt::format("${:02X},Y", i.byte1);
        break;
      }

      default:
      {
        throw std::runtime_error("nestest_instruction_log: Unimplemented address mode");
      }
    }

    // some instructions on memory need to be embellished with the current content of the memory location 
    std::string embellishment;

    switch(info.op)
    {
      case ADC:
      case AND:
      case ASL:
      case BIT:
      case CMP:
      case CPX:
      case CPY:
      case DCP:
      case DEC:
      case EOR:
      case Illegal_NOP:
      case Illegal_SBC:
      case INC:
      case ISC:
      case JMP:
      case LAX:
      case LDA:
      case LDX:
      case LDY:
      case LSR:
      case ORA:
      case ROL:
      case ROR:
      case RLA:
      case RRA:
      case SAX:
      case SBC:
      case SLO:
      case SRE:
      case STA:
      case STX:
      case STY:
      {
        // instructions with certain kinds of address mode need no embellishment
        if(info.mode == immediate or info.mode == accumulator or info.mode == relative)
        {
          break;
        }

        switch(info.mode)
        {
          case absolute_x_indexed:
          case absolute_y_indexed:
          {
            std::uint16_t address = calculate_address(program_counter_, i);
            std::uint8_t data = read(address);
            embellishment = fmt::format(" @ {:04X} = {:02X}", address, data);
            break;
          }

          case indexed_indirect:
          {
            std::uint8_t x_plus_immediate = index_register_x_ + i.byte1;
            std::uint16_t address = calculate_address(program_counter_, i);
            std::uint8_t data = read(address);
            embellishment = fmt::format(" @ {:02X} = {:04X} = {:02X}", x_plus_immediate, address, data);
            break;
          }

          case indirect:
          {
            std::uint16_t address = calculate_address(program_counter_, i);
            embellishment = fmt::format(" = {:04X}", address);
            break;
          }

          case indirect_indexed:
          {
            std::uint8_t zero_page_address = i.byte1;
            std::uint8_t low_byte_of_address = read(zero_page_address);
            ++zero_page_address;
            // note that the address of the high byte may wrap around to the beginning of the zero page
            std::uint8_t high_byte_of_address = read(zero_page_address);
            std::uint16_t address_in_table = (high_byte_of_address << 8) + low_byte_of_address;

            std::uint16_t address = calculate_address(program_counter_, i);
            std::uint8_t data = read(address);
            embellishment = fmt::format(" = {:04X} @ {:04X} = {:02X}", address_in_table, address, data);
            break;
          }

          case zero_page_x_indexed:
          case zero_page_y_indexed:
          {
            std::uint8_t address = calculate_address(program_counter_, i);
            std::uint8_t data = read(address);
            embellishment = fmt::format(" @ {:02X} = {:02X}", address, data);
            break;
          }

          default:
          {
            if(info.op == JMP)
            {
              // JMP needs no embellishment in absolute address mode
              break;
            }

            std::uint16_t address = calculate_address(program_counter_, i);

            std::uint8_t data = read(address);
            embellishment = fmt::format(" = {:02X}", data);
            break;
          }
        }

        break;
      }

      default:
      {
        break;
      }
    }

    arg += embellishment;

    std::string result;
    if(not arg.empty())
    {
      result = fmt::format("{} {}", info.mnemonic, arg);
    }
    else
    {
      result = info.mnemonic;
    }

    return result;
  }

  // sets cpu to initial conditions and returns the number of cycles consumed
  int reset()
  {
    // initialize the program counter
    program_counter_ = read(reset_vector_location) | (read(reset_vector_location + 1) << 8);

    // initialize the stack pointer
    stack_pointer_ = initial_stack_pointer_value;

    // initialize registers
    accumulator_ = initial_accumulator_value;
    index_register_x_ = initial_index_register_x_value;
    index_register_y_ = initial_index_register_y_value;

    // initialize status flags
    negative_flag_ = false;
    overflow_flag_ = false;
    decimal_mode_flag_ = false;
    interrupt_request_disable_flag_ = true;
    zero_flag_ = false;
    carry_flag_ = false;

    return 7;
  }

  std::uint8_t status_flags_as_byte() const
  {
    // note that the unused "constant" flag (bit 5) is hardwired to on
    // note that the break flag (bit 4) is not actually set by an instruction
    return (negative_flag_ << 7) |
           (overflow_flag_ << 6) |
           (1 << 5) |
           (0 << 4) |
           (decimal_mode_flag_ << 3) |
           (interrupt_request_disable_flag_ << 2) |
           (zero_flag_ << 1) |
           (carry_flag_ << 0);
  }

  std::uint8_t pop_stack()
  {
    ++stack_pointer_;

    return read(0x0100 + stack_pointer_);
  }

  void push_stack(std::uint8_t value)
  {
    write(0x0100 + stack_pointer_, value);

    --stack_pointer_;
  }

  void execute_add_with_carry(std::uint16_t address)
  {
    std::uint8_t m = read(address);

    std::uint16_t sum = accumulator_ + m + carry_flag_;

    // set the overflow flag depending on if the signs differ
    overflow_flag_ = !((accumulator_ ^ m) & 0x80) && ((accumulator_ ^ sum) & 0x80);

    // set the carry flag depending on whether there was a carry bit
    carry_flag_ = sum > 0x00FF;

    // assign the sum to the accumulator
    accumulator_ = static_cast<std::uint8_t>(sum);

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_arithmetic_shift_left(std::uint16_t address)
  {  
    std::uint8_t m = read(address);

    // set the carry flag
    carry_flag_ = 0b10000000 & m;

    // shift
    m <<= 1;

    // set the zero flag
    zero_flag_ = (m == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & m;

    // store
    write(address, m);
  }

  void execute_arithmetic_shift_left_accumulator()
  {  
    // set the carry flag
    carry_flag_ = 0b10000000 & accumulator_;

    // shift
    accumulator_ <<= 1;

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_bit_test(std::uint16_t address)
  {
    std::uint8_t data = read(address);

    // set the zero flag depending on the value of the data ANDed with the accumulator
    zero_flag_ = ((accumulator_ & data) == 0);

    // set the overflow flag to bit 6 of the data
    overflow_flag_ = 0b01000000 & data;

    // set the negative flag to bit 7 of the data
    negative_flag_ = 0b10000000 & data;
  }

  // returns whether or not the branch was taken
  bool execute_branch(bool condition, std::uint16_t target)
  {
    if(condition)
    {
      // assign the target to the program counter
      program_counter_ = target;
    }

    return condition;
  }

  bool execute_branch_if_carry_clear(std::uint16_t target)
  {
    return execute_branch(not carry_flag_, target);
  }

  bool execute_branch_if_carry_set(std::uint16_t target)
  {
    return execute_branch(carry_flag_, target);
  }

  bool execute_branch_if_equal_to_zero(std::uint16_t target)
  {
    return execute_branch(zero_flag_, target);
  }

  bool execute_branch_if_minus(std::uint16_t target)
  {
    return execute_branch(negative_flag_, target);
  }

  bool execute_branch_if_not_equal_to_zero(std::uint16_t address)
  {
    return execute_branch(not zero_flag_, address);
  }

  bool execute_branch_if_overflow_clear(std::uint16_t address)
  {
    return execute_branch(not overflow_flag_, address);
  }

  bool execute_branch_if_overflow_set(std::uint16_t address)
  {
    return execute_branch(overflow_flag_, address);
  }

  bool execute_branch_if_positive(std::uint16_t address)
  {
    return execute_branch(not negative_flag_, address);
  }

  void execute_break()
  {
    --program_counter_;

    // push the program counter to the stack
    std::uint8_t low_pc_byte = static_cast<std::uint8_t>(program_counter_);
    std::uint8_t high_pc_byte = program_counter_ >> 8;

    push_stack(high_pc_byte);
    push_stack(low_pc_byte);

    // push the processor status to the stack
    execute_push_processor_status();

    // set the program counter to the interrupt request vector
    program_counter_ = read(interrupt_request_vector_location) | (read(interrupt_request_vector_location + 1) << 8);
  }

  void execute_clear_flag(bool& flag)
  {
    flag = false;
  }

  void execute_clear_carry_flag()
  {
    execute_clear_flag(carry_flag_);
  }

  void execute_clear_decimal_mode_flag()
  {
    execute_clear_flag(decimal_mode_flag_);
  }

  void execute_clear_overflow_flag()
  {
    execute_clear_flag(overflow_flag_);
  }

  void execute_compare(std::uint8_t reg, std::uint16_t address)
  {
    std::uint8_t m = read(address);

    // set the carry flag based on the comparison
    carry_flag_ = reg >= m;

    // set the zero flag based on equality
    zero_flag_ = reg == m;

    std::uint8_t difference = reg - m;

    // set the negative flag based on the sign of the difference
    negative_flag_ = 0b10000000 & difference;
  }

  void execute_compare_accumulator(std::uint16_t address)
  {
    execute_compare(accumulator_, address);
  }

  void execute_compare_index_register_x(std::uint16_t address)
  {
    execute_compare(index_register_x_, address);
  }

  void execute_compare_index_register_y(std::uint16_t address)
  {
    execute_compare(index_register_y_, address);
  }

  void execute_decrement(std::uint8_t& target)
  {
    // decrement the target
    target -= 1;

    // set the zero flag
    zero_flag_ = (target == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & target;
  }

  void execute_decrement_index_register_x()
  {
    execute_decrement(index_register_x_);
  }

  void execute_decrement_index_register_y()
  {
    execute_decrement(index_register_y_);
  }

  void execute_decrement_memory(std::uint16_t address)
  {
    // read the memory location
    std::uint8_t m = read(address);

    // decrement the value, set flags
    execute_decrement(m);

    // write the result to memory
    write(address, m);
  }

  template<class Op>
  void execute_logical_operation(Op op, std::uint16_t address)
  {
    std::uint8_t m = read(address);

    // do the operation
    accumulator_ = op(accumulator_, m);

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_exclusive_or(std::uint16_t address)
  {
    execute_logical_operation(std::bit_xor{}, address);
  }

  void execute_increment(std::uint8_t& target)
  {
    // increment the target
    target += 1;

    // set the zero flag
    zero_flag_ = (target == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & target;
  }

  void execute_increment_index_register_x()
  {
    execute_increment(index_register_x_);
  }

  void execute_increment_index_register_y()
  {
    execute_increment(index_register_y_);
  }

  void execute_increment_memory(std::uint16_t address)
  {
    // read the memory location
    std::uint8_t m = read(address);

    // increment the value, set flags
    execute_increment(m);

    // write the result to memory
    write(address, m);
  }

  void execute_jump(std::uint16_t address)
  {
    // set the program counter
    program_counter_ = address;
  }

  void execute_jump_to_subroutine(std::uint16_t address)
  {
    --program_counter_;

    // push the program counter to the stack
    std::uint8_t low_pc_byte = static_cast<std::uint8_t>(program_counter_);
    std::uint8_t high_pc_byte = program_counter_ >> 8;

    push_stack(high_pc_byte);
    push_stack(low_pc_byte);

    // set the program counter
    program_counter_ = address;
  }

  void execute_load(std::uint8_t& reg, std::uint16_t address)
  {
    std::uint8_t m = read(address);

    // load register with the data
    reg = m;

    // set the zero flag
    zero_flag_ = (reg == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & reg;
  }

  void execute_load_accumulator(std::uint16_t address)
  {
    execute_load(accumulator_, address);
  }

  void execute_load_index_register_x(std::uint16_t address)
  {
    execute_load(index_register_x_, address);
  }

  void execute_load_index_register_y(std::uint16_t address)
  {
    execute_load(index_register_y_, address);
  }

  void execute_logical_and(std::uint16_t address)
  {
    execute_logical_operation(std::bit_and{}, address);
  }

  void execute_logical_inclusive_or(std::uint16_t address)
  {
    execute_logical_operation(std::bit_or{}, address);
  }

  void execute_logical_shift_right(std::uint16_t address)
  {  
    std::uint8_t m = read(address);

    // set the carry flag
    carry_flag_ = 0b00000001 & m;

    // shift
    m >>= 1;

    // set the zero flag
    zero_flag_ = (m == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & m;

    // store
    write(address, m);
  }

  void execute_logical_shift_right_accumulator()
  {  
    // set the carry flag
    carry_flag_ = 0b00000001 & accumulator_;

    // shift
    accumulator_ >>= 1;

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_no_operation()
  {
    // no operation
  }

  void execute_pull_accumulator()
  {
    // pop the stack into the accumulator
    accumulator_ = pop_stack();

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_pull_processor_status()
  {
    std::uint8_t data = pop_stack();

    negative_flag_                  = 0b10000000 & data;
    overflow_flag_                  = 0b01000000 & data;
    decimal_mode_flag_              = 0b00001000 & data;
    interrupt_request_disable_flag_ = 0b00000100 & data;
    zero_flag_                      = 0b00000010 & data;
    carry_flag_                     = 0b00000001 & data;
  }

  void execute_push_accumulator()
  {
    // push the value of the accumulator on to the stack
    push_stack(accumulator_);
  }

  void execute_push_processor_status()
  {
    // push the flags on to the stack

    std::uint8_t value = status_flags_as_byte();

    // set bit 4 in the value to push
    value |= 0b00010000;
    
    push_stack(value);
  }

  void execute_return_from_interrupt()
  {
    std::uint8_t flags = pop_stack();

    negative_flag_                  = 0b10000000 & flags;
    overflow_flag_                  = 0b01000000 & flags;
    decimal_mode_flag_              = 0b00001000 & flags;
    interrupt_request_disable_flag_ = 0b00000100 & flags;
    zero_flag_                      = 0b00000010 & flags;
    carry_flag_                     = 0b00000001 & flags;

    std::uint8_t low_pc_byte = pop_stack();
    std::uint8_t high_pc_byte = pop_stack();

    program_counter_ = (high_pc_byte << 8) + low_pc_byte;
  }

  void execute_return_from_subroutine()
  {
    std::uint8_t low_pc_byte = pop_stack();
    std::uint8_t high_pc_byte = pop_stack();

    program_counter_ = (high_pc_byte << 8) + low_pc_byte + 1;
  }

  void execute_rotate_left(std::uint16_t address)
  {  
    std::uint8_t m = read(address);

    // remember the old carry flag
    bool old_carry_flag = carry_flag_;

    // set the carry flag
    carry_flag_ = 0b10000000 & m;

    // shift
    m <<= 1;

    // set bit 0 to the old carry flag
    if(old_carry_flag)
    {
      m |= 0b00000001;
    }
    else
    {
      m &= 0b11111110;
    }

    // set the zero flag
    zero_flag_ = (m == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & m;

    // store
    write(address, m);
  }

  void execute_rotate_left_accumulator()
  {  
    // remember the old carry flag
    bool old_carry_flag = carry_flag_;

    // set the carry flag
    carry_flag_ = 0b10000000 & accumulator_;

    // shift
    accumulator_ <<= 1;

    // set bit 0 to the old carry flag
    if(old_carry_flag)
    {
      accumulator_ |= 0b00000001;
    }
    else
    {
      accumulator_ &= 0b11111110;
    }

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_rotate_right(std::uint16_t address)
  {  
    std::uint8_t m = read(address);

    // remember the old carry flag
    bool old_carry_flag = carry_flag_;

    // set the carry flag
    carry_flag_ = 0b00000001 & m;

    // shift
    m >>= 1;

    // set bit 7 to the old carry flag
    if(old_carry_flag)
    {
      m |= 0b10000000;
    }
    else
    {
      m &= 0b01111111;
    }

    // set the zero flag
    zero_flag_ = (m == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & m;

    // store
    write(address, m);
  }

  void execute_rotate_right_accumulator()
  {  
    // remember the old carry flag
    bool old_carry_flag = carry_flag_;

    // set the carry flag
    carry_flag_ = 0b00000001 & accumulator_;

    // shift
    accumulator_ >>= 1;

    // set bit 7 to the old carry flag
    if(old_carry_flag)
    {
      accumulator_ |= 0b10000000;
    }
    else
    {
      accumulator_ &= 0b01111111;
    }

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_set_flag(bool& flag)
  {
    flag = true;
  }

  void execute_set_carry_flag()
  {
    execute_set_flag(carry_flag_);
  }

  void execute_set_decimal_mode_flag()
  {
    execute_set_flag(decimal_mode_flag_);
  }

  void execute_set_interrupt_disable()
  {
    execute_set_flag(interrupt_request_disable_flag_);
  }

  void execute_store(std::uint8_t reg, std::uint16_t address)
  {
    // store the contents of the register to the address
    write(address, reg);
  }

  void execute_store_accumulator(std::uint16_t address)
  {
    execute_store(accumulator_, address);
  }

  void execute_store_logical_and_of_accumulator_and_index_register_x(std::uint16_t address)
  {
    execute_store(accumulator_ & index_register_x_, address);
  }

  void execute_store_index_register_x(std::uint16_t address)
  {
    execute_store(index_register_x_, address);
  }

  void execute_store_index_register_y(std::uint16_t address)
  {
    execute_store(index_register_y_, address);
  }

  void execute_subtract_with_carry(std::uint16_t address)
  {
    std::uint8_t m = read(address);

    std::uint16_t difference = accumulator_ - m - (1 - carry_flag_);

    // set the overflow flag depending on if the signs differ
    // note that this condition is the negation of execute_add_with_carry
    overflow_flag_ = ((accumulator_ ^ m) & 0x80) && ((accumulator_ ^ difference) & 0x80);

    // set the carry flag depending on whether there was a carry bit
    // note that this condition is the negation of execute_add_with_carry
    carry_flag_ = !(difference > 0x00FF);

    // assign the difference to the accumulator
    accumulator_ = static_cast<std::uint8_t>(difference);

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;
  }

  void execute_transfer(std::uint8_t from_reg, std::uint8_t& to_reg) 
  {
    // store the value in the from reg to the to register
    to_reg = from_reg;

    // set the zero flag
    zero_flag_ = (to_reg == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & to_reg;
  }

  void execute_transfer_accumulator_to_index_register_x()
  {
    execute_transfer(accumulator_, index_register_x_);
  }

  void execute_transfer_accumulator_to_index_register_y()
  {
    execute_transfer(accumulator_, index_register_y_);
  }

  void execute_transfer_index_register_x_to_accumulator()
  {
    execute_transfer(index_register_x_, accumulator_);
  }

  void execute_transfer_index_register_x_to_stack_pointer()
  {
    stack_pointer_ = index_register_x_;

    // no flags are affected
  }

  void execute_transfer_index_register_y_to_accumulator()
  {
    execute_transfer(index_register_y_, accumulator_);
  }

  void execute_transfer_stack_pointer_to_index_register_x()
  {
    execute_transfer(stack_pointer_, index_register_x_);
  }

  std::uint16_t calculate_absolute_address()
  {
    std::uint8_t low_byte = read(program_counter_);
    ++program_counter_;
    std::uint8_t high_byte = read(program_counter_);
    ++program_counter_;

    return (high_byte << 8) | low_byte;
  }

  // returns a pair containing the calculated address and whether or not a page boundary
  // was crossed while calculating that address
  std::pair<std::uint16_t,bool> calculate_absolute_indexed_address(std::uint8_t index_register)
  {
    std::uint8_t low_byte = read(program_counter_);
    ++program_counter_;
    std::uint8_t high_byte = read(program_counter_);
    ++program_counter_;

    std::uint16_t result = (high_byte << 8) | low_byte;
    result += index_register;

    // check for crossing page boundary
    bool page_boundary_crossed = (result >> 8) != high_byte;

    return {result, page_boundary_crossed};
  }

  // returns a pair containing the calculated address and whether or not a page boundary
  // was crossed while calculating that address
  std::pair<std::uint16_t,bool> calculate_absolute_x_indexed_address()
  {
    return calculate_absolute_indexed_address(index_register_x_);
  }

  // returns a pair containing the calculated address and whether or not a page boundary
  // was crossed while calculating that address
  std::pair<std::uint16_t,bool> calculate_absolute_y_indexed_address()
  {
    return calculate_absolute_indexed_address(index_register_y_);
  }

  std::uint16_t calculate_immediate_address()
  {
    std::uint16_t result = program_counter_;
    ++program_counter_;
    return result;
  }

  std::uint16_t calculate_indexed_indirect_address()
  {
    // X + the immediate byte wraps around, so the initial address is one byte
    std::uint8_t zero_page_address = index_register_x_ + read(program_counter_);
    ++program_counter_;
    std::uint8_t low_byte_of_result = read(zero_page_address);
    ++zero_page_address;
    // note that the address of the high byte may wrap around to the beginning of the zero page
    std::uint8_t high_byte_of_result = read(zero_page_address);
    return (high_byte_of_result << 8) + low_byte_of_result;
  }

  std::uint16_t calculate_indirect_address()
  {
    std::uint8_t low_byte_of_ptr = read(program_counter_);
    ++program_counter_;
    std::uint8_t high_byte_of_ptr = read(program_counter_);
    ++program_counter_;

    std::uint8_t low_byte_of_result = read((high_byte_of_ptr << 8) | low_byte_of_ptr);

    // the 6502 has a bug that prevents this addition from crossing a page boundary
    // IOW, adding 1 + 0x##FF wraps around to 0x##00
    // we simulate this by simply incrementing the low byte of the pointer
    ++low_byte_of_ptr;

    std::uint8_t high_byte_of_result = read((high_byte_of_ptr << 8) | low_byte_of_ptr);

    return (high_byte_of_result << 8) | low_byte_of_result;
  }

  // returns a pair containing the calculated address and whether or not a page boundary
  // was crossed while calculating that address
  std::pair<std::uint16_t,bool> calculate_indirect_indexed_address()
  {
    std::uint8_t zero_page_address = read(program_counter_);
    ++program_counter_;
    std::uint8_t low_byte_of_result = read(zero_page_address);
    ++zero_page_address;

    // note that the address of the high byte may wrap around to the beginning of the zero page
    std::uint8_t high_byte_of_result = read(zero_page_address);

    std::uint16_t result = (high_byte_of_result << 8) + low_byte_of_result;

    // add in the value stored in Y
    result += index_register_y_;

    // check for crossing page boundary
    bool page_boundary_crossed = (result >> 8) != high_byte_of_result;

    return {result, page_boundary_crossed};
  }

  // returns a pair containing the calculated address and whether or not a page boundary
  // was crossed while calculating that address
  std::pair<std::uint16_t,bool> calculate_relative_address()
  {
    std::uint8_t data = read(program_counter_);
    // the offset is interpreted as signed data
    std::int8_t offset = *reinterpret_cast<std::int8_t*>(&data);
    ++program_counter_;

    std::uint8_t high_byte = program_counter_ >> 8;
    std::uint16_t result = program_counter_ + offset;

    // check for crossing page boundary
    bool page_boundary_crossed = (result >> 8) != high_byte;

    return {result, page_boundary_crossed};
  }

  std::uint16_t calculate_zero_page_address()
  {
    std::uint16_t result = read(program_counter_);
    ++program_counter_;
    return result;
  }

  std::uint16_t calculate_zero_page_x_indexed_address()
  {
    std::uint8_t result = read(program_counter_);
    ++program_counter_;

    // note that this addition may wrap around to the beginning of the zero page
    result += index_register_x_;
    return result;
  }

  std::uint16_t calculate_zero_page_y_indexed_address()
  {
    std::uint8_t result = read(program_counter_);
    ++program_counter_;

    // note that this addition may wrap around to the beginning of the zero page
    result += index_register_y_;
    return result;
  }

  // this function returns a pair containing the calculated address argument
  // for the current instruction and whether or not the current instruction
  // will consume an additional cycle
  std::pair<uint16_t,bool> calculate_address(address_mode mode)
  {
    std::pair<uint16_t, bool> result{0, false};

    switch(mode)
    {
      case absolute:
      {
        result.first = calculate_absolute_address();
        break;
      }

      case absolute_x_indexed:
      {
        result = calculate_absolute_x_indexed_address();
        break;
      }

      case absolute_y_indexed:
      {
        result = calculate_absolute_y_indexed_address();
        break;
      }

      case accumulator:
      {
        break;
      }

      case immediate:
      {
        result.first = calculate_immediate_address();
        break;
      }

      case implied:
      {
        break;
      }

      case indexed_indirect:
      {
        result.first = calculate_indexed_indirect_address();
        break;
      }

      case indirect:
      {
        result.first = calculate_indirect_address();
        break;
      }

      case indirect_indexed:
      {
        result = calculate_indirect_indexed_address();
        break;
      }

      case relative:
      {
        result = calculate_relative_address();
        break;
      }

      case zero_page:
      {
        result.first = calculate_zero_page_address();
        break;
      }

      case zero_page_x_indexed:
      {
        result.first = calculate_zero_page_x_indexed_address();
        break;
      }

      case zero_page_y_indexed:
      {
        result.first = calculate_zero_page_y_indexed_address();
        break;
      }

      default:
      {
        throw std::runtime_error("calculate_address: Unimplemented address mode");
      }
    }

    return result;
  }

  int execute(std::uint8_t opcode)
  {
    address_mode mode = instruction_info_table[opcode].mode;
    auto [address, page_boundary_crossed] = calculate_address(mode);
    bool branch_taken = false;

    switch(instruction_info_table[opcode].op)
    {
      case ADC:
      {
        execute_add_with_carry(address);
        break;
      }

      case AND:
      {
        execute_logical_and(address);
        break;
      }

      case ASL:
      {
        if(mode == accumulator)
        {
          execute_arithmetic_shift_left_accumulator();
        }
        else
        {
          execute_arithmetic_shift_left(address);
        }

        break;
      }

      case BCC:
      {
        branch_taken = execute_branch_if_carry_clear(address);
        break;
      }

      case BCS:
      {
        branch_taken = execute_branch_if_carry_set(address);
        break;
      }

      case BEQ:
      {
        branch_taken = execute_branch_if_equal_to_zero(address);
        break;
      }

      case BIT:
      {
        execute_bit_test(address);
        break;
      }

      case BMI:
      {
        branch_taken = execute_branch_if_minus(address);
        break;
      }

      case BNE:
      {
        branch_taken = execute_branch_if_not_equal_to_zero(address);
        break;
      }

      case BPL:
      {
        branch_taken = execute_branch_if_positive(address);
        break;
      }

      //case BRK:
      //{
      //  execute_break();
      //  break;
      //}

      case BVC:
      {
        branch_taken = execute_branch_if_overflow_clear(address);
        break;
      }

      case BVS:
      {
        branch_taken = execute_branch_if_overflow_set(address);
        break;
      }

      case CLC:
      {
        execute_clear_carry_flag();
        break;
      }

      case CLD:
      {
        execute_clear_decimal_mode_flag();
        break;
      }

      case CLV:
      {
        execute_clear_overflow_flag();
        break;
      }

      case CMP:
      {
        execute_compare_accumulator(address);
        break;
      }

      case CPX:
      {
        execute_compare_index_register_x(address);
        break;
      }

      case CPY:
      {
        execute_compare_index_register_y(address);
        break;
      }

      case DCP:
      {
        execute_decrement_memory(address);
        execute_compare_accumulator(address);
        break;
      }

      case DEC:
      {
        execute_decrement_memory(address);
        break;
      }

      case DEX:
      {
        execute_decrement_index_register_x();
        break;
      }

      case DEY:
      {
        execute_decrement_index_register_y();
        break;
      }

      case EOR:
      {
        execute_exclusive_or(address);
        break;
      }

      case Illegal_NOP:
      {
        execute_no_operation();
        break;
      }

      case Illegal_SBC:
      {
        execute_subtract_with_carry(address);
        break;
      }

      case INC:
      {
        execute_increment_memory(address);
        break;
      }

      case INX:
      {
        execute_increment_index_register_x();
        break;
      }

      case INY:
      {
        execute_increment_index_register_y();
        break;
      }

      case ISC:
      {
        execute_increment_memory(address);
        execute_subtract_with_carry(address);
        break;
      }

      case JMP:
      {
        execute_jump(address);
        break;
      }

      case JSR:
      {
        execute_jump_to_subroutine(address);
        break;
      }

      case LAX:
      {
        execute_load_accumulator(address);
        execute_transfer_accumulator_to_index_register_x();
        break;
      }

      case LDA:
      {
        execute_load_accumulator(address);
        break;
      }

      case LDX:
      {
        execute_load_index_register_x(address);
        break;
      }

      case LDY:
      {
        execute_load_index_register_y(address);
        break;
      }

      case LSR:
      {
        if(mode == accumulator)
        {
          execute_logical_shift_right_accumulator();
        }
        else
        {
          execute_logical_shift_right(address);
        }

        break;
      }

      case NOP:
      {
        execute_no_operation();
        break;
      }

      case ORA:
      {
        execute_logical_inclusive_or(address);
        break;
      }

      case PHA:
      {
        execute_push_accumulator();
        break;
      }

      case PHP:
      {
        execute_push_processor_status();
        break;
      }

      case PLA:
      {
        execute_pull_accumulator();
        break;
      }

      case PLP:
      {
        execute_pull_processor_status();
        break;
      }

      case RLA:
      {
        execute_rotate_left(address);
        execute_logical_and(address);
        break;
      }

      case ROL:
      {
        if(mode == accumulator)
        {
          execute_rotate_left_accumulator();
        }
        else
        {
          execute_rotate_left(address);
        }

        break;
      }

      case ROR:
      {
        if(mode == accumulator)
        {
          execute_rotate_right_accumulator();
        }
        else
        {
          execute_rotate_right(address);
        }

        break;
      }

      case RRA:
      {
        execute_rotate_right(address);
        execute_add_with_carry(address);
        break;
      }
      
      case RTI:
      {
        execute_return_from_interrupt();
        break;
      }

      case RTS:
      {
        execute_return_from_subroutine();
        break;
      }

      case SAX:
      {
        execute_store_logical_and_of_accumulator_and_index_register_x(address);
        break;
      }

      case SEC:
      {
        execute_set_carry_flag();
        break;
      }

      case SED:
      {
        execute_set_decimal_mode_flag();
        break;
      }

      case SEI:
      {
        execute_set_interrupt_disable();
        break;
      }

      case SBC:
      {
        execute_subtract_with_carry(address);
        break;
      }

      case SLO:
      {
        execute_arithmetic_shift_left(address);
        execute_logical_inclusive_or(address);
        break;
      }

      case SRE:
      {
        execute_logical_shift_right(address);
        execute_exclusive_or(address);
        break;
      }

      case STA:
      {
        execute_store_accumulator(address);
        break;
      }

      case STX:
      {
        execute_store_index_register_x(address);
        break;
      }

      case STY:
      {
        execute_store_index_register_y(address);
        break;
      }

      case TAX:
      {
        execute_transfer_accumulator_to_index_register_x();
        break;
      }

      case TAY:
      {
        execute_transfer_accumulator_to_index_register_y();
        break;
      }

      case TSX:
      {
        execute_transfer_stack_pointer_to_index_register_x();
        break;
      }

      case TXA:
      {
        execute_transfer_index_register_x_to_accumulator();
        break;
      }

      case TXS:
      {
        execute_transfer_index_register_x_to_stack_pointer();
        break;
      }

      case TYA:
      {
        execute_transfer_index_register_y_to_accumulator();
        break;
      }

      default:
      {
        throw std::runtime_error(fmt::format("execute: Unknown opcode {:02X}", opcode));
      }
    }

    return instruction_info_table[opcode].num_cycles + calculate_extra_cycles(opcode, page_boundary_crossed, branch_taken);
  }

  void log(int cycle) const
  {
    instruction i = read_current_instruction();

    std::string instruction_words;
    switch(i.num_bytes())
    {
      case 1:
      {
        instruction_words = fmt::format("{:02X}", i.opcode);
        break;
      }

      case 2:
      {
        instruction_words = fmt::format("{:02X} {:02X}", i.opcode, i.byte1);
        break;
      }

      case 3:
      {
        instruction_words = fmt::format("{:02X} {:02X} {:02X}", i.opcode, i.byte1, i.byte2);
        break;
      }

      default:
      {
        throw std::runtime_error("log: Illegal number of instruction words");
      }
    }

    std::string instruction_log = nestest_instruction_log(i);
    std::string registers = fmt::format("A:{:02X} X:{:02X} Y:{:02X} P:{:02X} SP:{:02X}", accumulator_, index_register_x_, index_register_y_, status_flags_as_byte(), stack_pointer_);
    std::string ppu = fmt::format("PPU:{:3d},{:3d}", 0, 0);

    if(is_legal(i.opcode))
    {
      fmt::print(std::cout, "{:04X}  {:<8}  {:<31} {} {} CYC:{}\n", program_counter_, instruction_words, instruction_log, registers, ppu, cycle);
    }
    else
    {
      // denote illegal operations with a *
      fmt::print(std::cout, "{:04X}  {:<8} *{:<31} {} {} CYC:{}\n", program_counter_, instruction_words, instruction_log, registers, ppu, cycle);
    }
  }
};


int main()
{
  // create a bus
  my_bus bus{{}, cartridge{"nestest.nes"}};

  // initialize the reset vector location for headless nestest
  bus.write(my_6502::reset_vector_location, 0x00);
  bus.write(my_6502::reset_vector_location + 1, 0xC0);

  // create a cpu
  my_6502 cpu{bus};

  // reset the cpu
  int cycle = cpu.reset();

  try
  {
    while(true)
    {
      // log current state
      cpu.log(cycle);
  
      // read opcode
      std::uint8_t opcode = cpu.read(cpu.program_counter_);
      cpu.program_counter_++;

      // execute instruction
      cycle += cpu.execute(opcode);
    }
  }
  catch(std::exception& e)
  {
    std::cerr << "Caught exception: " << e.what() << std::endl;
  }

  return 0;
}

