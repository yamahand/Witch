#pragma once
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/TextureInfo.h"
#include <cstdint>

namespace witch {

class EmptyScene : public Scene {
public:
    void OnEnter() override;
    void Update(float dt) override;
    void OnExit() override;

private:
    uint64_t frameCount_ = 0;
    TextureInfo spriteTexture_;
    ObjectId witchId_ = kInvalidId; // 矢印キーで動かすスプライト（弱参照）。
};

} // namespace witch
