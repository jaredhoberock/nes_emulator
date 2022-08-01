// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include <future>
#include "gui.hpp"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "simulate.hpp"
#include <atomic>
#include <chrono>
#include <fmt/format.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <tuple>
#include <SDL.h>
#include <SDL_opengl.h>


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
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
  {
    throw std::runtime_error(fmt::format("Error: {}", SDL_GetError()));
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


int gui(class system& sys)
{
  auto [glsl_version, window, gl_context] = create_window();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  
  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  
  // Setup Platform/Renderer backends
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);
  
  // Our state
  bool show_log_window = true;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  std::atomic<bool> simulation_cancelled = false;
  std::atomic<bool> simulation_paused = true;
  std::future<void> simulation = make_ready_future();

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
    
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // draw a start/pause button
    ImGui::Begin("Simulation");
    if(is_complete(simulation))
    {
      if(ImGui::Button("Simulate"))
      {
        simulation_paused = false;
        simulation_cancelled = false;
        simulation = std::async([&]
        {
          simulate(sys, simulation_cancelled, simulation_paused, std::cout, std::cerr);
        });
      }
    }
    else
    {
      const char* text = simulation_paused ? "Unpause simulation" : "Pause simulation";
      if(ImGui::Button(text))
      {
        simulation_paused = !simulation_paused;
        simulation_paused.notify_all();
      }
    }
    ImGui::End();

    // show a log window
    if(show_log_window)
    {
      draw_log_window(&show_log_window);
    }
    
    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  // end and join with the simulation thread
  simulation_paused = false;
  simulation_paused.notify_all();
  simulation_cancelled = true;
  simulation_cancelled.notify_all();
  if(simulation.valid())
  {
    simulation.wait();
  }
  
  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  
  destroy_window(window, gl_context);
  
  return 0;
}

