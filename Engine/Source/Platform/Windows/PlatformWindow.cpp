#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Platform/PlatformWindow.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Rhi/IRenderer.h"
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#include <imgui_impl_win32.h>

// imgui_impl_win32.h は <windows.h> 依存を避けるため、この前方宣言を #if 0 で握り潰している。
// ヘッダの指示どおり、利用側で 1 行コピーして宣言する。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace witch::platform {

namespace {

constexpr wchar_t kClassName[] = L"WitchWindowClass";

/// アクティブな入力サービスを取得する（未生成なら nullptr）。
/// IInput* をそのまま返し、受け口は IInput 越しに呼ぶ（具象へのダウンキャストをしない）。
/// renderer の OnResize 転送と同じく Services 経由で参照し、新しいグローバルを増やさない。
IInput* ActiveInput() {
    return Services::Instance().input;
}

#ifdef WITCH_DEBUG_UI
// ImGui コンテキストが生成済みか。ウィンドウは renderer->Init（＝ImGui::CreateContext）より
// 先に作られ、CreateWindowExW/ShowWindow/UpdateWindow が同期的に WndProc へメッセージを
// 配送する。その間 ImGui は未初期化なので、コンテキスト有無を先に確認してから GetIO() に
// 触れないと GImGui==nullptr で assert する。GetCurrentContext は非 assert のゲッター。
bool ImGuiReady() { return ImGui::GetCurrentContext() != nullptr; }

// ImGui がキーボード／マウスをキャプチャしているか。キャプチャ中はゲーム入力を無効化し、
// 入力を ImGui に委ねる（テキスト入力欄にフォーカス中など）。
// 未初期化時は false（＝ゲーム入力を通す）へ安全に倒す。
bool ImGuiWantsKeyboard() { return ImGuiReady() && ImGui::GetIO().WantCaptureKeyboard; }
bool ImGuiWantsMouse()    { return ImGuiReady() && ImGui::GetIO().WantCaptureMouse; }
#else
constexpr bool ImGuiWantsKeyboard() { return false; }
constexpr bool ImGuiWantsMouse()    { return false; }
#endif

/// VK_* → 抽象キー Key。未対応コードは Key::Count を返し、呼び出し側が無視する。
/// プラットフォーム固有の VK 変換はこの Windows TU に閉じ込め、IInput へは Key で渡す。
Key VkToKey(unsigned int vk) {
    // A–Z / 0–9 は ASCII と一致する（'A'..'Z', '0'..'9'）。
    if (vk >= 'A' && vk <= 'Z')
        return static_cast<Key>(static_cast<int>(Key::A) + (vk - 'A'));
    if (vk >= '0' && vk <= '9')
        return static_cast<Key>(static_cast<int>(Key::Num0) + (vk - '0'));

    switch (vk) {
    case VK_LEFT:     return Key::Left;
    case VK_RIGHT:    return Key::Right;
    case VK_UP:       return Key::Up;
    case VK_DOWN:     return Key::Down;
    case VK_SPACE:    return Key::Space;
    case VK_RETURN:   return Key::Enter;
    case VK_ESCAPE:   return Key::Escape;
    case VK_TAB:      return Key::Tab;
    case VK_BACK:     return Key::Backspace;
    case VK_SHIFT:    return Key::LeftShift;
    case VK_LSHIFT:   return Key::LeftShift;
    case VK_CONTROL:  return Key::LeftControl;
    case VK_LCONTROL: return Key::LeftControl;
    case VK_MENU:     return Key::LeftAlt;
    case VK_LMENU:    return Key::LeftAlt;
    // VK_RSHIFT / VK_RCONTROL / VK_RMENU は Key enum に右側キーが未定義のため
    // ここで無視される（Key::Count に落ちて Win32Input 側で破棄）。
    // 将来 RightShift 等を Key に追加したら、この switch にも対応 case を足すこと。
    default:          return Key::Count;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
#ifdef WITCH_DEBUG_UI
    // ImGui がキャプチャを取り始めた瞬間（false→true）に、ゲーム側の押下状態を一括クリアする。
    // これがないと、キーを押下したまま ImGui がキャプチャを取り、その状態で WM_KEYUP が
    // 下の ImGui ハンドラに飲まれると OnKeyChange(false) が来ず IsDown が stuck する。
    {
        static bool prevWantCapture = false;
        const bool nowWantCapture = ImGuiWantsKeyboard() || ImGuiWantsMouse();
        if (nowWantCapture && !prevWantCapture) {
            if (auto* input = ActiveInput()) input->ClearAll();
        }
        prevWantCapture = nowWantCapture;
    }

    // コンテキスト未生成（起動時のウィンドウ生成〜renderer->Init の間）は ImGui へ渡さない。
    if (ImGuiReady() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
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
        if (auto* input = ActiveInput(); input && !ImGuiWantsKeyboard())
            input->OnKeyChange(VkToKey(static_cast<unsigned int>(wp)), true);
        // SYSKEY は DefWindowProc に渡してメニュー連携等を壊さない。
        if (msg == WM_SYSKEYDOWN)
            return DefWindowProcW(hwnd, msg, wp, lp);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (auto* input = ActiveInput(); input && !ImGuiWantsKeyboard())
            input->OnKeyChange(VkToKey(static_cast<unsigned int>(wp)), false);
        if (msg == WM_SYSKEYUP)
            return DefWindowProcW(hwnd, msg, wp, lp);
        return 0;

    // ── Mouse buttons ────────────────────────────────────────────────────
    // down で SetCapture、up で全ボタンが離れたら ReleaseCapture。これにより
    // ウィンドウ外でボタンを離しても WM_*BUTTONUP がこのウィンドウに届き、
    // ボタンが押下のまま stuck するのを防ぐ。wp の MK_* で残ボタンを判定する。
    // サイドボタン（XBUTTON1/2）は Key 未定義だが、押下中の早期解放を防ぐため
    // 残ボタン判定には含める（将来 Mouse4/5 を Key に追加する際も整合する）。
    //
    // ImGui がマウスをキャプチャ中はゲーム受け口（OnKeyChange）をスキップし入力を
    // ImGui に委ねる。SetCapture/ReleaseCapture は Win32 の capture 一貫性のため常に行う。
    case WM_LBUTTONDOWN:
        if (auto* input = ActiveInput(); input && !ImGuiWantsMouse()) input->OnKeyChange(Key::MouseLeft, true);
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        if (auto* input = ActiveInput(); input && !ImGuiWantsMouse()) input->OnKeyChange(Key::MouseLeft, false);
        if (!(wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2))) ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        if (auto* input = ActiveInput(); input && !ImGuiWantsMouse()) input->OnKeyChange(Key::MouseRight, true);
        SetCapture(hwnd);
        return 0;
    case WM_RBUTTONUP:
        if (auto* input = ActiveInput(); input && !ImGuiWantsMouse()) input->OnKeyChange(Key::MouseRight, false);
        if (!(wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2))) ReleaseCapture();
        return 0;
    case WM_MBUTTONDOWN:
        if (auto* input = ActiveInput(); input && !ImGuiWantsMouse()) input->OnKeyChange(Key::MouseMiddle, true);
        SetCapture(hwnd);
        return 0;
    case WM_MBUTTONUP:
        if (auto* input = ActiveInput(); input && !ImGuiWantsMouse()) input->OnKeyChange(Key::MouseMiddle, false);
        if (!(wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2))) ReleaseCapture();
        return 0;

    // ── Mouse move / wheel ───────────────────────────────────────────────
    // カーソル移動は常に反映（座標は ImGui キャプチャ中も追従させたい）。
    // ホイールはキャプチャ中スキップして ImGui のスクロールに委ねる。
    case WM_MOUSEMOVE:
        if (auto* input = ActiveInput()) {
            // LOWORD/HIWORD は符号なし。負座標（マルチモニタ等）に備え short で受ける。
            input->OnMouseMove(static_cast<float>(static_cast<short>(LOWORD(lp))),
                               static_cast<float>(static_cast<short>(HIWORD(lp))));
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (auto* input = ActiveInput(); input && !ImGuiWantsMouse()) {
            int raw = GET_WHEEL_DELTA_WPARAM(wp);
            input->OnMouseWheel(static_cast<float>(raw) / static_cast<float>(WHEEL_DELTA));
        }
        return 0;

    // ── Focus ────────────────────────────────────────────────────────────
    // フォーカス喪失（Alt+Tab・最小化等）。OS は押下中キーの WM_KEYUP を送らないため、
    // ここで全状態をクリアしてキーが押されっぱなしになるのを防ぐ。
    case WM_KILLFOCUS:
        if (auto* input = ActiveInput()) input->ClearAll();
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_DESTROY:
        Engine::Get().RequestExit();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

/// UTF-8 → UTF-16。ウィンドウタイトルやダイアログ文字列を Win32 API に渡す用。
std::wstring Utf8ToWide(const char* utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), len);
    return wide;
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

    std::wstring title = Utf8ToWide(params.title);

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

void ShowErrorDialog(const char* title, const char* message) {
    MessageBoxW(nullptr, Utf8ToWide(message).c_str(), Utf8ToWide(title).c_str(),
                MB_OK | MB_ICONERROR);
}

} // namespace witch::platform
