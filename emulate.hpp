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
      cycle += sys.cpu().step_instruction();
    }
  }
  catch(std::exception& e)
  {
    error_log << "emulate: Caught exception: " << e.what() << std::endl;
  }
}


inline void step_cycle(mos6502& cpu, int& total_num_cycles, int& countdown, std::ostream& cpu_log)
{
  if(countdown == 0)
  {
    cpu.log(cpu_log, total_num_cycles);

    countdown = cpu.step_instruction();
    total_num_cycles += countdown;
  }
  else
  {
    countdown--;
  }
}


inline void new_emulate(class system& sys, std::atomic<bool>& cancelled, std::atomic<bool>& paused, std::ostream& cpu_log, std::ostream& error_log)
{
  int num_cpu_cycles = sys.cpu().reset();

  // when this value reaches zero, step_cycle steps a cpu instruction
  int cpu_cycle_countdown = 0;

  try
  {
    while(not cancelled)
    {
      // wait until unpaused
      paused.wait(true);

      // each system cycle, the ppu outputs a pixel
      // there are three ppu cycles for each cpu cycle
      sys.ppu().step_pixel();
      sys.ppu().step_pixel();
      sys.ppu().step_pixel();

      // after three system cycles, we do a cpu cycle
      // step_cycle executes a cpu instruction when
      // cpu_cycle_countdown reaches zero
      // XXX this seems like it will put the cpu an instruction behind the ppu
      step_cycle(sys.cpu(), num_cpu_cycles, cpu_cycle_countdown, cpu_log);
    }
  }
  catch(std::exception& e)
  {
    error_log << "new_emulate: Caught exception: " << e.what() << std::endl;
  }
}

