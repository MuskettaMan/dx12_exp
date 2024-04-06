#include "precomp.hpp"
#include <app.hpp>

std::unique_ptr<App> app;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int32_t showCommand)
{
    app = std::make_unique<App>(hInstance, showCommand);
}

