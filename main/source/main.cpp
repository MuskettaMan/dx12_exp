#include <app.hpp>

#include <cstdint>
#include <windows.h>
#include <util.hpp>
#include <cassert>
#include <sstream>

std::unique_ptr<App> app;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int32_t showCommand)
{
    app = std::make_unique<App>(hInstance, showCommand);
}

