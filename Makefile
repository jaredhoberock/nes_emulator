nes: cartridge.hpp main.cpp
	clang -std=c++20 -Wall -Wextra main.cpp -lstdc++ -lfmt -o nes

clean:
	rm -rf *.o nes
