#include "system.hpp"
#include <atomic>
#include <iostream>


inline void emulate(class system& sys, std::atomic<bool>& cancelled, std::atomic<bool>& paused, std::ostream& cpu_log, std::ostream& error_log)
{
  // this approach steps the cpu one instruction and then steps the ppu 3
  // times as many pixels as the number of cpu cycles consumed

  int ppu_cycle = 0;
  int cpu_cycle = sys.cpu().reset();
  for(int i = 0; i < cpu_cycle; ++i)
  {
    sys.ppu().step_pixel();
    ++ppu_cycle;
  }

  try
  {
    while(not cancelled)
    {
      // wait until unpaused
      paused.wait(true);

      // log current cpu state
      sys.cpu().log(cpu_log, cpu_cycle, ppu_cycle);

      // execute the next instruction
      int num_cpu_cycles = sys.cpu().step_instruction();

      // let the ppu catch up to the cpu
      for(int i = 0; i < num_cpu_cycles; ++i)
      {
        sys.ppu().step_pixel();
        ++ppu_cycle;

        // detect the end of a frame
        // XXX get these numbers from elsewhere -- they are the number of pixels per frame
        // XXX we would sleep here in real time mode
        if(ppu_cycle == 341 * 261)
        {
          ppu_cycle = 0;
        }
      }

      cpu_cycle += num_cpu_cycles;
    }
  }
  catch(std::exception& e)
  {
    error_log << "emulate: Caught exception: " << e.what() << std::endl;
  }
}

