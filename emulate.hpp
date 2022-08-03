#include "system.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>


inline void emulate(class system& sys, std::atomic<bool>& cancelled, std::atomic<bool>& paused, std::ostream& cpu_log, std::ostream& error_log)
{
  // this approach steps the cpu one instruction and then steps the ppu 3
  // times as many pixels as the number of cpu cycles consumed

  auto frame_begin = std::chrono::high_resolution_clock::now();

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

      // let the ppu catch up to the cpu
      for(int i = 0; i < 3 * num_cpu_cycles; ++i)
      {
        sys.ppu().step_cycle();
        ++ppu_cycle;

        // detect the end of a frame
        // XXX get these numbers from elsewhere -- they are the number of pixels per frame
        if(ppu_cycle == 341 * 261)
        {
          ppu_cycle = 0;

          // how long did last frame take us?
          auto frame_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - frame_begin);

          // sleep until we need to start the next frame
          std::this_thread::sleep_for(std::chrono::microseconds(16670) - frame_us);

          frame_begin = std::chrono::high_resolution_clock::now();
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

