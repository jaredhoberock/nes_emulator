#include "bus.hpp"
#include "cartridge.hpp"
#include "mos6502.hpp"
#include "system.hpp"
#include <iostream>
#include <stdexcept>


int main()
{
  // create a system
  // XXX i can't use the word system on its own?
  class system sys{"nestest.nes"};

  // initialize the reset vector location for headless nestest
  sys.bus().write(mos6502::reset_vector_location, 0x00);
  sys.bus().write(mos6502::reset_vector_location + 1, 0xC0);

  // reset the cpu
  int cycle = sys.cpu().reset();

  try
  {
    while(true)
    {
      // log current state
      sys.cpu().log(std::cout, cycle, 3 * cycle);
  
      // execute the next instruction
      cycle += sys.cpu().step_instruction();
    }
  }
  catch(std::exception& e)
  {
    std::cerr << "Caught exception: " << e.what() << std::endl;
  }

  return 0;
}

