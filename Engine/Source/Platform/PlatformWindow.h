#pragma once

namespace witch::platform {

/// ウィンドウ生成パラメータ。
struct WindowParams {
    int width;
    int height;
    const char* title;
};

/// メインウィンドウを生成して表示する。
/// @return Win32 では HWND を void* にキャストして返す。
void* CreateMainWindow(const WindowParams& params);

/// 保留中の OS メッセージをすべて処理する。
/// @return 終了メッセージ（WM_QUIT 等）を受けたら false を返す。
bool PumpMessages();

} // namespace witch::platform
