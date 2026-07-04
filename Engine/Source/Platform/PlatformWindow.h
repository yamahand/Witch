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

/// エラーダイアログを表示する（起動失敗などユーザーに必ず伝えるべき致命的エラー用）。
/// ログと違い、コンソールやデバッガが無いプレイヤー環境でも見える。
/// @param title/message UTF-8
void ShowErrorDialog(const char* title, const char* message);

} // namespace witch::platform
