#pragma once

namespace witch {

/// デバッグ UI を描く能力を表す mixin 的基底。
/// WITCH_DEBUG_UI 定義時のみ virtual DrawDebugUI() を持つ。OFF ビルドでは空クラスになり、
/// 派生が DrawDebugUI() を override すると「基底に該当 virtual なし」でコンパイルエラーに
/// なる（override 漏れを release リンク時まで遅延させず早期検出する）。
class DebugUI {
#ifdef WITCH_DEBUG_UI
public:
    virtual ~DebugUI() = default;
    /// 毎フレーム ImGui フレーム内で呼ばれる自由描画フック。既定は何もしない。
    /// GameObject / Scene は選択状態に関わらず常時呼ばれる（オーバーレイ等の用途）。
    /// Component::DrawDebugUI() は現状呼び出し経路が無い（GameObject が転送しない）ため
    /// override しても実行時に呼ばれない。Component の表示は DrawInspector() を使うこと。
    virtual void DrawDebugUI() {}
    /// ヒエラルキーウィンドウで選択されたときだけ呼ばれる。既定は何もしない
    /// （表示するものがない派生は実装不要）。TreeNode / PushID の枠は呼び出し側
    /// （HierarchyWindow）が描くため、実装はプロパティ描画のみでよい。
    virtual void DrawInspector() {}
#endif
};

} // namespace witch
