#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Platform/PlatformWindow.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"

namespace witch::platform {

namespace {

constexpr wchar_t kClassName[] = L"WitchWindowClass";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        Engine::Get().RequestExit();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace

void* CreateMainWindow(const WindowParams& params) {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    RECT rc = {0, 0, params.width, params.height};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    // Convert title from UTF-8 to UTF-16.
    int len = MultiByteToWideChar(CP_UTF8, 0, params.title, -1, nullptr, 0);
    std::wstring title(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, params.title, -1, title.data(), len);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        log::Error("CreateWindowExW failed.");
        return nullptr;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    log::Info("Window created ({}x{})", params.width, params.height);
    return hwnd;
}

bool PumpMessages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

} // namespace witch::platform
