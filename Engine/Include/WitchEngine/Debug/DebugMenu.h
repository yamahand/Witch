#pragma once
#ifdef WITCH_DEBUG_UI
#include <deque>
#include <functional>
#include <string>
#include <string_view>

namespace witch::debug {

/// デバッグウィンドウが無い場所を右クリックしたときに出るグローバルコンテキストメニュー。
/// 項目名は "/" でネスト可能（例: "Debug/Toggle Collider" は "Debug" サブメニュー配下に
/// "Toggle Collider" を作る）。エンジン標準項目とゲーム側追加項目を同じリストで扱う。
/// スコープ付きの登録（コンストラクタで追加・デストラクタで削除）には DebugMenuItem を使う。
/// WITCH_DEBUG_UI 定義時のみ存在し、OFF ビルドではヘッダごとビルドから外れる。
class DebugMenu {
public:
    using Callback = std::function<void()>;

    /// 項目を登録する。path は "/" 区切りでネスト階層を表す。
    /// 登録順を各階層内の表示順として保持する（アルファベット順ソートはしない）。
    /// 空トークンを含む path（"Debug/" や "A//B" 等）は警告を出して登録せず無視する。
    /// 同一 path の重複登録は警告を出した上で後勝ち（旧項目は削除され、表示位置は
    /// 再登録側＝末尾に移る）。
    /// @return 登録できたら true。不正な path で無視した場合は false。
    bool AddItem(std::string path, Callback callback);

    /// path に一致する項目を削除する。実際の除去は次の Draw() 先頭まで遅延するため、
    /// メニュー項目のコールバック内から呼んでも安全。未登録の path は警告を出して無視。
    void RemoveItem(std::string_view path);

    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）で毎フレーム呼ぶ。
    /// デバッグウィンドウの外側で右クリックされたときだけメニューを開く。
    void Draw();

private:
    struct Item {
        std::string path;
        Callback callback;
        bool pendingRemove = false; ///< 削除予約。次の Draw 先頭で回収する。
    };
    // deque なのは push_back で既存要素への参照が無効化されないため。
    // Draw() 中の木は items_ の callback を指しており、メニュー項目のコールバック内から
    // AddItem() が呼ばれても（項目の動的追加）ぶら下がりポインタにならない。
    // 削除側も同じ理由で即時 erase せず pendingRemove -> Draw 先頭回収の遅延方式を取る。
    std::deque<Item> items_;
    bool hasPendingRemove_ = false;
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
