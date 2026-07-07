#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/DebugMenu.h"

#include <imgui.h>
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

/// path を "/" で分割しながら木にたどり、葉に callback を設定する。
/// "Debug/Toggle Collider" -> root -> "Debug" -> "Toggle Collider"(callback)。
void InsertPath(MenuNode& root, std::string_view path, const DebugMenu::Callback* callback) {
    MenuNode* node = &root;
    size_t start = 0;
    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const size_t end = (slash == std::string_view::npos) ? path.size() : slash;
        node = node->ChildFor(path.substr(start, end - start));
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    node->callback = callback; // 葉に到達。同一パス重複時は後勝ち。
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

void DebugMenu::AddItem(std::string path, Callback callback) {
    items_.push_back(Item{std::move(path), std::move(callback)});
}

void DebugMenu::Draw() {
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
