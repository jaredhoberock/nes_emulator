#include "nes/emulate.hpp"
#include "nes/system.hpp"
#include <fmt/format.h>


int main()
{
  // create a system
  nes::system sys{"nestest.nes"};

  sys.bus().write(mos6502::reset_vector_location + 0, 0x00);
  sys.bus().write(mos6502::reset_vector_location + 1, 0xC0);

  try
  {
    emulate(sys);
  }
  catch(std::exception& e)
  {
    std::cerr << "Caught exception: " << e.what() << std::endl;
    std::cerr << std::endl;
  }

  auto zp = sys.bus().zero_page();
  fmt::print("Zero page\n");
  for(std::uint8_t row = 0; row < 16; ++row)
  {
    const std::uint8_t* d = zp.data() + 16 * row;
    fmt::print("${:X}0: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}\n", 
                   row,  d[0],  d[1],  d[2],  d[3],  d[4],  d[5],  d[6],  d[7],  d[8],  d[9],  d[10], d[11], d[12], d[13], d[14], d[15]);
  }

  std::cout << std::endl;

  if(zp[2] == 0)
  {
    fmt::print("Opcode tests passed.\n");
  }
  else
  {
    fmt::print("Opcode tests failed with result: {:02X}\n", zp[2]);
  }

  if(zp[3] == 0)
  {
    fmt::print("Invalid opcode tests passed.\n");
  }
  else
  {
    fmt::print("Invalid opcode tests failed with result: {:02X}\n", zp[3]);
  }

  return 0;
}

