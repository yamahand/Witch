#pragma once
#ifdef WITCH_DEBUG_UI
#include <functional>
#include <string>
#include <vector>

namespace witch::debug {

/// デバッグウィンドウが無い場所を右クリックしたときに出るグローバルコンテキストメニュー。
/// 項目名は "/" でネスト可能（例: "Debug/Toggle Collider" は "Debug" サブメニュー配下に
/// "Toggle Collider" を作る）。エンジン標準項目とゲーム側追加項目を同じリストで扱う。
/// WITCH_DEBUG_UI 定義時のみ存在し、OFF ビルドではヘッダごとビルドから外れる。
class DebugMenu {
public:
    using Callback = std::function<void()>;

    /// 項目を登録する。path は "/" 区切りでネスト階層を表す。
    /// 登録順を各階層内の表示順として保持する（アルファベット順ソートはしない）。
    void AddItem(std::string path, Callback callback);

    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）で毎フレーム呼ぶ。
    /// デバッグウィンドウの外側で右クリックされたときだけメニューを開く。
    void Draw();

private:
    struct Item {
        std::string path;
        Callback callback;
    };
    std::vector<Item> items_;
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
