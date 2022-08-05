#include "emulate.hpp"
#include "system.hpp"
#include <fmt/format.h>


int main(int argc, const char** argv)
{
  if(argc != 2)
  {
    fmt::print("usage: {} filename\n", argv[0]);
    return 0;
  }

  // create a system
  // XXX i can't use the word system on its own?
  class system sys{argv[1]};

  //sys.bus().write(mos6502::reset_vector_location + 0, 0x00);
  //sys.bus().write(mos6502::reset_vector_location + 1, 0xC0);

  try
  {
    emulate(sys);
  }
  catch(std::exception& e)
  {
    std::cerr << "Caught exception: " << e.what() << std::endl;
    std::cerr << std::endl;

    auto zp = sys.bus().zero_page();
    fmt::print("Zero page\n");
    for(std::uint8_t row = 0; row < 16; ++row)
    {
      const std::uint8_t* d = zp.data() + 16 * row;
      fmt::print("${:X}0: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}\n", 
                     row,  d[0],  d[1],  d[2],  d[3],  d[4],  d[5],  d[6],  d[7],  d[8],  d[9],  d[10], d[11], d[12], d[13], d[14], d[15]);
    }
  }

  return 0;
}

