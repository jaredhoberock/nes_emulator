#pragma once

#include <cassert>
#include <cstdint>


class divider
{
  public:
    inline divider()
      : period_{0}, counter_{0}
    {}

    inline void set_period(std::uint8_t period)
    {
      period_ = period;
    }

    inline void reset()
    {
      counter_ = period_;
    }

    inline std::uint8_t counter() const
    {
      return counter_;
    }

    inline bool clock()
    {
      bool result = counter_ == 0;

      if(result)
      {
        reset();
      }
      else
      {
        counter_--;
      }

      return result;
    }

  private:
    std::uint8_t period_;
    std::uint8_t counter_;
};


class timer
{
  public:
    inline timer()
      : period_{0}, value_{0}
    {}

    inline void set_high_three_bits_of_period(std::uint8_t value)
    {
      assert(value <= 0b111);
      std::uint16_t p = (value << 8) | (period() & 0xFF);
      set_period(p);
    }

    inline void set_low_eight_bits_of_period(std::uint8_t value)
    {
      std::uint16_t p = (period() & 0x700) | value;
      set_period(p);
    }

    inline void set_period(std::uint16_t value)
    {
      assert(value <= 0b11111111111);
      period_ = value;
    }

    inline std::uint16_t period() const
    {
      return period_;
    }

    inline bool clock()
    {
      bool result = (value_ == 0);

      if(result)
      {
        value_ = period_;
      }
      else
      {
        value_--;
      }

      return result;
    }

  private:
    // note that the timer's period is actually 1 + period_, because the clock() function's signal happens with a delay of one clock cycle
    std::uint16_t period_;
    std::uint16_t value_;
};


class sweep
{
  public:
    inline sweep(timer& t, bool subtract_extra)
      : timer_{t}, subtract_extra_{subtract_extra}, enabled_{false}, negated_{false}, shift_count_{0}, do_reload_{true}
    {}

    inline void set(bool enabled, std::uint8_t period, bool negated, std::uint8_t shift_count)
    {
      assert(period < 8);
      assert(shift_count < 8);

      divider_.set_period(period);
      enabled_ = enabled;
      negated_ = negated;
      shift_count_ = shift_count;
      do_reload_ = true;
    }

    inline void clock()
    {
      if(do_reload_)
      {
        if(enabled_ and divider_.clock())
        {
          maybe_adjust_timer_period();
        }

        divider_.reset();

        do_reload_ = false;
      }
      else
      {
        if(divider_.counter() > 0)
        {
          divider_.clock();
        }
        else if(enabled_ and divider_.clock())
        {
          maybe_adjust_timer_period();
        }
      }
    }

    inline bool value() const
    {
      return not silence();
    }

  private:
    inline bool silence() const
    {
      return (timer_.period() < 8) or (target_period() > 0x7FF);
    }

    inline std::uint16_t target_period() const
    {
      std::uint16_t result = timer_.period();

      std::uint16_t shifted_period = timer_.period() >> shift_count_;

      if(negated_)
      {
        result -= shifted_period;

        if(subtract_extra_)
        {
          --result;
        }
      }
      else
      {
        result += shifted_period;
      }

      return result;
    }

    void maybe_adjust_timer_period()
    {
      if(enabled_ and shift_count_ > 0 and not silence())
      {
        timer_.set_period(target_period());
      }
    }

    timer& timer_;
    bool subtract_extra_;
    divider divider_;
    bool enabled_;
    bool negated_;
    std::uint8_t shift_count_;
    bool do_reload_;
};


class volume_envelope
{
  public:
    inline volume_envelope()
      : divider_{}, do_reset_{true}, loop_{false}, constant_{false}, volume_{0}, counter_{0}
    {}

    inline void set(bool loop, bool constant, std::uint8_t volume)
    {
      loop_ = loop;
      constant_ = constant;

      assert(volume < 16);
      volume_ = volume;
      divider_.set_period(volume);
    }

    inline void reset()
    {
      do_reset_ = true;
    }

    inline void clock()
    {
      if(do_reset_)
      {
        counter_ = 15;
        do_reset_ = false;
        divider_.reset();
      }
      else
      {
        if(divider_.clock())
        {
          if(counter_)
          {
            --counter_;
          }
          else if(loop_)
          {
            counter_ = 15;
          }
        }
      }
    }

    inline std::uint8_t value() const
    {
      return constant_ ? volume_ : counter_;
    }

  private:
    divider divider_;
    bool do_reset_;
    bool loop_;
    bool constant_;
    std::uint8_t volume_;
    std::uint8_t counter_;
};


class length_counter
{
  public:
    inline length_counter()
      : enabled_{false}, halted_{false}, counter_{0}
    {}

    inline void enable(bool enabled)
    {
      enabled_ = enabled;

      if(not enabled_)
      {
        counter_ = 0;
      }
    }

    inline void halt(bool halted)
    {
      halted_ = halted;
    }

    inline void clock()
    {
      if(counter_ != 0 and not halted_)
      {
        counter_--;
      }
    }

    inline void maybe_set_value_from_lookup_table(std::uint8_t index)
    {
      // see https://www.nesdev.org/wiki/APU_Length_Counter
      constexpr std::uint8_t table[] = {
        10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
      };

      // see https://www.nesdev.org/wiki/APU_Length_Counter
      // "If the enabled flag is set, the length counter is loaded with..."
      if(enabled_)
      {
        counter_ = table[index];
      }
    }

    inline bool value() const
    {
      return counter_ != 0;
    }

  private:
    bool enabled_;
    bool halted_;
    std::uint8_t counter_;
};


class pulse_wave
{
  public:
    inline pulse_wave()
      : duty_cycle_{0}, step_{0}
    {}

    inline void reset()
    {
      step_ = 0;
    }

    inline void set_duty_cycle(std::uint8_t value)
    {
      assert(value < 4);
      duty_cycle_ = value;
    }

    inline void clock()
    {
      ++step_;
      step_ %= 8;
    }

    inline bool value() const
    {
      // see https://www.nesdev.org/wiki/APU_Pulse#Sequencer_behavior,
      // Duty Cycle Sequences
      constexpr bool sequences[4][8] = {
        {0, 1, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 0, 0, 0},
        {1, 0, 0, 1, 1, 1, 1, 1}
      };

      return sequences[duty_cycle_][step_];
    }

  private:
    std::uint8_t duty_cycle_;
    std::uint8_t step_;
};


class pulse_channel
{
  public:
    inline pulse_channel(bool is_channel_0)
      : timer_{}, volume_envelope_{}, sequencer_{}, length_counter_{}, sweep_{timer_, is_channel_0}
    {}

    inline bool length_counter_status() const
    {
      return length_counter_.value() > 0;
    }

    inline void set_duty_cycle_and_volume_envelope(std::uint8_t duty, bool loop_volume, bool constant_volume, std::uint8_t volume_period)
    {
      sequencer_.set_duty_cycle(duty);
      length_counter_.halt(loop_volume);
      volume_envelope_.set(loop_volume, constant_volume, volume_period);
    }

    inline void set_length_counter_and_timer_high_bits(std::uint8_t table_index, std::uint8_t timer_bits)
    {
      length_counter_.maybe_set_value_from_lookup_table(table_index);
      timer_.set_high_three_bits_of_period(timer_bits);

      sequencer_.reset();
      volume_envelope_.reset();
    }

    inline void set_timer_low_bits(std::uint8_t timer_bits)
    {
      timer_.set_low_eight_bits_of_period(timer_bits);
    }

    inline void set_sweep(bool enabled, std::uint8_t period, bool negated, std::uint8_t shift_count)
    {
      sweep_.set(enabled, period, negated, shift_count);
    }

    inline void enable(bool enabled)
    {
      length_counter_.enable(enabled);
    }

    inline void clock()
    {
      if(timer_.clock())
      {
        sequencer_.clock();
      }
    }

    inline void clock_half_frame_signals()
    {
      length_counter_.clock();
      sweep_.clock();
    }

    inline void clock_quarter_frame_signals()
    {
      volume_envelope_.clock();
    }

    inline std::uint8_t value() const
    {
      return volume_envelope_.value() * sweep_.value() * length_counter_.value() * sequencer_.value();
    }

  private:
    timer timer_;
    volume_envelope volume_envelope_;
    pulse_wave sequencer_;
    length_counter length_counter_;
    sweep sweep_;
};


class linear_counter
{
  public:
    inline linear_counter()
      : control_{false}, do_reload_{false}, period_{0}, counter_{0}
    {}

    inline void set(bool control, std::uint8_t period)
    {
      assert(period <= 0b01111111);

      control_ = control;
      period_ = period;
    }

    inline void reset()
    {
      do_reload_ = true;
    }

    inline void clock()
    {
      if(do_reload_)
      {
        counter_ = period_;
      }
      else if(counter_ != 0)
      {
        --counter_;
      }

      if(not control_)
      {
        do_reload_ = false;
      }
    }

    inline bool value() const
    {
      return counter_ != 0;
    }

  private:
    bool control_;
    bool do_reload_;
    std::uint8_t period_;
    std::uint8_t counter_;
};


class triangle_wave
{
  public:
    inline triangle_wave()
      : step_{0}
    {}

    inline void reset()
    {
      step_ = 0;
    }

    inline void clock()
    {
      ++step_;
      step_ %= 32;
    }

    inline std::uint8_t value() const
    {
      // see https://www.nesdev.org/wiki/APU_Triangle
      constexpr std::uint8_t sequence[] = {
        15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
      };

      return sequence[step_];
    }

  private:
    std::uint8_t step_;
};


class triangle_channel
{
  public:
    inline void enable(bool enabled)
    {
      length_counter_.enable(enabled);
    }

    inline bool length_counter_status() const
    {
      return length_counter_.value() > 0;
    }

    inline void set_linear_counter(bool control, std::uint8_t period)
    {
      linear_counter_.set(control, period);
      length_counter_.halt(control);
    }

    inline void set_length_counter_and_timer_high_bits(std::uint8_t table_index, std::uint8_t timer_bits)
    {
      length_counter_.maybe_set_value_from_lookup_table(table_index);
      timer_.set_high_three_bits_of_period(timer_bits);

      linear_counter_.reset();
    }

    inline void set_timer_low_bits(std::uint8_t timer_bits)
    {
      timer_.set_low_eight_bits_of_period(timer_bits);
    }

    inline void clock()
    {
      // see https://www.nesdev.org/wiki/APU_Triangle
      // "The sequencer is clocked by the timer as long as both the linear
      // counter and the length counter are nonzero."
      if(timer_.clock() and linear_counter_.value() and length_counter_.value())
      {
        sequencer_.clock();
      }
    }

    inline void clock_half_frame_signals()
    {
      length_counter_.clock();
    }

    inline void clock_quarter_frame_signals()
    {
      linear_counter_.clock();
    }

    inline std::uint8_t value() const
    {
      // see https://www.nesdev.org/wiki/APU#Triangle_($4008-400B)
      // "silencing the channel [via the linear or length counter]
      // merely halts it, it will continue to output its last value,
      // rather than 0."
      return sequencer_.value();
    }

  private:
    timer timer_;
    linear_counter linear_counter_;
    length_counter length_counter_;
    triangle_wave sequencer_;
};


class linear_feedback_shift_register
{
  public:
    inline linear_feedback_shift_register()
      : mode_{false}, value_{1}
    {}

    inline void set_mode(bool mode)
    {
      mode_ = mode;
    }

    inline void clock()
    {
      bool bit_0 = (value_ & 0b1) != 0;
      bool bit_1 = (value_ & 0b10) != 0;
      bool bit_6 = (value_ & 0b1000000) != 0;

      bool other_bit = mode_ ? bit_6 : bit_1;

      bool feedback = bit_0 ^ other_bit;

      // shift the value to the right
      value_ >>= 1;

      // set bit 14 to the value of feedback
      value_ |= (feedback << 14);
    }

    inline bool value() const
    {
      bool bit_0 = (value_ & 0b1) != 0;

      return not bit_0;
    }

  private:
    bool mode_;
    std::uint16_t value_;
};


class noise_channel
{
  public:
    inline void enable(bool enabled)
    {
      length_counter_.enable(enabled);
    }

    inline bool length_counter_status() const
    {
      return length_counter_.value() > 0;
    }

    inline void set_length_counter_halt_and_volume_envelope(bool halt, bool constant_volume, std::uint8_t volume_period)
    {
      length_counter_.halt(halt);
      volume_envelope_.set(true, constant_volume, volume_period);
    }

    inline void set_mode_and_timer_period(bool mode, std::uint8_t index)
    {
      assert(index < 16);

      shift_register_.set_mode(mode);

      constexpr std::uint16_t period[] =
      {
        4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
      };

      timer_.set_period(period[index]);
    }

    inline void set_length_counter(std::uint8_t index)
    {
      length_counter_.maybe_set_value_from_lookup_table(index);
      volume_envelope_.reset();
    }

    inline void clock()
    {
      if(timer_.clock())
      {
        shift_register_.clock();
      }
    }

    inline void clock_half_frame_signals()
    {
      length_counter_.clock();
    }

    inline void clock_quarter_frame_signals()
    {
      volume_envelope_.clock();
    }

    inline std::uint8_t value() const
    {
      return volume_envelope_.value() * length_counter_.value() * shift_register_.value();
    }

  private:
    timer timer_;
    linear_feedback_shift_register shift_register_;
    length_counter length_counter_;
    volume_envelope volume_envelope_;
};


class frame_counter
{
  public:
    inline frame_counter(pulse_channel& pulse_0, pulse_channel& pulse_1, triangle_channel& triangle, noise_channel& noise)
      : frame_interrupt_flag_{false},
        in_five_step_mode_{false},
        inhibit_interrupts_{false},
        num_cpu_cycles_{0},
        pulse_0_{pulse_0},
        pulse_1_{pulse_1},
        triangle_{triangle},
        noise_{noise}
    {}

    // this gets called once each CPU cycle
    inline void clock()
    {
      if(in_five_step_mode_)
      {
        five_step_mode_clock();
      }
      else
      {
        four_step_mode_clock();
      }
    }

    void set(bool five_step_mode, bool inhibit_interrupts)
    {
      in_five_step_mode_ = five_step_mode;
      inhibit_interrupts_ = inhibit_interrupts;

      if(in_five_step_mode_)
      {
        // see https://www.nesdev.org/wiki/APU_Frame_Counter
        // "if the mode flag is set (i.e. if in five step mode), then both quarter frame and half frame signals are also generated
        clock_quarter_frame_signals();
        clock_half_frame_signals();
      }

      // the counter is also reset
      num_cpu_cycles_ = 0;
    }

    inline bool frame_interrupt_flag()
    {
      bool result = frame_interrupt_flag_;
      frame_interrupt_flag_ = false;
      return result;
    }

  private:
    void clock_quarter_frame_signals()
    {
      pulse_0_.clock_quarter_frame_signals();
      pulse_1_.clock_quarter_frame_signals();
      triangle_.clock_quarter_frame_signals();
      noise_.clock_quarter_frame_signals();
    }

    void clock_half_frame_signals()
    {
      pulse_0_.clock_half_frame_signals();
      pulse_1_.clock_half_frame_signals();
      triangle_.clock_half_frame_signals();
      noise_.clock_half_frame_signals();
    }

    constexpr static std::size_t to_cpu_cycles(double value)
    {
      return 2 * value;
    }

    inline void four_step_mode_clock()
    {
      // see https://www.nesdev.org/wiki/APU_Frame_Counter table Mode 0
      switch(num_cpu_cycles_)
      {
        case to_cpu_cycles(3728.5):
        case to_cpu_cycles(11185.5):
        {
          clock_quarter_frame_signals();
          break;
        }

        case to_cpu_cycles(7456.5):
        {
          clock_quarter_frame_signals();
          clock_half_frame_signals();
          break;
        }

        case to_cpu_cycles(14914):
        case to_cpu_cycles(14915):
        {
          if(not inhibit_interrupts_)
          {
            frame_interrupt_flag_ = true;
          }

          break;
        }

        case to_cpu_cycles(14914.5):
        {
          clock_quarter_frame_signals();
          clock_half_frame_signals();
          
          if(not inhibit_interrupts_)
          {
            frame_interrupt_flag_ = true;
          }

          break;
        }

        default:
        {
          // do nothing
          break;
        }
      }

      num_cpu_cycles_ = (num_cpu_cycles_ == to_cpu_cycles(14915)) ? 0 : num_cpu_cycles_ + 1;
    }

    inline void five_step_mode_clock()
    {
      // see https://www.nesdev.org/wiki/APU_Frame_Counter table Mode 1
      switch(num_cpu_cycles_)
      {
        case to_cpu_cycles(3728.5):
        case to_cpu_cycles(11185.5):
        {
          clock_quarter_frame_signals();
          break;
        }

        case to_cpu_cycles(7456.5):
        case to_cpu_cycles(18640.5):
        {
          clock_quarter_frame_signals();
          clock_half_frame_signals();
          break;
        }

        default:
        {
          // do nothing
          break;
        }
      }

      num_cpu_cycles_ = (num_cpu_cycles_ == to_cpu_cycles(18641)) ? 0 : num_cpu_cycles_ + 1;
    }

    bool frame_interrupt_flag_;
    bool in_five_step_mode_;
    bool inhibit_interrupts_;
    std::size_t num_cpu_cycles_;
    pulse_channel& pulse_0_;
    pulse_channel& pulse_1_;
    triangle_channel& triangle_;
    noise_channel& noise_;
};


class apu
{
  public:
    inline apu()
      : is_odd_cpu_clock_{false},
        pulse_0_{true},
        pulse_1_{false},
        triangle_{},
        noise_{},
        frame_counter_{pulse_0_, pulse_1_, triangle_, noise_}
    {}

    inline void step_cycle()
    {
      // the frame counter gets clocked every cpu clock
      frame_counter_.clock();

      // so does the triangle
      triangle_.clock();

      // other channels clock every other cpu clock
      if(is_odd_cpu_clock_)
      {
        pulse_0_.clock();
        pulse_1_.clock();
        noise_.clock();
      }

      is_odd_cpu_clock_ = !is_odd_cpu_clock_;
    }

    inline void set_frame_counter_mode_and_interrupts(bool five_step_mode, bool inhibit_interrupts)
    {
      frame_counter_.set(five_step_mode, inhibit_interrupts);
    }

    inline bool frame_interrupt_flag()
    {
      return frame_counter_.frame_interrupt_flag();
    }

    inline void enable_channels(bool enable_dmc, bool enable_noise, bool enable_triangle, bool enable_pulse_1, bool enable_pulse_0)
    {
      //dmc_.enable(enable_dmc);
      noise_.enable(enable_noise);
      triangle_.enable(enable_triangle);
      pulse_0_.enable(enable_pulse_0);
      pulse_1_.enable(enable_pulse_1);
    }

    inline bool pulse_0_length_counter_status() const
    {
      return pulse_0_.length_counter_status();
    }

    inline void set_pulse_0_duty_cycle_and_volume_envelope(std::uint8_t duty, bool loop_volume, bool constant_volume, std::uint8_t volume_period)
    {
      pulse_0_.set_duty_cycle_and_volume_envelope(duty, loop_volume, constant_volume, volume_period);
    }

    inline void set_pulse_0_length_counter_and_timer_high_bits(std::uint8_t length_counter_table_index, std::uint8_t timer_bits)
    {
      pulse_0_.set_length_counter_and_timer_high_bits(length_counter_table_index, timer_bits);
    }

    inline void set_pulse_0_timer_low_bits(std::uint8_t timer_bits)
    {
      pulse_0_.set_timer_low_bits(timer_bits);
    }

    inline void set_pulse_0_sweep(bool enabled, std::uint8_t period, bool negated, std::uint8_t shift_count)
    {
      pulse_0_.set_sweep(enabled, period, negated, shift_count);
    }

    inline bool pulse_1_length_counter_status() const
    {
      return pulse_1_.length_counter_status();
    }

    inline void set_pulse_1_duty_cycle_and_volume_envelope(std::uint8_t duty, bool loop_volume, bool constant_volume, std::uint8_t volume_period)
    {
      pulse_1_.set_duty_cycle_and_volume_envelope(duty, loop_volume, constant_volume, volume_period);
    }

    inline void set_pulse_1_length_counter_and_timer_high_bits(std::uint8_t length_counter_table_index, std::uint8_t timer_bits)
    {
      pulse_1_.set_length_counter_and_timer_high_bits(length_counter_table_index, timer_bits);
    }

    inline void set_pulse_1_timer_low_bits(std::uint8_t timer_bits)
    {
      pulse_1_.set_timer_low_bits(timer_bits);
    }

    inline void set_pulse_1_sweep(bool enabled, std::uint8_t period, bool negated, std::uint8_t shift_count)
    {
      pulse_1_.set_sweep(enabled, period, negated, shift_count);
    }

    inline bool triangle_length_counter_status() const
    {
      return triangle_.length_counter_status();
    }

    inline void set_triangle_linear_counter(bool control, std::uint8_t period)
    {
      triangle_.set_linear_counter(control, period);
    }

    inline void set_triangle_length_counter_and_timer_high_bits(std::uint8_t length_counter_table_index, std::uint8_t timer_bits)
    {
      triangle_.set_length_counter_and_timer_high_bits(length_counter_table_index, timer_bits);
    }

    inline void set_triangle_timer_low_bits(std::uint8_t timer_bits)
    {
      triangle_.set_timer_low_bits(timer_bits);
    }

    inline bool noise_length_counter_status()
    {
      return noise_.length_counter_status();
    }

    inline void set_noise_length_counter_halt_and_volume_envelope(bool halt_length_counter, bool constant_volume, std::uint8_t volume_period)
    {
      noise_.set_length_counter_halt_and_volume_envelope(halt_length_counter, constant_volume, volume_period);
    }

    inline void set_noise_mode_and_timer_period(bool mode, std::uint8_t index)
    {
      noise_.set_mode_and_timer_period(mode, index);
    }

    inline void set_noise_length_counter(std::uint8_t index)
    {
      noise_.set_length_counter(index);
    }

    inline float sample() const
    {
      float pulse = pulse_0_.value() + pulse_1_.value();

      float pulse_denom = 8128.f / pulse + 100.f;
      float pulse_out = 95.88f / pulse_denom;

      float triangle = triangle_.value();
      float noise = noise_.value();

      float tnd_denom = 1.f / (triangle/8227 + noise/12241) + 100.f;
      float tnd_out = 159.79f / tnd_denom;

      return pulse_out + tnd_out;
    }

  private:
    bool is_odd_cpu_clock_;
    pulse_channel pulse_0_;
    pulse_channel pulse_1_;
    triangle_channel triangle_;
    noise_channel noise_;
    frame_counter frame_counter_;
};

