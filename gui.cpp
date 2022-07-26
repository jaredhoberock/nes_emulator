// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "gui.hpp"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "nes/emulate.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fmt/format.h>
#include <future>
#include <iostream>
#include <optional>
#include <stdio.h>
#include <string>
#include <tuple>
#include <SDL.h>
#include <SDL_opengl.h>


struct null_buffer : std::streambuf
{
  int overflow(int c) { return c; };
};

null_buffer nb;
std::ostream null_stream{&nb};


std::future<void> make_ready_future()
{
  std::promise<void> p;
  p.set_value();
  return p.get_future();
}


bool is_complete(std::future<void>& f)
{
  return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}


void update_controller_state(nes::system& sys)
{
  std::uint8_t controller_zero_state = 0;

  if(ImGui::IsKeyPressed(SDL_SCANCODE_X))
  {
    controller_zero_state |= nes::system::a_button_bitmask;
  }

  if(ImGui::IsKeyPressed(SDL_SCANCODE_Z))
  {
    controller_zero_state |= nes::system::b_button_bitmask;
  }

  if(ImGui::IsKeyPressed(SDL_SCANCODE_RSHIFT))
  {
    controller_zero_state |= nes::system::select_button_bitmask;
  }

  if(ImGui::IsKeyPressed(SDL_SCANCODE_RETURN))
  {
    controller_zero_state |= nes::system::start_button_bitmask;
  }

  if(ImGui::IsKeyPressed(SDL_SCANCODE_UP))
  {
    controller_zero_state |= nes::system::up_button_bitmask;
  }

  if(ImGui::IsKeyPressed(SDL_SCANCODE_DOWN))
  {
    controller_zero_state |= nes::system::down_button_bitmask;
  }

  if(ImGui::IsKeyPressed(SDL_SCANCODE_LEFT))
  {
    controller_zero_state |= nes::system::left_button_bitmask;
  }

  if(ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT))
  {
    controller_zero_state |= nes::system::right_button_bitmask;
  }

  sys.set_controller(0, controller_zero_state);
}


class disassembly_window
{
  private:
    std::map<std::uint16_t, std::string> disassembly_;

  public:
    disassembly_window(const nes::system& sys)
      : disassembly_(sys.cpu().disassemble_program())
    {}

    void draw(const nes::system& sys)
    {
      ImGui::Begin("Disassembly");
      std::uint16_t focal_address = sys.cpu().program_counter();
      auto focal_instruction = disassembly_.find(focal_address);
      if(focal_instruction == disassembly_.end())
      {
        ImGui::Text("Couldn't find instruction");
      }
      else
      {
        // rewind to the instructions before the focal instruction
        std::uint16_t num_instructions = 100;
        std::uint16_t counter = num_instructions / 2;
        auto instruction = focal_instruction;
        while(instruction != disassembly_.begin() and counter > 0)
        {
          --instruction;
          --counter;
        }

        // draw the instructions before the focal address
        for(; instruction != focal_instruction; ++instruction)
        {
          ImGui::Text("$%04X: %s", instruction->first, instruction->second.c_str());
        }

        // draw the focal instruction
        ImGui::TextColored(ImVec4(0,1,0,1), "$%04X: %s", focal_instruction->first, focal_instruction->second.c_str());

        // draw the instructions after the focal instruction
        counter = num_instructions / 2;
        for(auto instruction = std::next(focal_instruction); instruction != disassembly_.end() and counter > 0; ++instruction)
        {
          ImGui::Text("$%04X: %s", instruction->first, instruction->second.c_str());
          --counter;
        }
      }

      ImGui::End();
    }
};


void draw_zero_page(const nes::system& sys)
{
  std::span zp = sys.zero_page();

  ImGui::Begin("Zero page");
  for(std::uint8_t row = 0; row < 16; ++row)
  {
    const std::uint8_t* d = zp.data() + 16 * row;
    ImGui::Text("$%X0: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                  row, d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15]);
  }
  ImGui::End();
}

void draw_nametable(const nes::system& sys, int which)
{
  std::span nt = sys.nametable(which);

  const char* title = which == 0 ? "Nametable 0" : "Nametable 1";
  ImGui::Begin(title);
  for(int row = 0; row < 30; ++row)
  {
    for(int tile = 0; tile < 32; ++tile)
    {
      ImGui::Text("%02X", nt[row * 32 + tile]);
      if(tile != 31)
      {
        ImGui::SameLine();
      }
    }
  }
  ImGui::End();
}


void draw_object_attributes(const nes::system& sys)
{
  std::span attributes = sys.object_attributes();

  ImGui::Begin("Object Attributes");
  for(int i = 0; i < 20; ++i)
  {
    const auto& a = attributes[i];
    ImGui::Text("%02d: (%03d,%03d) Tile: %02X, Attribute: %02X", i, a.x_position, a.y_position, a.tile_id, a.attribute);
  }
  ImGui::End();
}


class palettes_window
{
  private:
    // there are four colors in a palette
    const int num_colors_ = 4;
    const ImGuiWindowFlags window_flags_ = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
    std::array<GLuint,8> textures_;

  public:
    palettes_window()
    {
      glGenTextures(textures_.size(), textures_.data());

      for(auto t : textures_)
      {
        glBindTexture(GL_TEXTURE_2D, t);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, num_colors_, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        
        if(glGetError())
        {
          throw std::runtime_error("palettes_window: GL error after glTexImage2D");
        }
      }
    }

    ~palettes_window()
    {
      glDeleteTextures(2, textures_.data());
    }

    std::optional<int> draw(const nes::system& sys) const
    {
      // copy the current state of the palettes into our textures
      for(size_t i = 0; i < textures_.size(); ++i)
      {
        // get the current contents of the selected palette
        std::array palette = sys.palette_as_image(i);

        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, num_colors_, 1, GL_RGB, GL_UNSIGNED_BYTE, palette.data());
        if(glGetError())
        {
          throw std::runtime_error(fmt::format("palettes_window: GL error after glCopyTexImage2D"));
        }
      }

      // draw the palettes as buttons and allow selection
      int frame_padding = 1;
      std::optional<int> result;
      ImGui::Begin("Palettes" , nullptr, window_flags_);
      for(size_t i = 0; i < textures_.size(); ++i)
      {
        void* tex_id = reinterpret_cast<void*>(textures_[i]);
        if(ImGui::ImageButton(tex_id, ImVec2(40, 10), ImVec2(0,0), ImVec2(1,1), frame_padding))
        {
          result = i;
        }
        ImGui::SameLine();
      }
      ImGui::End();

      return result;
    }
};



class pattern_tables_window
{
  private:
    const int width_  = nes::system::pattern_table_dim;
    const int height_ = nes::system::pattern_table_dim;
    const ImGuiWindowFlags window_flags_ = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
    std::array<GLuint,2> textures_;

  public:
    pattern_tables_window()
    {
      glGenTextures(2, textures_.data());

      for(auto t : textures_)
      {
        glBindTexture(GL_TEXTURE_2D, t);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        
        if(glGetError())
        {
          throw std::runtime_error("pattern_tables_window: GL error after glTexImage2D");
        }
      }
    }

    ~pattern_tables_window()
    {
      glDeleteTextures(2, textures_.data());
    }

    void draw(const nes::system& sys, int selected_palette) const
    {
      // copy the current state of the framebuffer into our texture
      for(int i = 0; i < 2; ++i)
      {
        // get the current contents of the pattern table
        std::array pattern_table = sys.pattern_table_as_image(i, selected_palette);

        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, pattern_table.data());
        if(glGetError())
        {
          throw std::runtime_error(fmt::format("pattern_tables_window: GL error after glCopyTexImage2D"));
        }
      }

      // draw a window displaying both pattern tables
      ImGui::Begin("Pattern tables" , nullptr, window_flags_);
      ImGui::Image(reinterpret_cast<void*>(textures_[0]), ImVec2(2*width_, 2*height_), ImVec2(0,0), ImVec2(1,1));
      ImGui::SameLine();
      ImGui::Image(reinterpret_cast<void*>(textures_[1]), ImVec2(2*width_, 2*height_), ImVec2(0,0), ImVec2(1,1));
      ImGui::End();
    }
};


class framebuffer_window
{
  private:
    const int width_  = nes::ppu::framebuffer_width;
    const int height_ = nes::ppu::framebuffer_height;
    const ImGuiWindowFlags window_flags_ = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
    GLuint texture_;

  public:
    framebuffer_window()
      : texture_{}
    {
      glGenTextures(1, &texture_);
      glBindTexture(GL_TEXTURE_2D, texture_);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
      
      if(glGetError())
      {
        throw std::runtime_error("framebuffer_window: GL error after glTexImage2D");
      }
    }

    ~framebuffer_window()
    {
      glDeleteTextures(1, &texture_);
    }

    void draw(const nes::system& sys) const
    {
      // copy the current state of the framebuffer into our texture
      glBindTexture(GL_TEXTURE_2D, texture_);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, sys.framebuffer().data());
      if(glGetError())
      {
        throw std::runtime_error(fmt::format("framebuffer_window: GL error after glCopyTexImage2D"));
      }

      // draw a window
      ImGui::Begin("Framebuffer", nullptr, window_flags_);
      ImGui::Image(reinterpret_cast<void*>(texture_), ImVec2(2 * width_, 2 * height_), ImVec2(0,0), ImVec2(1,1));
      ImGui::End();
    }
};


struct log_window
{
  std::string buffer_;
  bool auto_scroll_;

  log_window() : auto_scroll_{true}
  {
    append("test test test test test\n");
    append("test test test test test test test\n");
    append("test test test test\n");
    append("test test test test test\n");
    append("test test test test\n");
    append("test test test \n");
    append("test test test test test\n");
    append("test test test \n");
    append("test test test test\n");
    append("test test test test test\n");
    append("test test test test test test test\n");
    append("test test test test test\n");
    append("test test test test test\n");
    append("test test test test test test test\n");
    append("test test test test\n");
    append("test test test test test\n");
    append("test test test test\n");
    append("test test test \n");
    append("test test test test test\n");
    append("test test test \n");
    append("test test test test\n");
    append("test test test test test\n");
    append("test test test test test test test\n");
    append("test test test test test\n");
  }

  void append(const char* s)
  {
    buffer_ += s;
  }

  void draw(const char* title, bool* p_open = nullptr)
  {
    if(!ImGui::Begin(title, p_open))
    {
      ImGui::End();
      return;
    }

    ImGui::TextUnformatted(buffer_.data(), buffer_.data() + buffer_.size());

    if(auto_scroll_ and ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::End();
  }
};


void draw_log_window(bool* p_open)
{
  static log_window l;

  l.draw("Log", p_open);
}


std::tuple<const char*, SDL_Window*, SDL_GLContext> create_window()
{
  // Setup SDL
  // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
  // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
  // XXX since this initializes things unrelated to windowing, we might want to hoist this out of create_window
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0)
  {
    throw std::runtime_error(fmt::format("create_window: error after SDL_Init: {}", SDL_GetError()));
  }
  
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* window = SDL_CreateWindow("App Name", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1); // Enable vsync

  return {glsl_version, window, gl_context};
}


void destroy_window(SDL_Window* window, SDL_GLContext context)
{
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

SDL_AudioDeviceID create_audio()
{
  SDL_AudioSpec desired{};
  SDL_AudioSpec obtained{};

  //desired.freq = 44100;
  desired.freq = 88200;
  desired.format = AUDIO_F32;
  desired.channels = 1;
  desired.callback = nullptr;

  SDL_AudioDeviceID result = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
  if(result == 0)
  {
    throw std::runtime_error(fmt::format("create_audio: Couldn't open audio device: {}", SDL_GetError()));
  }

  if(obtained.format != desired.format)
  {
    throw std::runtime_error("create_audio: Obtained undesired audio format");
  }

  // unpause the audio
  SDL_PauseAudioDevice(result, 0);

  return result;
}

void destroy_audio(SDL_AudioDeviceID audio)
{
  SDL_CloseAudioDevice(audio);
}


float square_wave(float frequency, float t)
{
  return 2.f * (2.f * std::floor(t * frequency) - std::floor(2.f * t * frequency)) + 1.f;
}


float sawtooth_wave(float frequency, float t)
{
  return 2.f * (t * frequency - std::floor(0.5f + t * frequency));
}


float pulse_wave(float frequency, float duty_cycle, float t)
{
  return sawtooth_wave(frequency, t) - sawtooth_wave(frequency, t + duty_cycle/frequency);
}


int gui(nes::system& sys)
{
  auto [glsl_version, window, gl_context] = create_window();

  SDL_AudioDeviceID audio = create_audio();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  
  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  
  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);
  
  // Our state
  bool show_log_window = true;
  framebuffer_window framebuffer;
  palettes_window palettes;
  int selected_palette = 0;
  pattern_tables_window pattern_tables;
  disassembly_window disassembly{sys};
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  std::atomic<bool> emulation_cancelled = false;
  std::atomic<bool> emulation_paused = true;
  std::future<void> emulation = make_ready_future();

  // Main loop
  bool done = false;
  while(!done)
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if(event.type == SDL_QUIT)
      {
        done = true;
      }

      if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
      {
        done = true;
      }
    }

    update_controller_state(sys);
    
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // draw a start/pause button
    ImGui::Begin("Emulation");
    if(is_complete(emulation))
    {
      if(ImGui::Button("Emulate"))
      {
        emulation_paused = false;
        emulation_cancelled = false;
        emulation = std::async([&]
        {
          //emulate(sys, emulation_cancelled, emulation_paused, null_stream, std::cerr);
          //emulate(sys, emulation_cancelled, emulation_paused, std::cout, std::cerr);
          emulate(sys, emulation_cancelled, emulation_paused, std::cout, std::cerr, [audio](float sample)
          {
            if(SDL_QueueAudio(audio, &sample, sizeof(float)))
            {
              throw std::runtime_error(fmt::format("gui: Error after SDL_QueueAudio: {}", SDL_GetError()));
            }
            //// XXX it's not clear if buffering here is helpful
            //static std::vector<float> buffer;
            //if(buffer.capacity() == 0)
            //{
            //  buffer.resize(512);
            //}

            //buffer.push_back(sample);

            //if(buffer.size() == buffer.capacity())
            //{
            //  if(SDL_QueueAudio(audio, buffer.data(), sizeof(float) * buffer.size()))
            //  {
            //    throw std::runtime_error(fmt::format("gui: Error after SDL_QueueAudio: {}", SDL_GetError()));
            //  }

            //  buffer.clear();
            //}
          });
        });
      }
    }
    else
    {
      const char* text = emulation_paused ? "Continue" : "Pause";
      if(ImGui::Button(text))
      {
        emulation_paused = !emulation_paused;
        emulation_paused.notify_all();
      }
    }
    ImGui::End();

    // show a log window
    if(show_log_window)
    {
      draw_log_window(&show_log_window);
    }

    // draw the current framebuffer
    framebuffer.draw(sys);

    // draw the palettes
    if(std::optional new_palette = palettes.draw(sys))
    {
      selected_palette = *new_palette;
    }

    // draw the pattern tables
    pattern_tables.draw(sys, selected_palette);

    draw_nametable(sys, 0);
    draw_nametable(sys, 1);
    draw_object_attributes(sys);
    draw_zero_page(sys);
    disassembly.draw(sys);
    
    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  // end and join with the emulation thread
  emulation_paused = false;
  emulation_paused.notify_all();
  emulation_cancelled = true;
  emulation_cancelled.notify_all();
  if(emulation.valid())
  {
    emulation.wait();
  }

  destroy_audio(audio);
  
  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  
  destroy_window(window, gl_context);
  
  return 0;
}

