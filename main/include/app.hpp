#pragma once

#include <memory>
#include <windows.h>
#include <game_timer.hpp>

class Device;

class App
{
public:
	App(HINSTANCE hInstance, int32_t showCommand);
	~App();

private:
	int Run();
	bool InitWindowsApp(HINSTANCE hInstance, int32_t show);

	static LRESULT WndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam);

	HWND _mainWnd = nullptr;
	std::unique_ptr<Device> _device;
	uint32_t _clientWidth = 600;
	uint32_t _clientHeight = 400;
	GameTimer timer;
};