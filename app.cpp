#include "gui.hpp"
#include "nes/system.hpp"

int main()
{
  // create a system
  nes::system sys{"nestest.nes"};

  return gui(sys);
}

