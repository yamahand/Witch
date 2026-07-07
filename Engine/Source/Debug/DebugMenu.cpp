#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/DebugMenu.h"

#include <imgui.h>
#include <string_view>
#include <utility>

namespace witch::debug {

namespace {

/// path をトークン列に分解する。"Debug/Toggle Collider" -> {"Debug", "Toggle Collider"}。
std::vector<std::string_view> SplitPath(std::string_view path) {
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

/// items をトークン列にした上で、共通の親トークンをネストした BeginMenu として描画し、
/// 葉トークンだけ MenuItem として表示する。呼ばれるたびに木を作り直す
/// （項目数は少数想定のため、シンプルさを優先しキャッシュしない）。
void DrawLevel(const std::vector<std::pair<std::vector<std::string_view>, const DebugMenu::Callback*>>& entries,
               size_t depth) {
    size_t i = 0;
    while (i < entries.size()) {
        const auto& tokens = entries[i].first;
        const std::string_view label = tokens[depth];

        if (tokens.size() == depth + 1) {
            // 葉ノード: 実行可能な項目。
            if (ImGui::MenuItem(std::string(label).c_str())) {
                (*entries[i].second)();
            }
            ++i;
            continue;
        }

        // 同じ親トークンを持つ連続区間をまとめてサブメニューにする。
        size_t j = i + 1;
        while (j < entries.size() && entries[j].first.size() > depth &&
               entries[j].first[depth] == label) {
            ++j;
        }

        if (ImGui::BeginMenu(std::string(label).c_str())) {
            std::vector<std::pair<std::vector<std::string_view>, const DebugMenu::Callback*>> sub(
                entries.begin() + static_cast<long>(i), entries.begin() + static_cast<long>(j));
            DrawLevel(sub, depth + 1);
            ImGui::EndMenu();
        }
        i = j;
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

    std::vector<std::pair<std::vector<std::string_view>, const Callback*>> entries;
    entries.reserve(items_.size());
    for (const auto& item : items_) {
        entries.emplace_back(SplitPath(item.path), &item.callback);
    }
    DrawLevel(entries, 0);

    ImGui::EndPopup();
}

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
