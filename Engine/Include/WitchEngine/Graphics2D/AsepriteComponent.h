#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Graphics2D/AsepriteSheet.h"
#include <memory>
#include <string_view>

namespace witch {

class SpriteComponent;

/// AsepriteSheet のコマを兄弟 SpriteComponent に時間で反映するコンポーネント。
/// コマごとの duration・タグ・ループ方向（forward / reverse / pingpong）に対応する。
/// 兄弟解決は遅延を許す: AddComponent は OnAttach を push の前に呼ぶため、
/// AsepriteComponent を先に追加すると OnAttach 時点では SpriteComponent がまだ無く、
/// 解決は最初の Update の ResolveSprite まで遅れる。Animation フェーズは Render
/// フェーズより先に走るため、追加順によらずコマ更新は同一フレームの描画提出に反映される。
/// シート差し替え時は SpriteComponent のテクスチャも張り替える。
class AsepriteComponent : public Component {
public:
    explicit AsepriteComponent(std::shared_ptr<const AsepriteSheet> sheet);

    void OnAttach() override;
    void Update(float dt) override;

    /// コマ送りは Animation フェーズ（Render フェーズの提出より前に矩形を確定する）。
    UpdatePhase Phase() const override { return UpdatePhase::Animation; }

#ifdef WITCH_DEBUG_UI
    void DrawInspector() override;
#endif

    /// 全フレームを先頭から無限ループで再生する。
    void Play();
    /// 名前のタグを再生する（方向・repeat はタグ定義に従う。repeat 0 = 無限）。
    /// タグが見つからなければ警告して現在の再生を維持し false を返す。
    bool Play(std::string_view tagName);
    /// 現在のコマで停止する。
    void Stop();
    bool IsPlaying() const { return playing_; }
    /// 有限 repeat の再生が最後まで到達済みか。
    bool IsFinished() const { return finished_; }

    /// シートを差し替えて全フレーム再生を先頭から始める。
    /// 兄弟 SpriteComponent のテクスチャも新しいアトラスに張り替える。
    void SetSheet(std::shared_ptr<const AsepriteSheet> sheet);
    const std::shared_ptr<const AsepriteSheet>& Sheet() const { return sheet_; }

    /// 現在表示中のフレーム index（.ase 内の絶対フレーム番号）。
    int CurrentFrame() const { return frame_; }

private:
    /// 再生範囲を設定して先頭コマから再生を始める。
    void StartRange(int from, int to, AsepriteLoopDir dir, int repeat);
    /// 1 コマ進める。範囲端では方向・repeat に従いループ / 折り返し / 停止する。
    void Advance();
    /// 現在コマのソース矩形を SpriteComponent に反映する。
    void ApplyFrame();
    /// sprite_ 未解決時に Owner から解決を試みる。見つかればテクスチャと現在コマを反映。
    /// @param warnIfMissing 見つからない場合に一度だけ警告する。OnAttach 時点では
    ///                      SpriteComponent が後から追加される正常ケースがあるため false。
    bool ResolveSprite(bool warnIfMissing = true);

    std::shared_ptr<const AsepriteSheet> sheet_;
    SpriteComponent* sprite_ = nullptr;

    // 再生範囲（タグまたは全フレーム）。
    int from_ = 0;
    int to_   = 0;
    AsepriteLoopDir dir_ = AsepriteLoopDir::Forward;
    int repeatLeft_ = 0;        ///< 残り再生回数。0 = 無限
    bool infinite_  = true;
    bool pingpongForward_ = true;

    int   frame_ = 0;           ///< 現在の絶対フレーム index
    float time_  = 0.0f;        ///< 現在コマ内の経過秒
    bool  playing_  = true;
    bool  finished_ = false;
    bool  warnedNoSprite_ = false;
};

} // namespace witch
