#include <device.hpp>

#include <cstdint>
#include <windows.h>
#include <util.hpp>
#include <cassert>

HWND hMainWnd = nullptr;
uint32_t clientWidth = 600;
uint32_t clientHeight = 400;

bool InitWindowsApp(HINSTANCE hInstance, int32_t show);
int Run();

LRESULT CALLBACK WndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int32_t showCommand)
{
    if(!InitWindowsApp(hInstance, showCommand))
        return 0;

    Device device{ hMainWnd, clientWidth, clientHeight };

    return Run();
}

bool InitWindowsApp(HINSTANCE hInstance, int32_t show)
{
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"BasicWndClass";

    if(!RegisterClass(&wc))
    {
        MessageBox(0, L"RegisterClass FAILED", 0, 0);
        return false;
    }

    RECT r{ 0, 0, static_cast<LONG>(clientWidth), static_cast<LONG>(clientHeight) };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);
    int width = r.right - r.left;
    int height = r.bottom - r.top;

    hMainWnd = CreateWindow(
        L"BasicWndClass",
        L"Win32Basic", 
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, 
        CW_USEDEFAULT,
        width, 
        height, 
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if(!hMainWnd)
    {
        MessageBox(0, L"CreateWindow FAILED", 0, 0);
        return false;
    }

    ShowWindow(hMainWnd, show);
    UpdateWindow(hMainWnd);

    return true;
}

int Run()
{
    MSG msg{ 0 };

    while(msg.message != WM_QUIT)
    {
        if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {

        }
    }

    return static_cast<int32_t>(msg.wParam);
}

LRESULT WndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
    case WM_LBUTTONDOWN:
        MessageBox(0, L"Hello world", L"Hello", MB_OK);
        return 0;

    case WM_KEYDOWN:
        if(wParam == VK_ESCAPE)
            DestroyWindow(hMainWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

