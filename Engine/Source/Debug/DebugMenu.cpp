#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/DebugMenu.h"
#include "WitchEngine/Core/Logger.h"

#include <imgui.h>
#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace witch::debug {

namespace {

/// メニュー階層を表す木のノード。登録順に依存せず、パスの各トークンで
/// 親子関係を確定させてからまとめて描画する。
/// 1 ノードは「サブメニュー（children を持つ）」と「実行可能項目（callback を持つ）」を
/// 兼ねられる。両方を持つ場合は callback を先頭の MenuItem として、その下に children を出す。
struct MenuNode {
    std::string label;                              ///< この階層でのラベル（トークン）。
    const DebugMenu::Callback* callback = nullptr;   ///< 葉として実行可能なら非 null。
    std::vector<std::unique_ptr<MenuNode>> children; ///< 子ノード（登録順を保持）。

    /// label に一致する子を返す。無ければ末尾に作って返す（登録順マージ）。
    MenuNode* ChildFor(std::string_view token) {
        for (const auto& child : children) {
            if (child->label == token) return child.get();
        }
        children.push_back(std::make_unique<MenuNode>());
        children.back()->label = std::string(token);
        return children.back().get();
    }
};

/// path を "/" で分割したトークン列を返す。分割規則はここに一元化する
/// （空トークン判定と木構築で規則がズレないようにするため）。
/// "Debug/Toggle Collider" -> {"Debug", "Toggle Collider"}。空文字列 -> {""}。
std::vector<std::string_view> SplitTokens(std::string_view path) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const size_t end = (slash == std::string_view::npos) ? path.size() : slash;
        tokens.push_back(path.substr(start, end - start));
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return tokens;
}

/// path のトークン列をたどって木に挿入し、葉に callback を設定する。
void InsertPath(MenuNode& root, std::string_view path, const DebugMenu::Callback* callback) {
    MenuNode* node = &root;
    for (const std::string_view token : SplitTokens(path)) {
        node = node->ChildFor(token);
    }
    node->callback = callback; // 葉に到達。
}

/// root の子を ImGui のメニュー項目として描く。children を持つノードはサブメニュー、
/// callback だけを持つノードは実行項目。両方持つ場合は MenuItem + サブメニューの両方を出す。
void DrawChildren(const MenuNode& node) {
    for (const auto& child : node.children) {
        // ID はラベル文字列から生成させる（フレーム間で安定。ポインタ由来だと
        // 木を毎フレーム作り直す都合で ID が変わり、サブメニューのホバー展開が壊れる）。
        // 兄弟間のラベル一意性は ChildFor のマージが保証する。葉（MenuItem）と
        // サブメニュー（BeginMenu）が同名ラベルを共有する場合の ID 衝突は、
        // サブメニュー側に "##menu" を付けて回避する（"##" 以降は表示されない）。
        if (child->callback) {
            if (ImGui::MenuItem(child->label.c_str())) {
                (*child->callback)();
            }
        }
        if (!child->children.empty()) {
            if (ImGui::BeginMenu((child->label + "##menu").c_str())) {
                DrawChildren(*child);
                ImGui::EndMenu();
            }
        }
    }
}

} // namespace

bool DebugMenu::AddItem(std::string path, Callback callback) {
    // 空トークン（"Debug/" や "A//B" 等）はタイプミスの可能性が高く、空白の
    // メニュー項目が出てしまうため、警告して登録せずに無視する。
    const auto tokens = SplitTokens(path);
    if (std::ranges::any_of(tokens, [](std::string_view t) { return t.empty(); })) {
        log::Warn("DebugMenu::AddItem: path \"{}\" contains an empty token. Item ignored.",
                  path);
        return false;
    }
    // 同一 path の重複登録は後勝ち。旧項目の callback をその場で書き換えず、
    // 削除予約して新項目を積む（旧 callback の実行中に AddItem されても、実行中の
    // std::function を破壊しないため）。表示位置は再登録側（末尾）に移る。
    for (auto& item : items_) {
        if (!item.pendingRemove && item.path == path) {
            log::Warn("DebugMenu::AddItem: duplicate path \"{}\". "
                      "The existing callback will be overridden.",
                      path);
            item.pendingRemove = true;
            hasPendingRemove_ = true;
            break;
        }
    }
    items_.push_back(Item{std::move(path), std::move(callback)});
    return true;
}

void DebugMenu::RemoveItem(std::string_view path) {
    for (auto& item : items_) {
        if (!item.pendingRemove && item.path == path) {
            // 即時 erase せず削除予約に留める（メニューコールバック中に呼ばれても
            // 描画中の木や実行中の callback を無効化しないため。Scene の遅延破棄と同じ発想）。
            item.pendingRemove = true;
            hasPendingRemove_ = true;
            return;
        }
    }
    log::Warn("DebugMenu::RemoveItem: path \"{}\" is not registered.", path);
}

void DebugMenu::Draw() {
    // 削除予約の回収はコールバックが走っていない Draw 先頭で行う。
    if (hasPendingRemove_) {
        std::erase_if(items_, [](const Item& item) { return item.pendingRemove; });
        hasPendingRemove_ = false;
    }

    if (!ImGui::BeginPopupContextVoid("DebugMenuPopup", ImGuiPopupFlags_MouseButtonRight)) {
        return;
    }

    // 毎フレーム木を作り直す（項目数は少数想定のためキャッシュしない）。
    // 登録順に依存せず、同じ親を持つ項目が非連続に登録されても 1 つのサブメニューに
    // マージされる。
    MenuNode root;
    for (const auto& item : items_) {
        InsertPath(root, item.path, &item.callback);
    }
    DrawChildren(root);

    ImGui::EndPopup();
}

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
