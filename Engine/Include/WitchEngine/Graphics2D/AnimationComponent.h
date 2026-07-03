#pragma once
#include "WitchEngine/Scene/Component.h"
#include <vector>

namespace witch {

class SpriteComponent;

/// スプライトシート上の 1 アニメーション定義。
/// シートは frameWidth x frameHeight のセルを行優先グリッドとみなし、
/// frames にはセル番号（cell = row * columns + col）を再生順に並べる。
struct AnimationClip {
    int frameWidth  = 0;      ///< 1 コマの px 幅
    int frameHeight = 0;      ///< 1 コマの px 高さ
    int columns     = 1;      ///< シート上のグリッド列数
    std::vector<int> frames;  ///< 再生順のセル番号
    float fps  = 10.0f;
    bool  loop = true;
};

/// 兄弟 SpriteComponent のソース矩形を時間で切り替えるコンポーネント。
/// 契約: SpriteComponent を先に AddComponent しておくこと
/// （OnAttach で GetComponent するため。不在なら警告して不活性になる）。
class AnimationComponent : public Component {
public:
    explicit AnimationComponent(AnimationClip clip);

    void OnAttach() override;
    void Update(float dt) override;

#ifdef WITCH_DEBUG_UI
    void DrawDebugUI() override;
#endif

    /// 先頭から再生する（finished をリセット）。
    void Play();
    /// 現在のコマで停止する。
    void Stop();
    bool IsPlaying() const { return playing_; }
    /// loop = false のクリップが最終コマへ到達済みか。
    bool IsFinished() const { return finished_; }

    /// クリップを差し替えて先頭から再生する（歩き / 待機の切替に使う）。
    void SetClip(AnimationClip clip);
    /// frames 配列内の現在インデックス。
    int CurrentFrame() const { return frameIndex_; }

private:
    /// 現在コマのセル番号からソース矩形を計算して SpriteComponent に反映する。
    void ApplyFrame();

    AnimationClip clip_;
    SpriteComponent* sprite_ = nullptr;
    float time_       = 0.0f;
    int   frameIndex_ = 0;
    bool  playing_    = true;
    bool  finished_   = false;
};

} // namespace witch
