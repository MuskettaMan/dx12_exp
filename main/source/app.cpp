#include "app.hpp"
#include "device.hpp"
#include <util.hpp>
#include <WindowsX.h>

App::App(HINSTANCE hInstance, int32_t showCommand)
{
#if defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        if (!InitWindowsApp(hInstance, showCommand))
            return;

        _device = std::make_unique<Device>(_mainWnd, INITIAL_WIDTH, INITIAL_HEIGHT);

        Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return;
    }
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

    RECT r{ 0, 0, static_cast<LONG>(INITIAL_WIDTH), static_cast<LONG>(INITIAL_HEIGHT) };
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
    _timer.Reset();

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
            _timer.Tick();

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
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            app->_timer.Stop();
            app->_paused = true;
        }
        else
        {
            app->_timer.Start();
            app->_paused = false;
        }
        return 0;
    case WM_SIZE:
        if (app->_device)
        {
            app->_device->_clientWidth = LOWORD(lParam);
            app->_device->_clientHeight = HIWORD(lParam);

            if (wParam == SIZE_MINIMIZED)
            {
                app->_paused = true;
                app->_minimized = true;
                app->_maximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                app->_paused = false;
                app->_minimized = false;
                app->_maximized = true;
                app->_device->OnResize();
            }
            else if (wParam == SIZE_RESTORED)
            {
                if (app->_minimized)
                {
                    app->_paused = false;
                    app->_minimized = false;
                    app->_device->OnResize();
                }
                else if (app->_maximized)
                {
                    app->_paused = false;
                    app->_maximized = false;
                    app->_device->OnResize();
                }
                else if (app->_resizing)
                {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                }
                else
                {
                    app->_device->OnResize();
                }
            }
        }
        return 0;
    case WM_ENTERSIZEMOVE:
        app->_paused = true;
        app->_resizing = true;
        app->_timer.Stop();
        return 0;
    case WM_EXITSIZEMOVE:
        app->_paused = false;
        app->_resizing = false;
        app->_timer.Start();
        app->_device->OnResize();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    // The WM_MENUCHAR message is sent when a menu is active and the user presses 
    // a key that does not correspond to any mnemonic or accelerator key. 
    case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

    // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO:
        (reinterpret_cast<MINMAXINFO*>(lParam))->ptMinTrackSize.x = 200;
        (reinterpret_cast<MINMAXINFO*>(lParam))->ptMinTrackSize.y = 200;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        app->OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        app->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        app->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_KEYUP:
        if (wParam == VK_ESCAPE)
            PostQuitMessage(0);

        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

