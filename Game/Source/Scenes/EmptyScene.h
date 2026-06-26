#pragma once
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Rhi/RhiTypes.h"
#include <cstdint>

namespace witch {

class EmptyScene : public Scene {
public:
    void OnEnter() override;
    void Update(float dt) override;
    void OnExit() override;

private:
    uint64_t frameCount_ = 0;
    rhi::TextureHandle spriteTexture_;
};

} // namespace witch
