#include "system.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>


inline void emulate(class system& sys)
{
  // this approach steps the cpu one instruction and then steps the ppu 3
  // times as many cycles as the number of cpu cycles consumed

  std::size_t ppu_cycle = 0;
  std::size_t cpu_cycle = sys.cpu().reset();
  for(std::size_t i = 0; i < 3 * cpu_cycle; ++i)
  {
    sys.ppu().step_cycle();
    ++ppu_cycle;
  }

  try
  {
    while(true)
    {
      // log current cpu state
      //sys.cpu().log(std::cout, cpu_cycle, ppu_cycle);

      std::size_t num_cpu_cycles = 0;

      if(sys.bus().dma_in_progress())
      {
        // the cpu is suspended during a dma
        sys.bus().step_dma_cycle(cpu_cycle);
        num_cpu_cycles = 1;
      }
      else
      {
        // execute the next instruction
        num_cpu_cycles = sys.cpu().step_instruction();

        // execute any nonmaskable interrupt
        if(sys.ppu().nmi)
        {
          num_cpu_cycles += sys.cpu().nonmaskable_interrupt();
          sys.ppu().nmi = false;
        }
      }

      // let the ppu catch up to the cpu
      for(std::size_t i = 0; i < 3 * num_cpu_cycles; ++i)
      {
        sys.ppu().step_cycle();
        ++ppu_cycle;
      }

      cpu_cycle += num_cpu_cycles;
    }
  }
  catch(std::exception& e)
  {
    std::cerr << "emulate: Caught exception: " << e.what() << std::endl;
  }
}


inline void emulate(class system& sys, std::atomic<bool>& cancelled, std::atomic<bool>& paused, std::ostream& cpu_log, std::ostream& error_log)
{
  // this approach steps the cpu one instruction and then steps the ppu 3
  // times as many cycles as the number of cpu cycles consumed

  std::size_t ppu_cycle = 0;
  std::size_t cpu_cycle = sys.cpu().reset();
  for(std::size_t i = 0; i < 3 * cpu_cycle; ++i)
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
      //sys.cpu().log(cpu_log, cpu_cycle, ppu_cycle);

      std::size_t num_cpu_cycles = 0;

      if(sys.bus().dma_in_progress())
      {
        // the cpu is suspended during a dma
        sys.bus().step_dma_cycle(cpu_cycle);
        num_cpu_cycles = 1;
      }
      else
      {
        // execute the next instruction
        num_cpu_cycles = sys.cpu().step_instruction();

        // execute any nonmaskable interrupt
        if(sys.ppu().nmi)
        {
          num_cpu_cycles += sys.cpu().nonmaskable_interrupt();
          sys.ppu().nmi = false;
        }
      }

      // let the ppu catch up to the cpu
      for(std::size_t i = 0; i < 3 * num_cpu_cycles; ++i)
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

