#pragma once
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/TextureInfo.h"
#include <cstdint>

namespace witch {

class SpriteComponent;
class AnimationComponent;

/// M5 機能のデモシーン。
/// 矢印: Witch 移動 / WASD: カメラ / Q,E,ホイール: ズーム
/// R: 回転（押下中）/ F: 左右反転 / T: tint 循環 / L: レイヤー入替
/// P: アニメ Play/Stop / O: アニメ loop 切替 / Escape: 終了
class EmptyScene : public Scene {
public:
    void OnEnter() override;
    void Update(float dt) override;
    void OnExit() override;

private:
    uint64_t frameCount_ = 0;
    TextureInfo spriteTexture_;
    TextureInfo testSheet_;
    ObjectId witchId_ = kInvalidId;         // 矢印キーで動かすスプライト（弱参照）。

    // Spawn 時に得たコンポーネントへの生ポインタ。オーナー GameObject が
    // シーンと同寿命（Destroy しない）なのでデモ用途ではそのまま保持できる。
    SpriteComponent*    witchSprite_ = nullptr;
    SpriteComponent*    staticSprite_ = nullptr;
    AnimationComponent* anim_ = nullptr;

    int  tintIndex_ = 0;
    bool animLoop_ = true;
    bool loggedFinished_ = false;
};

} // namespace witch
