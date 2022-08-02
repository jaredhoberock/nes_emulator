#include "gui.hpp"
#include "system.hpp"

int main()
{
  // create a system
  // XXX i can't use the word system on its own?
  class system sys{"nestest.nes"};

  return gui(sys);
}

