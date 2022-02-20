#include "emulator/chip8_emulator.h"
#include <string>
#include <vector>

int main(int argc, char** argv) {
    // Turn args into a vector of strings for simplicity
    //const auto args = std::vector<std::string>(argv, argv + argc);

    auto emulator = Chip8Emulator{};

    // if (!emulator.load_rom("roms/demos/Particle Demo [zeroZshadow, 2008].ch8")) {
    //     return 1;
    // }

    emulator.run();

    return 0;
}
