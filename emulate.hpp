#include "system.hpp"
#include <atomic>
#include <iostream>


inline void emulate(class system& sys, std::atomic<bool>& cancelled, std::atomic<bool>& paused, std::ostream& cpu_log, std::ostream& error_log)
{
  // reset the cpu
  int cycle = sys.cpu().reset();

  try
  {
    while(not cancelled)
    {
      // wait until unpaused
      paused.wait(true);

      // log current cpu state
      sys.cpu().log(cpu_log, cycle);

      // execute the next instruction
      sys.cpu().step_instruction();
    }
  }
  catch(std::exception& e)
  {
    error_log << "emulate: Caught exception: " << e.what() << std::endl;
  }
}

