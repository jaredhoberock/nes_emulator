headless: nes/bus.hpp nes/cartridge.hpp nes/cpu.hpp nes/ppu.hpp nes/ppu_renderer.hpp nes/ppu_renderer.cpp nes/system.hpp main.cpp 
	clang -std=c++20 -Wall -Wextra -g headless.cpp nes/ppu_renderer.cpp -lstdc++ -lfmt -o $@

nestest: *.hpp *.cpp Makefile
	clang -std=c++20 -Wall -Wextra -g nestest.cpp -lstdc++ -lfmt -o $@

IMGUI_DIR = ../imgui
IMGUI_SOURCES = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/backends/imgui_impl_sdl.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
IMGUI_OBJS = $(addsuffix .o, $(basename $(notdir $(IMGUI_SOURCES))))
IMGUI_CFLAGS = -I../imgui -I../imgui/backends `sdl2-config --cflags`
IMGUI_LIBS = `sdl2-config --libs` -lGL -ldl -lm

%.o:$(IMGUI_DIR)/%.cpp
	clang -std=c++20 -Wall -Wextra $(IMGUI_CFLAGS) -O3 -c -o $@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	clang -std=c++20 -Wall -Wextra $(IMGUI_CFLAGS) -O3 -c -o $@ $<

%.o:%.cpp *.hpp
	clang -std=c++20 -Wall -Wextra -O3 -c -o $@ $<

gui.o:gui.cpp gui.hpp nes/*.hpp
	clang -std=c++20 -Wall -Wextra $(IMGUI_CFLAGS) -O3 -c -o $@ $<

app:app.o gui.o nes/ppu_renderer.o $(IMGUI_OBJS)
	clang -o $@ $^ $(IMGUI_LIBS) -lstdc++ -lfmt -lpthread

clean:
	rm -rf *.o app headless nestest
