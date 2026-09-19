#include "windows_canonical_surface_host.hpp"

#if defined(_WIN32)

#include <windows.h>

#include <chrono>
#include <iostream>
#include <thread>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    canvas::windows_demo::WindowsCanonicalSurfaceHost host;
    std::string error;
    if (!host.initialize(800U, 600U, &error)) {
        std::cerr << error << '\n';
        return 1;
    }
    for (int frame = 0; frame < 3; ++frame) {
        if (!host.pumpOnce(&error)) return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return 0;
}

#else
int main() { return 2; }
#endif
