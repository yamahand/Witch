#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/ProfilerHud.h"

#include <imgui.h>

#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"

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

void ProfilerHud::DrawBody() {
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

    // GPU タイムスタンプは、対応するフレームスロットのフェンス完了後に読む。
    // CPU の Render.WaitGpu / Present 待機とは別の、実際の GPU コマンド実行時間。
    if (rhi::IRenderer* renderer = Services::Instance().renderer;
        renderer && renderer->HasGpuFrameTiming()) {
        ImGui::SameLine();
        ImGui::Text("GPU %.2f ms", renderer->GpuFrameMs());
    }

    ImGui::SameLine();
    ImGui::Checkbox("Graph", &showGraph_);

    // ── VSync トグル + 状態表示 ──
    // 実際の renderer 状態を直接読む（推測でなく事実で切り分ける）。ティアリング
    // 未対応環境では SetVSync(false) が無視されるため、チェックを外しても ON の
    // ままになる。その場合はここのチェックが再び入って「効いていない」と分かる。
    if (rhi::IRenderer* renderer = Services::Instance().renderer) {
        ImGui::SameLine();
        bool vsync = renderer->VSync();
        if (ImGui::Checkbox("VSync", &vsync)) {
            renderer->SetVSync(vsync);
        }
    }

    // スパイク検出ログのトグル。ON にすると閾値超過フレームのゾーン内訳が Logger
    // （Log Viewer / ファイル）へ Warn 出力される。原因ゾーンと発生タイミングを追う用。
    ImGui::SameLine();
    bool spikeLog = collector.SpikeLogEnabled();
    if (ImGui::Checkbox("Spike log", &spikeLog)) {
        // 閾値の倍率・下限は Collector 側の既定値（kDefaultSpikeMultiplier /
        // kDefaultSpikeFloorMs）に一元化されている。ここでは有効/無効だけを切り替え、
        // 実際の閾値は Collector が「直近平均 * 倍率と下限の大きい方」を毎フレーム
        // 動的計算する（ON にした瞬間の平均で固定しないため乖離しない）。
        profile::Collector::Instance().SetSpikeLog(spikeLog);
    }

    // 蓄積 max / peak のリセット。起動時やレベル読み込みの一過性スパイクが
    // Max 列・peak 表示に残り続けるのを消す（フレーム時間履歴もクリアするので
    // グラフは一瞬空になる）。avg と直近の内訳は残す。
    ImGui::SameLine();
    if (ImGui::Button("Reset max")) {
        profile::Collector::Instance().ResetMax();
    }

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
            // strict-weak-ordering を保つため、降順は !less ではなく引数を入れ替えて
            // 評価する（!less だと等値で true を返し std::sort が未定義動作になる）。
            const profile::ZoneStat& lhs = ascending ? a : b;
            const profile::ZoneStat& rhs = ascending ? b : a;
            switch (column) {
            case 0:  return lhs.name   < rhs.name;
            case 1:  return lhs.calls  < rhs.calls;
            case 2:  return lhs.lastMs < rhs.lastMs;
            case 3:  return lhs.avgMs  < rhs.avgMs;
            case 4:  return lhs.maxMs  < rhs.maxMs;
            default: return lhs.lastMs < rhs.lastMs;
            }
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
        // DefaultSort だけだと初回のソート方向が昇順になり、「既定 Last の降順」
        // （重いゾーンを上に出す）と食い違うため PreferSortDescending を併せて指定する。
        ImGui::TableSetupColumn("Last(ms)", ImGuiTableColumnFlags_WidthFixed |
                                                ImGuiTableColumnFlags_DefaultSort |
                                                ImGuiTableColumnFlags_PreferSortDescending,
                                70.0f);
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
}

#else  // !WITCH_PROFILE_COLLECT

void ProfilerHud::DrawBody() {
    // このビルドではインプロセス集約が無効（WITCH_PROFILE_COLLECT 未定義）。
    // 通常 WITCH_DEBUG_UI と連動して定義されるため、ここに来るのは
    // 手動でマクロ定義を外した特殊構成のみ。
    ImGui::TextWrapped(
        "Profiling collector is disabled in this build "
        "(WITCH_PROFILE_COLLECT is not defined).");
}

#endif // WITCH_PROFILE_COLLECT

// 共通の前後処理はここに一本化し、中身だけを DrawBody() で切り替える。
void ProfilerHud::Draw() {
    if (!open_) {
        return;
    }
    if (!ImGui::Begin("Profiler", &open_)) {
        ImGui::End();
        return;
    }
    DrawBody();
    ImGui::End();
}

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
