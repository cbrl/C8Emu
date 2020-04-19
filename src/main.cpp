#include "emulator/chip8_emulator.h"
#include <string>
#include <vector>

int main(int argc, char** argv) {
    // Turn args into a vector of strings for simplicity
    const std::vector<std::string> args(argv, argv + argc);

    Chip8Emulator emulator;

    if (!emulator.load_rom("roms/demos/Particle Demo [zeroZshadow, 2008].ch8")) {
        return 1;
    }

    emulator.run();

    return 0;
}

// Windows likes to be special
#ifdef _WIN32
#include <Windows.h>
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
#endif