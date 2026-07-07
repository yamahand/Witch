#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/HierarchyWindow.h"
#include "WitchEngine/Scene/Scene.h"
#include <format>
#include <imgui.h>
#include <string>
#include <string_view>
#include <typeinfo>

namespace witch::debug {

namespace {

/// typeid(...).name() から "class witch::PlayerObject" → "PlayerObject" を取り出す。
/// 先頭からの削除のみなので、返るポインタは元の文字列終端まで有効（null 終端保証）。
const char* StripTypeName(const char* raw) {
    std::string_view s(raw);
    if (s.starts_with("class "))  s.remove_prefix(6);
    if (s.starts_with("struct ")) s.remove_prefix(7);
    if (const auto pos = s.rfind("::"); pos != std::string_view::npos)
        s.remove_prefix(pos + 2);
    return s.data();
}

/// ヒエラルキー・インスペクター見出しに使う表示名。Name() 未設定時は型名。
const char* DisplayName(const GameObject& obj) {
    return obj.Name().empty() ? StripTypeName(typeid(obj).name()) : obj.Name().c_str();
}

void DrawInspectorPane(GameObject* obj) {
    if (!obj) {
        ImGui::TextDisabled("(no selection)");
        return;
    }

    // 名前（編集可能）と id。ラベル重複を避けるため ID スコープを切る。
    ImGui::PushID(obj);
    char nameBuf[128] = {};
    obj->Name().copy(nameBuf, sizeof(nameBuf) - 1);
    if (ImGui::InputTextWithHint("name", DisplayName(*obj), nameBuf, sizeof(nameBuf)))
        obj->SetName(nameBuf);
    ImGui::Text("id=%llu", static_cast<unsigned long long>(obj->Id()));

    // Transform は GameObject の標準メンバなのでウィンドウ側が直接描く。
    ImGui::SeparatorText("Transform");
    ImGui::DragFloat2("position", &obj->transform.x);
    ImGui::DragFloat("rotation", &obj->transform.rotation, 0.01f);
    ImGui::DragFloat2("scale", &obj->transform.scaleX, 0.01f);

    // サブクラス固有の追加表示。
    obj->DrawInspector();

    ImGui::SeparatorText("Components");
    for (const auto& comp : obj->DebugComponents()) {
        ImGui::PushID(comp.get());
        if (ImGui::TreeNodeEx(StripTypeName(typeid(*comp).name()),
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            comp->DrawInspector();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

} // namespace

void HierarchyWindow::Draw(Scene* scene) {
    if (!open_ || !scene) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(560, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hierarchy", &open_)) {
        ImGui::End();
        return;
    }

    // ── 左ペイン: オブジェクト一覧 ──
    GameObject* selectedObj = nullptr;
    if (ImGui::BeginChild("HierarchyPane", ImVec2(220, 0), ImGuiChildFlags_ResizeX)) {
        for (const auto& obj : scene->DebugObjects()) {
            if (obj->IsDestroyed()) continue;
            // id 付きラベルなので表示文字列自体が ImGui ID として一意。
            const std::string label = std::format(
                "{} (id={})", DisplayName(*obj), obj->Id());
            const bool isSelected = (obj->Id() == selected_);
            if (ImGui::Selectable(label.c_str(), isSelected))
                selected_ = obj->Id();
            if (obj->Id() == selected_)
                selectedObj = obj.get();
        }
    }
    ImGui::EndChild();

    // 選択オブジェクトが消えていたら未選択に戻す。
    if (!selectedObj)
        selected_ = kInvalidId;

    // ── 右ペイン: インスペクター ──
    ImGui::SameLine();
    if (ImGui::BeginChild("InspectorPane")) {
        DrawInspectorPane(selectedObj);
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
