#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

struct my_bus
{
  std::uint8_t* ptr;

  std::uint8_t read(std::uint16_t address) const
  {
    return ptr[address];
  }

  void write(std::uint16_t address, std::uint8_t value)
  {
    ptr[address] = value;
  }
};


enum operation
{
  ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC,
  CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP,
  JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI,
  RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA
};


enum address_mode
{
  accumulator, absolute, absolute_x_indexed, absolute_y_indexed, immediate,
  implied, indexed_indirect, indirect, indirect_indexed, relative,
  zero_page, zero_page_x_indexed, zero_page_y_indexed
};


struct instruction_info
{
  const char* mnemonic;
  operation op;
  address_mode mode;
  int num_bytes;
};


std::array<instruction_info,256> initialize_instruction_info_table()
{
  std::array<instruction_info,256> result = {};

  // XXX think of a way to bake the cycle count into this table
  result[0x01] = {"ORA", ORA, indexed_indirect};
  result[0x05] = {"ORA", ORA, zero_page,      };
  result[0x06] = {"ASL", ASL, zero_page,      };
  result[0x08] = {"PHP", PHP, implied,        };
  result[0x09] = {"ORA", ORA, immediate,      };
  result[0x0D] = {"ORA", ORA, absolute,       };
  result[0x0E] = {"ASL", ASL, absolute,       };
  result[0x10] = {"BPL", BPL, relative,       };
  result[0x0A] = {"ASL", ASL, accumulator,    };
  result[0x18] = {"CLC", CLC, implied,        };
  result[0x20] = {"JSR", JSR, absolute,       };
  result[0x21] = {"AND", AND, indexed_indirect};
  result[0x24] = {"BIT", BIT, zero_page,      };
  result[0x25] = {"AND", AND, zero_page,      };
  result[0x26] = {"ROL", ROL, zero_page,      };
  result[0x28] = {"PLP", PLP, implied,        };
  result[0x29] = {"AND", AND, immediate,      };
  result[0x2A] = {"ROL", ROL, accumulator,    };
  result[0x2C] = {"BIT", BIT, absolute,       };
  result[0x2D] = {"AND", AND, absolute,       };
  result[0x2E] = {"ROL", ROL, absolute,       };
  result[0x30] = {"BMI", BMI, relative,       };
  result[0x38] = {"SEC", SEC, implied,        };
  result[0x40] = {"RTI", RTI, implied,        };
  result[0x41] = {"EOR", EOR, indexed_indirect};
  result[0x45] = {"EOR", EOR, zero_page,      };
  result[0x46] = {"LSR", LSR, zero_page,      };
  result[0x48] = {"PHA", PHA, implied,        };
  result[0x49] = {"EOR", EOR, immediate,      };
  result[0x4A] = {"LSR", LSR, accumulator,    };
  result[0x4C] = {"JMP", JMP, absolute,       };
  result[0x4D] = {"EOR", EOR, absolute,       };
  result[0x4E] = {"LSR", LSR, absolute,       };
  result[0x50] = {"BVC", BVC, relative,       };
  result[0x60] = {"RTS", RTS, implied,        };
  result[0x61] = {"ADC", ADC, indexed_indirect};
  result[0x65] = {"ADC", ADC, zero_page,      };
  result[0x66] = {"ROR", ROR, zero_page,      };
  result[0x68] = {"PLA", PLA, implied,        };
  result[0x69] = {"ADC", ADC, immediate,      };
  result[0x6A] = {"ROR", ROR, accumulator,    };
  result[0x6D] = {"ADC", ADC, absolute,       };
  result[0x6E] = {"ROR", ROR, absolute,       };
  result[0x70] = {"BVS", BVS, relative,       };
  result[0x78] = {"SEI", SEI, implied,        };
  result[0x81] = {"STA", STA, indexed_indirect};
  result[0x84] = {"STY", STY, zero_page,      };
  result[0x85] = {"STA", STA, zero_page,      };
  result[0x86] = {"STX", STX, zero_page,      };
  result[0x88] = {"DEY", DEY, implied,        };
  result[0x8A] = {"TXA", TXA, implied,        };
  result[0x8C] = {"STY", STY, absolute,       };
  result[0x8D] = {"STA", STA, absolute,       };
  result[0x8E] = {"STX", STX, absolute,       };
  result[0x90] = {"BCC", BCC, relative,       };
  result[0x98] = {"TYA", TYA, implied,        };
  result[0x9A] = {"TXS", TXS, implied,        };
  result[0xA0] = {"LDY", LDY, immediate,      };
  result[0xA1] = {"LDA", LDA, indexed_indirect};
  result[0xA2] = {"LDX", LDX, immediate,      };
  result[0xA4] = {"LDY", LDY, zero_page,      };
  result[0xA5] = {"LDA", LDA, zero_page,      };
  result[0xA6] = {"LDX", LDX, zero_page,      };
  result[0xA8] = {"TAY", TAY, implied,        };
  result[0xA9] = {"LDA", LDA, immediate,      };
  result[0xAA] = {"TAX", TAX, implied,        };
  result[0xAC] = {"LDY", LDY, absolute,       };
  result[0xAD] = {"LDA", LDA, absolute,       };
  result[0xAE] = {"LDX", LDX, absolute,       };
  result[0xB0] = {"BCS", BCS, relative,       };
  result[0xB1] = {"LDA", LDA, indirect_indexed};
  result[0xB8] = {"CLV", CLV, implied,        };
  result[0xBA] = {"TSX", TSX, implied,        };
  result[0xC0] = {"CPY", CPY, immediate,      };
  result[0xC1] = {"CMP", CMP, indexed_indirect};
  result[0xC4] = {"CPY", CPY, zero_page,      };
  result[0xC5] = {"CMP", CMP, zero_page,      };
  result[0xC6] = {"DEC", DEC, zero_page,      };
  result[0xC8] = {"INY", INY, implied,        };
  result[0xC9] = {"CMP", CMP, immediate,      };
  result[0xCA] = {"DEX", DEX, implied,        };
  result[0xCC] = {"CPY", CPY, absolute,       };
  result[0xCD] = {"CMP", CMP, absolute,       };
  result[0xCE] = {"DEC", DEC, absolute,       };
  result[0xD0] = {"BNE", BNE, relative,       };
  result[0xD8] = {"CLD", CLD, implied,        };
  result[0xE0] = {"CPX", CPX, immediate,      };
  result[0xE1] = {"SBC", SBC, indexed_indirect};
  result[0xE4] = {"CPX", CPX, zero_page,      };
  result[0xE5] = {"SBC", SBC, zero_page,      };
  result[0xE6] = {"INC", INC, zero_page,      };
  result[0xE8] = {"INX", INX, implied,        };
  result[0xE9] = {"SBC", SBC, immediate,      };
  result[0xEA] = {"NOP", NOP, implied,        };
  result[0xEC] = {"CPX", CPX, absolute,       };
  result[0xED] = {"SBC", SBC, absolute,       };
  result[0xEE] = {"INC", INC, absolute,       };
  result[0xF0] = {"BEQ", BEQ, relative,       };
  result[0xF8] = {"SED", SED, implied,        };

  return result;
}

// XXX make this a static class member
std::array<instruction_info,256> instruction_info_table = initialize_instruction_info_table();


struct my_6502
{
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

    // XXX derive this from the address mode
    int num_bytes() const
    {
      if(instruction_info_table[opcode].mnemonic == 0)
      {
        std::string what;
        what.resize(60);
        snprintf(what.data(), what.size(), "instruction::num_bytes: Unknown opcode %02X", opcode);
        throw std::runtime_error(what);
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
  // this function exists for disassemble's benefit
  std::uint16_t calculate_address(std::uint16_t program_counter, instruction i) const
  {
    std::uint16_t result = 0;

    switch(instruction_info_table[i.opcode].mode)
    {
      case absolute:
      {
        std::uint8_t low_byte = i.byte1;
        std::uint8_t high_byte = i.byte2;

        result = (high_byte << 8) | low_byte;
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
        result = (high_byte_of_result << 8) + low_byte_of_result;
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
        result = program_counter + 1 + i.byte1;
        break;
      }

      case zero_page:
      {
        result = i.byte1;
        break;
      }

      default:
      {
        throw std::runtime_error("calculate_address: Unimplemented address mode");
      }
    }

    return result;
  }

  std::string disassemble(instruction i) const
  {
    std::string arg;
    arg.resize(20);

    auto info = instruction_info_table[i.opcode];

    switch(info.mode)
    {
      case accumulator:
      {
        arg.resize(snprintf(arg.data(), arg.size(), "A"));
        break;
      }

      case absolute:
      {
        arg.resize(snprintf(arg.data(), arg.size(), "$%02X%02X", i.byte2, i.byte1));
        break;
      }

      case immediate:
      {
        arg.resize(snprintf(arg.data(), arg.size(), "#$%02X", i.byte1));
        break;
      }

      case implied:
      {
        break;
      }

      case indexed_indirect:
      {
        arg.resize(snprintf(arg.data(), arg.size(), "($%02X,X)", i.byte1));
        break;
      }

      case indirect_indexed:
      {
        arg.resize(snprintf(arg.data(), arg.size(), "($%02X),Y", i.byte1));
        break;
      }

      case relative:
      {
        // XXX this is weird because the "disassembly" uses the state of the cpu
        std::uint16_t target = program_counter_ + i.num_bytes() + i.byte1;
        arg.resize(snprintf(arg.data(), arg.size(), "$%04X", target));
        break;
      }

      case zero_page:
      {
        arg.resize(snprintf(arg.data(), arg.size(), "$%02X", i.byte1));
        break;
      }

      default:
      {
        throw std::runtime_error("disassemble: Unimplemented address mode");
      }
    }

    // some instructions on memory need to be embellished with the current content of the memory location 
    // XXX me may not need to consider the operation at all, the address mode might be enough
    switch(info.op)
    {
      case ADC:
      case AND:
      case ASL:
      case CMP:
      case CPX:
      case CPY:
      case DEC:
      case EOR:
      case INC:
      case LDA:
      case LDX:
      case LDY:
      case LSR:
      case ORA:
      case ROL:
      case ROR:
      case SBC:
      case BIT:
      case STA:
      case STX:
      case STY:
      {
        // instructions with immediate mode addressing need no embellishment
        if(info.mode == immediate)
        {
          break;
        }

        std::string embellishment;
        embellishment.resize(30);

        if(info.mode == indexed_indirect)
        {
          // XXX this is weird because the disassembly uses the state of the memory
          std::uint8_t x_plus_immediate = index_register_x_ + i.byte1;
          std::uint16_t address = calculate_address(program_counter_, i);
          std::uint8_t data = read(address);
          embellishment.resize(snprintf(embellishment.data(), embellishment.size(), " @ %02X = %04X = %02X", x_plus_immediate, address, data));
        }
        else if(info.mode == indirect_indexed)
        {
          // XXX this is weird because the disassembly uses the state of the memory
          std::uint8_t zero_page_address = i.byte1;
          std::uint8_t low_byte_of_address = read(zero_page_address);
          ++zero_page_address;
          // note that the address of the high byte may wrap around to the beginning of the zero page
          std::uint8_t high_byte_of_address = read(zero_page_address);
          std::uint16_t address_in_table = (high_byte_of_address << 8) + low_byte_of_address;

          std::uint16_t address = calculate_address(program_counter_, i);
          std::uint8_t data = read(address);
          embellishment.resize(snprintf(embellishment.data(), embellishment.size(), " = %04X @ %04X = %02X", address_in_table, address, data));
        }
        else
        {
          std::uint16_t address = calculate_address(program_counter_, i);

          // XXX this is weird because the disassembly uses the state of the memory
          std::uint8_t data = read(address);
          embellishment.resize(snprintf(embellishment.data(), embellishment.size(), " = %02X", data));
        }

        arg += embellishment;
        break;
      }

      default:
      {
        break;
      }
    }

    std::string result;
    if(arg.size())
    {
      result.resize(3 + 1 + arg.size() + 1);
      result.resize(snprintf(result.data(), result.size(), "%s %s", info.mnemonic, arg.c_str()));
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

  int execute_add_with_carry(std::uint16_t address, address_mode mode)
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

    // the cycles consumed depends on the address mode
    int result = 0;

    switch(mode)
    {
      case immediate:
      {
        result = 2;
        break;
      }

      case zero_page:
      {
        result = 3;
        break;
      }

      case absolute:
      case zero_page_x_indexed:
      {
        result = 4;
        break;
      }

      case absolute_x_indexed:
      case absolute_y_indexed:
      {
        result = 4;
        // XXX +1 if paged crossed
        break;
      }

      case indirect_indexed:
      {
        result = 5;
        // XXX +1 if page crossed
        break;
      }

      case indexed_indirect:
      {
        result = 6;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_arithmetic_shift_left(std::uint16_t address, address_mode mode)
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

    int result = 0;

    switch(mode)
    {
      case zero_page:
      {
        result = 5;
        break;
      }

      case zero_page_x_indexed:
      {
        result = 6;
        break;
      }

      case absolute:
      {
        result = 6;
        break;
      }

      case absolute_x_indexed:
      {
        result = 7;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_arithmetic_shift_left_accumulator()
  {  
    // set the carry flag
    carry_flag_ = 0b10000000 & accumulator_;

    // shift
    accumulator_ <<= 1;

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;

    return 2;
  }

  int execute_bit_test(std::uint16_t address, address_mode mode)
  {
    std::uint8_t data = read(address);

    // set the zero flag depending on the value of the data ANDed with the accumulator
    zero_flag_ = ((accumulator_ & data) == 0);

    // set the overflow flag to bit 6 of the data
    overflow_flag_ = 0b01000000 & data;

    // set the negative flag to bit 7 of the data
    negative_flag_ = 0b10000000 & data;

    // the cycles consumed depends on the address mode
    int result = 0;
    switch(mode)
    {
      case zero_page:
      {
        result = 3;
        break;
      }

      case absolute:
      {
        result = 4;
        break;
      }

      default:
      {
      }
    }

    return result;
  }

  int execute_branch(bool condition, std::uint16_t target)
  {
    int result = 2;

    if(condition)
    {
      // the branch succeeds so add a cycle
      result += 1;

      // remember the old program counter
      std::uint16_t old_program_counter = program_counter_;

      // assign the target to the program counter
      program_counter_ = target;

      // add a cycle if we branch to a new page
      if((old_program_counter >> 8) != (program_counter_ >> 8))
      {
        result += 1;
      }
    }

    return result;
  }

  int execute_branch_if_carry_clear(std::uint16_t target)
  {
    return execute_branch(not carry_flag_, target);
  }

  int execute_branch_if_carry_set(std::uint16_t target)
  {
    return execute_branch(carry_flag_, target);
  }

  int execute_branch_if_equal_to_zero(std::uint16_t target)
  {
    return execute_branch(zero_flag_, target);
  }

  int execute_branch_if_minus(std::uint16_t target)
  {
    return execute_branch(negative_flag_, target);
  }

  int execute_branch_if_not_equal_to_zero(std::uint16_t address)
  {
    return execute_branch(not zero_flag_, address);
  }

  int execute_branch_if_overflow_clear(std::uint16_t address)
  {
    return execute_branch(not overflow_flag_, address);
  }

  int execute_branch_if_overflow_set(std::uint16_t address)
  {
    return execute_branch(overflow_flag_, address);
  }

  int execute_branch_if_positive(std::uint16_t address)
  {
    return execute_branch(not negative_flag_, address);
  }

  int execute_clear_flag(bool& flag)
  {
    flag = false;
    return 2;
  }

  int execute_clear_carry_flag()
  {
    return execute_clear_flag(carry_flag_);
  }

  int execute_clear_decimal_mode_flag()
  {
    return execute_clear_flag(decimal_mode_flag_);
  }

  int execute_clear_overflow_flag()
  {
    return execute_clear_flag(overflow_flag_);
  }

  int execute_compare(std::uint8_t reg, std::uint16_t address, address_mode mode)
  {
    std::uint8_t m = read(address);

    // set the carry flag based on the comparison
    carry_flag_ = reg >= m;

    // set the zero flag based on equality
    zero_flag_ = reg == m;

    std::uint8_t difference = reg - m;

    // set the negative flag based on the sign of the difference
    negative_flag_ = 0b10000000 & difference;

    // the cycles consumed depends on the address mode
    int result = 0;
    switch(mode)
    {
      case immediate:
      {
        result = 2;
        break;
      }

      case zero_page:
      {
        result = 3;
        break;
      }

      case absolute:
      case zero_page_x_indexed:
      {
        result = 4;
        break;
      }

      case absolute_x_indexed:
      case absolute_y_indexed:
      {
        result = 4;
        // XXX +1 if page is crossed
        break;
      }

      case indirect_indexed:
      {
        result = 5;
        // XXX +1 if page is crossed
        break;
      }

      case indexed_indirect:
      {
        result = 6;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_compare_accumulator(std::uint16_t address, address_mode mode)
  {
    return execute_compare(accumulator_, address, mode);
  }

  int execute_compare_index_register_x(std::uint16_t address, address_mode mode)
  {
    return execute_compare(index_register_x_, address, mode);
  }

  int execute_compare_index_register_y(std::uint16_t address, address_mode mode)
  {
    return execute_compare(index_register_y_, address, mode);
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

  int execute_decrement_index_register_x()
  {
    execute_decrement(index_register_x_);
    return 2;
  }

  int execute_decrement_index_register_y()
  {
    execute_decrement(index_register_y_);
    return 2;
  }

  int execute_decrement_memory(std::uint16_t address, address_mode mode)
  {
    // read the memory location
    std::uint8_t m = read(address);

    // decrement the value, set flags
    execute_decrement(m);

    // write the result to memory
    write(address, m);

    // the cycles consumed depends on the address mode
    int result = 0;
    switch(mode)
    {
      case zero_page:
      {
        result = 5;
        break;
      }

      case absolute:
      case zero_page_x_indexed:
      {
        result = 6;
        break;
      }

      case absolute_x_indexed:
      {
        result = 7;
        break;
      }

      default:
      {
      }
    }

    return result;
  }

  template<class Op>
  int execute_logical_operation(Op op, std::uint16_t address, address_mode mode)
  {
    std::uint8_t m = read(address);

    // do the operation
    accumulator_ = op(accumulator_, m);

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;

    // the cycles consumed depends on the address mode
    int result = 0;
    switch(mode)
    {
      case immediate:
      {
        result = 2;
        break;
      }

      case zero_page:
      {
        result = 3;
        break;
      }

      case zero_page_x_indexed:
      case absolute:
      {
        result = 4;
        break;
      }

      case absolute_x_indexed:
      case absolute_y_indexed:
      {
        result = 4;
        // XXX +1 if page crossed
        break;
      }

      case indirect_indexed:
      {
        result = 5;
        break;
      }

      case indexed_indirect:
      {
        result = 6;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_exclusive_or(std::uint16_t address, address_mode mode)
  {
    return execute_logical_operation(std::bit_xor{}, address, mode);
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

  int execute_increment_index_register_x()
  {
    execute_increment(index_register_x_);
    return 2;
  }

  int execute_increment_index_register_y()
  {
    execute_increment(index_register_y_);
    return 2;
  }

  int execute_increment_memory(std::uint16_t address, address_mode mode)
  {
    // read the memory location
    std::uint8_t m = read(address);

    // increment the value, set flags
    execute_increment(m);

    // write the result to memory
    write(address, m);

    // the cycles consumed depends on the address mode
    int result = 0;
    switch(mode)
    {
      case zero_page:
      {
        result = 5;
        break;
      }

      case absolute:
      case zero_page_x_indexed:
      {
        result = 6;
        break;
      }

      case absolute_x_indexed:
      {
        result = 7;
        break;
      }

      default:
      {
      }
    }

    return result;
  }

  int execute_jump(std::uint16_t address)
  {
    // set the program counter
    program_counter_ = address;

    return 3;
  }

  int execute_jump_to_subroutine(std::uint16_t address)
  {
    --program_counter_;

    // push the program counter to the stack
    std::uint8_t low_pc_byte = static_cast<std::uint8_t>(program_counter_);
    std::uint8_t high_pc_byte = program_counter_ >> 8;

    push_stack(high_pc_byte);
    push_stack(low_pc_byte);

    // set the program counter
    program_counter_ = address;

    return 6;
  }

  int execute_load(std::uint8_t& reg, std::uint16_t address, address_mode mode)
  {
    std::uint8_t m = read(address);

    // load register with the data
    reg = m;

    // set the zero flag
    zero_flag_ = (reg == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & reg;

    // the cycles consumed depends on the address mode
    int result = 0;
    switch(mode)
    {
      case immediate:
      {
        result = 2;
        break;
      }

      case zero_page:
      {
        result = 3;
        break;
      }

      case zero_page_x_indexed:
      case absolute:
      {
        result = 4;
        break;
      }

      case absolute_x_indexed:
      case absolute_y_indexed:
      {
        result = 4;
        // XXX +1 if page crossed
        break;
      }

      case indirect_indexed:
      {
        result = 5;
        // XXX +1 if page crossed
        break;
      }

      case indexed_indirect:
      {
        result = 6;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_load_accumulator(std::uint16_t address, address_mode mode)
  {
    return execute_load(accumulator_, address, mode);
  }

  int execute_load_index_register_x(std::uint16_t address, address_mode mode)
  {
    return execute_load(index_register_x_, address, mode);
  }

  int execute_load_index_register_y(std::uint16_t address, address_mode mode)
  {
    return execute_load(index_register_y_, address, mode);
  }

  int execute_logical_and(std::uint16_t address, address_mode mode)
  {
    return execute_logical_operation(std::bit_and{}, address, mode);
  }

  int execute_logical_inclusive_or(std::uint16_t address, address_mode mode)
  {
    return execute_logical_operation(std::bit_or{}, address, mode);
  }

  int execute_logical_shift_right(std::uint16_t address, address_mode mode)
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

    int result = 0;

    switch(mode)
    {
      case zero_page:
      {
        result = 5;
        break;
      }

      case zero_page_x_indexed:
      {
        result = 6;
        break;
      }

      case absolute:
      {
        result = 6;
        break;
      }

      case absolute_x_indexed:
      {
        result = 7;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_logical_shift_right_accumulator()
  {  
    // set the carry flag
    carry_flag_ = 0b00000001 & accumulator_;

    // shift
    accumulator_ >>= 1;

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;

    return 2;
  }

  int execute_no_operation()
  {
    // no operation

    return 2;
  }

  int execute_pull_accumulator()
  {
    // pop the stack into the accumulator
    accumulator_ = pop_stack();

    // set the zero flag
    zero_flag_ = (accumulator_ == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & accumulator_;

    return 4;
  }

  int execute_pull_processor_status()
  {
    std::uint8_t data = pop_stack();

    negative_flag_                  = 0b10000000 & data;
    overflow_flag_                  = 0b01000000 & data;
    decimal_mode_flag_              = 0b00001000 & data;
    interrupt_request_disable_flag_ = 0b00000100 & data;
    zero_flag_                      = 0b00000010 & data;
    carry_flag_                     = 0b00000001 & data;

    return 4;
  }

  int execute_push_accumulator()
  {
    // push the value of the accumulator on to the stack
    push_stack(accumulator_);

    return 3;
  }

  int execute_push_processor_status()
  {
    // push the flags on to the stack

    std::uint8_t value = status_flags_as_byte();

    // set bit 4 in the value to push
    value |= 0b00010000;
    
    push_stack(value);

    return 3;
  }

  int execute_return_from_interrupt()
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

    return 6;
  }

  int execute_return_from_subroutine()
  {
    std::uint8_t low_pc_byte = pop_stack();
    std::uint8_t high_pc_byte = pop_stack();

    program_counter_ = (high_pc_byte << 8) + low_pc_byte + 1;

    return 6;
  }

  int execute_rotate_left(std::uint16_t address, address_mode mode)
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

    int result = 0;

    switch(mode)
    {
      case zero_page:
      {
        result = 5;
        break;
      }

      case zero_page_x_indexed:
      {
        result = 6;
        break;
      }

      case absolute:
      {
        result = 6;
        break;
      }

      case absolute_x_indexed:
      {
        result = 7;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_rotate_left_accumulator()
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

    return 2;
  }

  int execute_rotate_right(std::uint16_t address, address_mode mode)
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

    int result = 0;

    switch(mode)
    {
      case zero_page:
      {
        result = 5;
        break;
      }

      case zero_page_x_indexed:
      {
        result = 6;
        break;
      }

      case absolute:
      {
        result = 6;
        break;
      }

      case absolute_x_indexed:
      {
        result = 7;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_rotate_right_accumulator()
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

    return 2;
  }

  int execute_set_flag(bool& flag)
  {
    flag = true;
    return 2;
  }

  int execute_set_carry_flag()
  {
    return execute_set_flag(carry_flag_);
  }

  int execute_set_decimal_mode_flag()
  {
    return execute_set_flag(decimal_mode_flag_);
  }

  int execute_set_interrupt_disable()
  {
    return execute_set_flag(interrupt_request_disable_flag_);
  }

  int execute_store(std::uint8_t reg, std::uint16_t address, address_mode mode)
  {
    if(address == 0x0180)
    {
      std::cout << "execute_store: storing to 0x0180" << std::endl;
    }

    // store the contents of the register to the address
    write(address, reg);

    // the cycles consumed depends on the address mode
    int result = 0;
    switch(mode)
    {
      case zero_page:
      {
        result = 3;
        break;
      }

      case zero_page_x_indexed:
      case absolute:
      {
        result = 4;
        break;
      }

      case absolute_x_indexed:
      case absolute_y_indexed:
      {
        result = 5;
        break;
      }

      case indirect_indexed:
      case indexed_indirect:
      {
        result = 6;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_store_accumulator(std::uint16_t address, address_mode mode)
  {
    return execute_store(accumulator_, address, mode);
  }

  int execute_store_index_register_x(std::uint16_t address, address_mode mode)
  {
    return execute_store(index_register_x_, address, mode);
  }

  int execute_store_index_register_y(std::uint16_t address, address_mode mode)
  {
    return execute_store(index_register_y_, address, mode);
  }

  int execute_subtract_with_carry(std::uint16_t address, address_mode mode)
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

    // the cycles consumed depends on the address mode
    int result = 0;

    switch(mode)
    {
      case immediate:
      {
        result = 2;
        break;
      }

      case zero_page:
      {
        result = 3;
        break;
      }

      case absolute:
      case zero_page_x_indexed:
      {
        result = 4;
        break;
      }

      case absolute_x_indexed:
      case absolute_y_indexed:
      {
        result = 4;
        // XXX +1 if paged crossed
        break;
      }

      case indirect_indexed:
      {
        result = 5;
        // XXX +1 if page crossed
        break;
      }

      case indexed_indirect:
      {
        result = 6;
        break;
      }

      default:
      {
        break;
      }
    }

    return result;
  }

  int execute_transfer(std::uint8_t from_reg, std::uint8_t& to_reg) 
  {
    // store the value in the from reg to the to register
    to_reg = from_reg;

    // set the zero flag
    zero_flag_ = (to_reg == 0);

    // set the negative flag
    negative_flag_ = 0b10000000 & to_reg;

    return 2;
  }

  int execute_transfer_accumulator_to_index_register_x()
  {
    return execute_transfer(accumulator_, index_register_x_);
  }

  int execute_transfer_accumulator_to_index_register_y()
  {
    return execute_transfer(accumulator_, index_register_y_);
  }

  int execute_transfer_index_register_x_to_accumulator()
  {
    return execute_transfer(index_register_x_, accumulator_);
  }

  int execute_transfer_index_register_x_to_stack_pointer()
  {
    stack_pointer_ = index_register_x_;

    // no flags are affected

    return 2;
  }

  int execute_transfer_index_register_y_to_accumulator()
  {
    return execute_transfer(index_register_y_, accumulator_);
  }

  int execute_transfer_stack_pointer_to_index_register_x()
  {
    return execute_transfer(stack_pointer_, index_register_x_);
  }

  // XXX this needs to indicate when a page boundary is crossed
  //     during some of this math
  // XXX break these cases into individual functions
  std::uint16_t calculate_address(address_mode mode)
  {
    std::uint16_t result = 0;

    switch(mode)
    {
      case absolute:
      {
        std::uint8_t low_byte = read(program_counter_);
        ++program_counter_;
        std::uint8_t high_byte = read(program_counter_);
        ++program_counter_;

        result = (high_byte << 8) | low_byte;
        break;
      }

      case accumulator:
      {
        break;
      }

      case immediate:
      {
        result = program_counter_;
        ++program_counter_;
        break;
      }

      case implied:
      {
        break;
      }

      case indexed_indirect:
      {
        // X + the immediate byte wraps around, so the initial address is one byte
        std::uint8_t zero_page_address = index_register_x_ + read(program_counter_);
        ++program_counter_;
        std::uint8_t low_byte_of_result = read(zero_page_address);
        ++zero_page_address;
        // note that the address of the high byte may wrap around to the beginning of the zero page
        std::uint8_t high_byte_of_result = read(zero_page_address);
        result = (high_byte_of_result << 8) + low_byte_of_result;
        break;
      }

      case indirect_indexed:
      {
        std::uint8_t zero_page_address = read(program_counter_);
        ++program_counter_;
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
        std::uint8_t offset = read(program_counter_);
        ++program_counter_;
        result = program_counter_ + offset;
        break;
      }

      case zero_page:
      {
        result = read(program_counter_);
        ++program_counter_;
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
    int result = 0;
    address_mode mode = instruction_info_table[opcode].mode;
    std::uint16_t address = calculate_address(mode);

    switch(instruction_info_table[opcode].op)
    {
      case ADC:
      {
        result = execute_add_with_carry(address, mode);
        break;
      }

      case AND:
      {
        result = execute_logical_and(address, mode);
        break;
      }

      case ASL:
      {
        result = (mode == accumulator) ? execute_arithmetic_shift_left_accumulator() : execute_arithmetic_shift_left(address, mode);
        break;
      }

      case BCC:
      {
        result = execute_branch_if_carry_clear(address);
        break;
      }

      case BCS:
      {
        result = execute_branch_if_carry_set(address);
        break;
      }

      case BEQ:
      {
        result = execute_branch_if_equal_to_zero(address);
        break;
      }

      case BIT:
      {
        result = execute_bit_test(address, mode);
        break;
      }

      case BMI:
      {
        result = execute_branch_if_minus(address);
        break;
      }

      case BNE:
      {
        result = execute_branch_if_not_equal_to_zero(address);
        break;
      }

      case BPL:
      {
        result = execute_branch_if_positive(address);
        break;
      }

      case BVC:
      {
        result = execute_branch_if_overflow_clear(address);
        break;
      }

      case BVS:
      {
        result = execute_branch_if_overflow_set(address);
        break;
      }

      case CLC:
      {
        result = execute_clear_carry_flag();
        break;
      }

      case CLD:
      {
        result = execute_clear_decimal_mode_flag();
        break;
      }

      case CLV:
      {
        result = execute_clear_overflow_flag();
        break;
      }

      case CMP:
      {
        result = execute_compare_accumulator(address, mode);
        break;
      }

      case CPX:
      {
        result = execute_compare_index_register_x(address, mode);
        break;
      }

      case CPY:
      {
        result = execute_compare_index_register_y(address, mode);
        break;
      }

      case DEC:
      {
        result = execute_decrement_memory(address, mode);
        break;
      }

      case DEX:
      {
        result = execute_decrement_index_register_x();
        break;
      }

      case DEY:
      {
        result = execute_decrement_index_register_y();
        break;
      }

      case EOR:
      {
        result = execute_exclusive_or(address, mode);
        break;
      }

      case INC:
      {
        result = execute_increment_memory(address, mode);
        break;
      }

      case INX:
      {
        result = execute_increment_index_register_x();
        break;
      }

      case INY:
      {
        result = execute_increment_index_register_y();
        break;
      }

      case JMP:
      {
        result = execute_jump(address);
        break;
      }

      case JSR:
      {
        result = execute_jump_to_subroutine(address);
        break;
      }

      case LDA:
      {
        result = execute_load_accumulator(address, mode);
        break;
      }

      case LDX:
      {
        result = execute_load_index_register_x(address, mode);
        break;
      }

      case LDY:
      {
        result = execute_load_index_register_y(address, mode);
        break;
      }

      case LSR:
      {
        result = (mode == accumulator) ? execute_logical_shift_right_accumulator() : execute_logical_shift_right(address, mode);
        break;
      }

      case NOP:
      {
        result = execute_no_operation();
        break;
      }

      case ORA:
      {
        result = execute_logical_inclusive_or(address, mode);
        break;
      }

      case PHA:
      {
        result = execute_push_accumulator();
        break;
      }

      case PHP:
      {
        result = execute_push_processor_status();
        break;
      }

      case PLA:
      {
        result = execute_pull_accumulator();
        break;
      }

      case PLP:
      {
        result = execute_pull_processor_status();
        break;
      }

      case ROL:
      {
        result = (mode == accumulator) ? execute_rotate_left_accumulator() : execute_rotate_left(address, mode);
        break;
      }

      case ROR:
      {
        result = (mode == accumulator) ? execute_rotate_right_accumulator() : execute_rotate_right(address, mode);
        break;
      }
      
      case RTI:
      {
        result = execute_return_from_interrupt();
        break;
      }

      case RTS:
      {
        result = execute_return_from_subroutine();
        break;
      }

      case SEC:
      {
        result = execute_set_carry_flag();
        break;
      }

      case SED:
      {
        result = execute_set_decimal_mode_flag();
        break;
      }

      case SEI:
      {
        result = execute_set_interrupt_disable();
        break;
      }

      case SBC:
      {
        result = execute_subtract_with_carry(address, mode);
        break;
      }

      case STA:
      {
        result = execute_store_accumulator(address, mode);
        break;
      }

      case STX:
      {
        result = execute_store_index_register_x(address, mode);
        break;
      }

      case STY:
      {
        result = execute_store_index_register_y(address, mode);
        break;
      }

      case TAX:
      {
        result = execute_transfer_accumulator_to_index_register_x();
        break;
      }

      case TAY:
      {
        result = execute_transfer_accumulator_to_index_register_y();
        break;
      }

      case TSX:
      {
        result = execute_transfer_stack_pointer_to_index_register_x();
        break;
      }

      case TXA:
      {
        result = execute_transfer_index_register_x_to_accumulator();
        break;
      }

      case TXS:
      {
        result = execute_transfer_index_register_x_to_stack_pointer();
        break;
      }

      case TYA:
      {
        result = execute_transfer_index_register_y_to_accumulator();
        break;
      }

      default:
      {
        char what[80];
        snprintf(what, 80, "execute: Unknown opcode %02X", opcode);
        throw std::runtime_error(what);
      }
    }

    return result;
  }

  void log(int cycle) const
  {
    instruction i = read_current_instruction();

    std::array<char,9> instruction_words;
    switch(i.num_bytes())
    {
      case 1:
      {
        snprintf(instruction_words.data(), instruction_words.size(), "%02X", i.opcode);
        break;
      }

      case 2:
      {
        snprintf(instruction_words.data(), instruction_words.size(), "%02X %02X", i.opcode, i.byte1);
        break;
      }

      case 3:
      {
        snprintf(instruction_words.data(), instruction_words.size(), "%02X %02X %02X", i.opcode, i.byte1, i.byte2);
        break;
      }

      default:
      {
        throw std::runtime_error("log: Illegal number of instruction words");
      }
    }

    std::string disassembly = disassemble(i);

    std::string registers;
    registers.resize(30);
    snprintf(registers.data(), registers.size(), "A:%02X X:%02X Y:%02X P:%02X SP:%02X", accumulator_, index_register_x_, index_register_y_, status_flags_as_byte(), stack_pointer_);

    std::string ppu;
    ppu.resize(30);
    snprintf(ppu.data(), ppu.size(), "PPU:%3d,%3d", 0, 0);

    printf("%04X  %-8s  %-31s %s %s CYC:%d\n", program_counter_, instruction_words.data(), disassembly.c_str(), registers.c_str(), ppu.c_str(), cycle);
  }
};


int main()
{
  std::ifstream is{"nestest.nes", std::ios::binary}; 

  std::vector<char> program_text{std::istreambuf_iterator<char>{is}, std::istreambuf_iterator<char>{}};

  // XXX for now, just choose some size large enough to accomodate two copies of the program at the addresses below
  std::vector<std::uint8_t> memory(128 * 1024, 0);

  // load two copies of the program text into memory
  std::copy(program_text.begin() + 0x0010, program_text.end(), memory.begin() + 0x8000);
  std::copy(program_text.begin() + 0x0010, program_text.end(), memory.begin() + 0xC000);

  // initialize the reset vector location
  memory[my_6502::reset_vector_location]     = 0x00;
  memory[my_6502::reset_vector_location + 1] = 0xC0;

  // create a bus
  my_bus bus{memory.data()};

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

