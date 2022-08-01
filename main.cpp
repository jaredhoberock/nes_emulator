#include "bus.hpp"
#include "cartridge.hpp"
#include "mos6502.hpp"
#include <iostream>
#include <stdexcept>


int main()
{
  // create a bus
  bus b{{}, cartridge{"nestest.nes"}};

  // initialize the reset vector location for headless nestest
  b.write(mos6502::reset_vector_location, 0x00);
  b.write(mos6502::reset_vector_location + 1, 0xC0);

  // create a cpu
  mos6502 cpu{b};

  // reset the cpu
  int cycle = cpu.reset();

  try
  {
    while(true)
    {
      // log current state
      cpu.log(std::cout, cycle);
  
      // execute the next instruction
      cycle += cpu.step_instruction();
    }
  }
  catch(std::exception& e)
  {
    std::cerr << "Caught exception: " << e.what() << std::endl;
  }

  return 0;
}

