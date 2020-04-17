#include "chip8/chip8.h"

#include <string>
#include <vector>


int main(int argc, char** argv) {
    // Turn args into a vector of strings for simplicity
    const std::vector<std::string> args(argv, argv + argc);

    chip8 chip;
    chip.loadROM(args[1]);

    while (true) {
        chip.runCycle();
    }

    return 0;
}