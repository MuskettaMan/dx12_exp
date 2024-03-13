#include "app.hpp"
#include "device.hpp"

App::App(HINSTANCE hInstance, int32_t showCommand)
{
    if (!InitWindowsApp(hInstance, showCommand))
        return;

    _device = std::make_unique<Device>(_mainWnd, _clientWidth, _clientHeight);

    Run();
}

App::~App() = default;

bool App::InitWindowsApp(HINSTANCE hInstance, int32_t show)
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

    if (!RegisterClass(&wc))
    {
        MessageBox(0, L"RegisterClass FAILED", 0, 0);
        return false;
    }

    RECT r{ 0, 0, static_cast<LONG>(_clientWidth), static_cast<LONG>(_clientHeight) };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);
    int width = r.right - r.left;
    int height = r.bottom - r.top;

    _mainWnd = CreateWindow(
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

    SetWindowLongPtr(_mainWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if (!_mainWnd)
    {
        MessageBox(0, L"CreateWindow FAILED", 0, 0);
        return false;
    }

    ShowWindow(_mainWnd, show);
    UpdateWindow(_mainWnd);

    return true;
}

int App::Run()
{
    timer.Reset();

    MSG msg{ 0 };

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            timer.Tick();

            _device->Draw();
        }
    }

    return static_cast<int32_t>(msg.wParam);
}

LRESULT App::WndProc(HWND hWnd, uint32_t msg, WPARAM wParam, LPARAM lParam)
{
    App* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (app == nullptr)
        return DefWindowProc(hWnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            DestroyWindow(app->_mainWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

