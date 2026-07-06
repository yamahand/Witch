#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/LogViewerWindow.h"
#include "WitchEngine/Core/Log/ViewerSink.h"
#include "Core/Log/TimestampFormat.h"
#include <format>
#include <imgui.h>
#include <string_view>
#include <vector>

namespace witch::debug {

namespace {

ImVec4 LevelColor(log::LogLevel level) {
    switch (level) {
    case log::LogLevel::Trace: return {0.6f, 0.6f, 0.6f, 1.0f};
    case log::LogLevel::Info:  return {0.9f, 0.9f, 0.9f, 1.0f};
    case log::LogLevel::Warn:  return {1.0f, 0.8f, 0.3f, 1.0f};
    case log::LogLevel::Error: return {1.0f, 0.4f, 0.4f, 1.0f};
    case log::LogLevel::Fatal: return {1.0f, 0.2f, 0.6f, 1.0f};
    }
    return {1.0f, 1.0f, 1.0f, 1.0f};
}

} // namespace

void LogViewerWindow::Draw() {
    if (!open_) {
        return;
    }
    if (!ImGui::Begin("Log", &open_)) {
        ImGui::End();
        return;
    }

    // ── コントロール行 ──
    if (ImGui::Button("Clear")) {
        sink_->Clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    {
        static constexpr const char* kLevelNames[] = {"Trace", "Info", "Warn", "Error", "Fatal"};
        int level = static_cast<int>(minLevel_);
        if (ImGui::Combo("##minLevel", &level, kLevelNames, IM_ARRAYSIZE(kLevelNames))) {
            minLevel_ = static_cast<log::LogLevel>(level);
        }
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##categoryFilter", "category filter",
                             categoryFilter_, sizeof(categoryFilter_));

    ImGui::Separator();

    // ── ログ本体 ──
    // Snapshot はコピーを返すが、既定容量 4096 件のデバッグ UI 用途では十分軽い。
    const auto entries = sink_->Snapshot();
    const std::string_view filter(categoryFilter_);

    std::vector<const log::ViewerSink::Entry*> visible;
    visible.reserve(entries.size());
    for (const auto& e : entries) {
        if (e.level < minLevel_) continue;
        if (!filter.empty() && e.category.find(filter) == std::string::npos) continue;
        visible.push_back(&e);
    }

    if (ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& e = *visible[static_cast<size_t>(i)];
                std::string line;
                if (e.category.empty()) {
                    line = std::format("[{}][{:<5}] {}",
                                       log::FormatTimestamp(e.timestamp),
                                       log::ToString(e.level), e.message);
                } else {
                    line = std::format("[{}][{:<5}][{}] {}",
                                       log::FormatTimestamp(e.timestamp),
                                       log::ToString(e.level), e.category, e.message);
                }
                ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(e.level));
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s:%u\n%s", e.file, e.line, e.function);
                }
            }
        }
        clipper.End();

        // 末尾に張り付いているときだけ自動スクロールする（手動スクロール中は邪魔しない）。
        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
