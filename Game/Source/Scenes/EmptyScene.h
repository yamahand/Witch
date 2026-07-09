#pragma once
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/TextureInfo.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace witch {

class SpriteComponent;
class AnimationComponent;
class AsepriteComponent;
struct AsepriteSheet;

/// M5 機能のデモシーン。
/// 矢印: Witch 移動 / WASD: カメラ / Q,E,ホイール: ズーム
/// R: 回転（押下中）/ F: 左右反転 / T: tint 循環 / L: レイヤー入替
/// P: アニメ Play/Stop / O: アニメ loop 切替 / N: Unity ちゃんアニメ切替 / Escape: 終了
class EmptyScene : public Scene {
public:
    void OnEnter() override;
    /// 連続量の入力（IsDown + dt スケール）: 移動・回転・カメラ・キーズーム。
    void FixedUpdate(float fixedDt) override;
    /// エッジ/瞬間量の入力（WasPressed / ホイール）と定期ログ。
    /// エッジ検出は入力世代がフレーム単位のため、固定側に置くと
    /// 多重ステップフレームで二重発火する（Scene.h の契約参照）。
    void FrameUpdate(float dt) override;
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

    // Aseprite デモ（Unity ちゃん）。N キーでシートを循環する。
    // path はロードに成功したものだけを sheet と対で持つ（ログ表示用）。
    struct UnitySheet {
        const char* path;
        std::shared_ptr<const AsepriteSheet> sheet;
    };
    std::vector<UnitySheet> unitySheets_;
    AsepriteComponent* unityAnim_ = nullptr;
    int unitySheetIndex_ = 0;

    int  tintIndex_ = 0;
    bool animLoop_ = true;
    bool loggedFinished_ = false;
};

} // namespace witch
