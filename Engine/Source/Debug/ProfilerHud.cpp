#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/ProfilerHud.h"

#include <imgui.h>

#ifdef WITCH_PROFILE_COLLECT
#include "Core/ProfileCollector.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>
#endif

namespace witch::debug {

#ifdef WITCH_PROFILE_COLLECT

namespace {

// フレーム時間 [ms] から色を決める（60fps=16.7ms 基準）。緑→黄→赤。
ImVec4 FrameMsColor(float ms) {
    if (ms <= 16.7f) return {0.4f, 0.9f, 0.4f, 1.0f}; // 60fps 以上
    if (ms <= 33.3f) return {1.0f, 0.8f, 0.3f, 1.0f}; // 30〜60fps
    return {1.0f, 0.4f, 0.4f, 1.0f};                  // 30fps 未満
}

} // namespace

void ProfilerHud::Draw() {
    if (!open_) {
        return;
    }
    if (!ImGui::Begin("Profiler", &open_)) {
        ImGui::End();
        return;
    }

    const auto& collector = profile::Collector::Instance();

    // 履歴を時系列順に取り出す（グラフ用・折り返し解決済み）。
    static std::array<float, profile::kHistoryFrames> history;
    const std::size_t historyCount = collector.FrameHistoryOrdered(history);

    // ── ヘッダ: FPS / フレーム ms ──
    const float latestMs = collector.LastFrameMs();
    double sumMs = 0.0;
    float maxMs = 0.0f;
    for (std::size_t i = 0; i < historyCount; ++i) {
        const float v = history[i];
        sumMs += v;
        if (v > maxMs) maxMs = v;
    }
    const double avgMs = historyCount > 0 ? sumMs / static_cast<double>(historyCount) : 0.0;
    // FPS は最新フレーム時間から出す（体感に近い瞬間値）。0 割は避ける。
    const double fps = latestMs > 0.0f ? 1000.0 / latestMs : 0.0;
    ImGui::PushStyleColor(ImGuiCol_Text, FrameMsColor(latestMs));
    ImGui::Text("%.1f FPS  |  %.2f ms  |  %.2f ms avg  |  %.2f ms peak",
                fps, latestMs, avgMs, maxMs);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::Checkbox("Graph", &showGraph_);

    // ── フレーム時間グラフ ──
    if (showGraph_ && historyCount > 1) {
        // PlotLines は連続配列を先頭から描く。リングバッファをそのまま渡すと
        // 折り返し位置で不連続に見えるが、120 フレームの俯瞰用途では許容範囲。
        // スケール上限は 33.3ms（30fps ライン）と実測ピークの大きい方に合わせる。
        const float scaleMax = std::max(33.3f, maxMs * 1.1f);
        ImGui::PlotLines("##frametime", history.data(), static_cast<int>(historyCount),
                         0, nullptr, 0.0f, scaleMax, ImVec2(-1.0f, 60.0f));
    }

    ImGui::Separator();

    // ── ゾーン別テーブル ──
    // Snapshot は前フレーム確定値のコピー参照。表示のためだけにローカルへコピーし、
    // 選択列でソートする（元データは順不同）。
    std::vector<profile::ZoneStat> zones(collector.Snapshot().begin(),
                                         collector.Snapshot().end());

    auto sortZones = [&](int column, bool ascending) {
        auto cmp = [&](const profile::ZoneStat& a, const profile::ZoneStat& b) {
            bool less;
            switch (column) {
            case 0:  less = a.name < b.name; break;
            case 1:  less = a.calls < b.calls; break;
            case 2:  less = a.lastMs < b.lastMs; break;
            case 3:  less = a.avgMs < b.avgMs; break;
            case 4:  less = a.maxMs < b.maxMs; break;
            default: less = a.lastMs < b.lastMs; break;
            }
            return ascending ? less : !less;
        };
        std::sort(zones.begin(), zones.end(), cmp);
    };

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("Zones", 5, kTableFlags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Zone", ImGuiTableColumnFlags_WidthStretch |
                                            ImGuiTableColumnFlags_NoSort);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Last(ms)", ImGuiTableColumnFlags_WidthFixed |
                                                ImGuiTableColumnFlags_DefaultSort, 70.0f);
        ImGui::TableSetupColumn("Avg(ms)", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Max(ms)", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();

        // ImGui のソート指定を取り込む（列ヘッダクリックで更新される）。
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
            if (specs->SpecsCount > 0) {
                sortColumn_ = specs->Specs[0].ColumnIndex;
                sortAscending_ = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
            }
        }
        sortZones(sortColumn_, sortAscending_);

        for (const auto& z : zones) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            // name は string_view（null 終端保証なし）。%.*s で長さ指定して渡す。
            ImGui::Text("%.*s", static_cast<int>(z.name.size()), z.name.data());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", z.calls);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", z.lastMs);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f", z.avgMs);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.3f", z.maxMs);
        }

        ImGui::EndTable();
    }

#else  // !WITCH_PROFILE_COLLECT

void ProfilerHud::Draw() {
    if (!open_) {
        return;
    }
    if (!ImGui::Begin("Profiler", &open_)) {
        ImGui::End();
        return;
    }
    // このビルドではインプロセス集約が無効（WITCH_PROFILE_COLLECT 未定義）。
    // 通常 WITCH_DEBUG_UI と連動して定義されるため、ここに来るのは
    // 手動でマクロ定義を外した特殊構成のみ。
    ImGui::TextWrapped(
        "Profiling collector is disabled in this build "
        "(WITCH_PROFILE_COLLECT is not defined).");

#endif // WITCH_PROFILE_COLLECT

    ImGui::End();
}

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
