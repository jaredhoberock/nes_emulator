#include "gui.hpp"
#include "system.hpp"

int main()
{
  // create a system
  // XXX i can't use the word system on its own?
  class system sys{"nestest.nes"};

  // initialize the reset vector location for headless nestest
  sys.bus().write(mos6502::reset_vector_location, 0x00);
  sys.bus().write(mos6502::reset_vector_location + 1, 0xC0);

  return gui(sys);
}

