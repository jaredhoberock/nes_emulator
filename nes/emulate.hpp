#include "system.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>


namespace nes
{


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


inline void emulate(class system& sys, std::atomic<bool>& cancelled, std::atomic<bool>& paused, std::ostream& cpu_log, std::ostream& error_log, std::function<void(float)> audio = [](float){})
{
  // this approach steps the cpu one instruction and then steps the ppu 3
  // times as many cycles as the number of cpu cycles consumed

  auto frame_began = std::chrono::high_resolution_clock::now();

  float mean_audio_sample = 0;
  std::size_t num_audio_samples = 0;
  // XXX this corresponds to an audio sampling rate of 88200
  std::array<std::size_t,4> samples_per_output = {20, 20, 20, 21};

  std::size_t ppu_cycle = 0;
  std::size_t cpu_cycle = sys.cpu().reset();

  for(std::size_t i = 0; i < cpu_cycle; ++i)
  {
    sys.apu().step_cycle();
  }

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

          auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - frame_began);

          if(frame_duration < std::chrono::microseconds(16667))
          {
            std::this_thread::sleep_for(std::chrono::microseconds(16667) - frame_duration);
          }

          frame_began = std::chrono::high_resolution_clock::now();
        }
      }

      // let the apu catch up to the cpu
      for(std::size_t i = 0; i < num_cpu_cycles; ++i)
      {
        sys.apu().step_cycle();
        ++num_audio_samples;
        mean_audio_sample += (sys.apu().sample() - mean_audio_sample)/num_audio_samples;

        // output an audio sample every so often
        if(num_audio_samples == samples_per_output.front())
        {
          audio(mean_audio_sample);
          mean_audio_sample = 0;
          num_audio_samples = 0;
          std::rotate(samples_per_output.begin(), samples_per_output.begin() + 1, samples_per_output.end());
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


} // end nes

