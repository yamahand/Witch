#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Platform/PlatformWindow.h"
#include "Platform/Windows/Win32Input.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"
#ifdef WITCH_DEBUG_UI
#include <imgui_impl_win32.h>

// imgui_impl_win32.h は <windows.h> 依存を避けるため、この前方宣言を #if 0 で握り潰している。
// ヘッダの指示どおり、利用側で 1 行コピーして宣言する。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace witch::platform {

namespace {

constexpr wchar_t kClassName[] = L"WitchWindowClass";

/// アクティブな入力サービスを具象型で取得する。未生成なら nullptr。
/// renderer の OnResize 転送と同じく Services 経由で参照し、新しいグローバルを増やさない。
Win32Input* ActiveInput() {
    return static_cast<Win32Input*>(Services::Instance().input);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
#ifdef WITCH_DEBUG_UI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return TRUE;
#endif

    switch (msg) {
    case WM_SIZE:
        {
            int w = static_cast<int>(LOWORD(lp));
            int h = static_cast<int>(HIWORD(lp));
            auto* renderer = Services::Instance().renderer;
            if (renderer && w > 0 && h > 0)
                renderer->OnResize(w, h);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);

    // ── Keyboard ─────────────────────────────────────────────────────────
    // WM_SYSKEYDOWN/UP も拾い、Alt 等のシステムキーを入力に反映する。
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (auto* input = ActiveInput())
            input->OnKeyDown(static_cast<unsigned int>(wp));
        // SYSKEY は DefWindowProc に渡してメニュー連携等を壊さない。
        if (msg == WM_SYSKEYDOWN)
            return DefWindowProcW(hwnd, msg, wp, lp);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (auto* input = ActiveInput())
            input->OnKeyUp(static_cast<unsigned int>(wp));
        if (msg == WM_SYSKEYUP)
            return DefWindowProcW(hwnd, msg, wp, lp);
        return 0;

    // ── Mouse buttons ────────────────────────────────────────────────────
    case WM_LBUTTONDOWN:
        if (auto* input = ActiveInput()) input->OnMouseButton(Key::MouseLeft, true);
        return 0;
    case WM_LBUTTONUP:
        if (auto* input = ActiveInput()) input->OnMouseButton(Key::MouseLeft, false);
        return 0;
    case WM_RBUTTONDOWN:
        if (auto* input = ActiveInput()) input->OnMouseButton(Key::MouseRight, true);
        return 0;
    case WM_RBUTTONUP:
        if (auto* input = ActiveInput()) input->OnMouseButton(Key::MouseRight, false);
        return 0;
    case WM_MBUTTONDOWN:
        if (auto* input = ActiveInput()) input->OnMouseButton(Key::MouseMiddle, true);
        return 0;
    case WM_MBUTTONUP:
        if (auto* input = ActiveInput()) input->OnMouseButton(Key::MouseMiddle, false);
        return 0;

    // ── Mouse move / wheel ───────────────────────────────────────────────
    case WM_MOUSEMOVE:
        if (auto* input = ActiveInput()) {
            // LOWORD/HIWORD は符号なし。負座標（マルチモニタ等）に備え short で受ける。
            input->OnMouseMove(static_cast<float>(static_cast<short>(LOWORD(lp))),
                               static_cast<float>(static_cast<short>(HIWORD(lp))));
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (auto* input = ActiveInput()) {
            int raw = GET_WHEEL_DELTA_WPARAM(wp);
            input->OnMouseWheel(static_cast<float>(raw) / static_cast<float>(WHEEL_DELTA));
        }
        return 0;

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
