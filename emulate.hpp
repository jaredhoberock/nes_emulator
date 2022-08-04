#include "system.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>


inline void emulate(class system& sys, std::atomic<bool>& cancelled, std::atomic<bool>& paused, std::ostream& cpu_log, std::ostream& error_log)
{
  // this approach steps the cpu one instruction and then steps the ppu 3
  // times as many pixels as the number of cpu cycles consumed

  int ppu_cycle = 0;
  int cpu_cycle = sys.cpu().reset();
  for(int i = 0; i < 3 * cpu_cycle; ++i)
  {
    sys.ppu().step_cycle();
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

      // execute any nonmaskable interrupt
      if(sys.ppu().nmi)
      {
        num_cpu_cycles += sys.cpu().nonmaskable_interrupt();
        sys.ppu().nmi = false;
      }

      // let the ppu catch up to the cpu
      for(int i = 0; i < 3 * num_cpu_cycles; ++i)
      {
        sys.ppu().step_cycle();
        ++ppu_cycle;
      }

      cpu_cycle += num_cpu_cycles;
    }
  }
  catch(std::exception& e)
  {
    error_log << "emulate: Caught exception: " << e.what() << std::endl;
  }
}

