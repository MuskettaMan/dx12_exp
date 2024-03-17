#pragma once

#include <memory>
#include <engine.hpp>
#include <windows.h>
#include <game_timer.hpp>

class Device;

constexpr uint32_t INITIAL_WIDTH = 1920;
constexpr uint32_t INITIAL_HEIGHT = 1080;

class App
{
public:
	App(HINSTANCE hInstance, int32_t showCommand);
	~App();

private:
	int Run();
	bool InitWindowsApp(HINSTANCE hInstance, int32_t show);

	virtual void OnMouseDown(WPARAM buttonState, int x, int y) {}
	virtual void OnMouseUp(WPARAM buttonState, int x, int y) {}
	virtual void OnMouseMove(WPARAM buttonState, int x, int y) {}

	static LRESULT WndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam);

	HWND _mainWnd = nullptr;
	std::unique_ptr<Engine> _engine;
	std::shared_ptr<Device> _device;
	GameTimer _timer;
	bool _paused;
	bool _minimized;
	bool _maximized;
	bool _resizing;
};