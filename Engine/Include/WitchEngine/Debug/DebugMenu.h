#pragma once
#ifdef WITCH_DEBUG_UI
#include <cstdint>
#include <deque>
#include <functional>
#include <string>

namespace witch::debug {

/// デバッグウィンドウが無い場所を右クリックしたときに出るグローバルコンテキストメニュー。
/// 項目名は "/" でネスト可能（例: "Debug/Toggle Collider" は "Debug" サブメニュー配下に
/// "Toggle Collider" を作る）。エンジン標準項目とゲーム側追加項目を同じリストで扱う。
/// スコープ付きの登録（コンストラクタで追加・デストラクタで削除）には DebugMenuItem を使う。
/// WITCH_DEBUG_UI 定義時のみ存在し、OFF ビルドではヘッダごとビルドから外れる。
class DebugMenu {
public:
    using Callback = std::function<void()>;

    /// AddItem が返す項目ハンドル。RemoveItem での削除対象の特定に使う。
    /// path 文字列ではなく id で特定するのは、同一 path の上書き登録後に古いハンドルの
    /// 削除が新しい登録を誤って消さないようにするため。
    using ItemId = std::uint64_t;
    static constexpr ItemId kInvalidItemId = 0;

    /// 項目を登録する。path は "/" 区切りでネスト階層を表す。
    /// 登録順を各階層内の表示順として保持する（アルファベット順ソートはしない）。
    /// 空トークンを含む path（"Debug/" や "A//B" 等）と、ImGui が ID 区切りとして
    /// 解釈する "##" を含む path は、警告を出して登録せず無視する。
    /// 同一 path の重複登録は警告を出した上で後勝ち（旧項目は削除され旧 id は無効になる。
    /// 表示位置は再登録側＝末尾に移る）。
    /// @return 項目の一意な id。不正な path で無視した場合は kInvalidItemId。
    ItemId AddItem(std::string path, Callback callback);

    /// id に一致する項目を削除する。実際の除去は次の Draw() 先頭まで遅延するため、
    /// メニュー項目のコールバック内から呼んでも安全。未登録の id は警告を出して無視。
    void RemoveItem(ItemId id);

    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）で毎フレーム呼ぶ。
    /// デバッグウィンドウの外側で右クリックされたときだけメニューを開く。
    void Draw();

private:
    struct Item {
        std::string path;
        Callback callback;
        ItemId id = kInvalidItemId;
        bool pendingRemove = false; ///< 削除予約。次の Draw 先頭で回収する。
    };
    // deque なのは push_back で既存要素への参照が無効化されないため。
    // Draw() 中の木は items_ の callback を指しており、メニュー項目のコールバック内から
    // AddItem() が呼ばれても（項目の動的追加）ぶら下がりポインタにならない。
    // 削除側も同じ理由で即時 erase せず pendingRemove -> Draw 先頭回収の遅延方式を取る。
    std::deque<Item> items_;
    ItemId nextId_ = 1; ///< 単調増加。kInvalidItemId(0) は使わない。
    bool hasPendingRemove_ = false;
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
