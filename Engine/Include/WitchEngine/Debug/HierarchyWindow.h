#pragma once
#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Scene/GameObject.h"

namespace witch {
class Scene;
} // namespace witch

namespace witch::debug {

/// シーン内の GameObject 一覧（ヒエラルキー）と選択中オブジェクトの詳細
/// （インスペクター）を 1 ウィンドウ左右 2 ペインで表示するデバッグウィンドウ。
/// インスペクターは選択中オブジェクトの Transform・DrawInspector()・
/// 各 Component の DrawInspector() を描く。
/// WITCH_DEBUG_UI 定義時のみ存在し、OFF ビルドではヘッダごとビルドから外れる。
class HierarchyWindow {
public:
    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）で毎フレーム呼ぶ。
    /// @param scene 表示対象の現在シーン（非所有）。null 可（シーン未設定時は何もしない）。
    void Draw(Scene* scene);

    bool IsOpen() const { return open_; }
    void SetOpen(bool open) { open_ = open; }

private:
    ObjectId selected_ = kInvalidId;  ///< 弱参照。消えたら未選択に戻す。
    bool open_ = true;                ///< 起動時 ON。閉じたら再表示なし（ON/OFF 切替は今後追加）。
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
