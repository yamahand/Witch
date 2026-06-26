#pragma once
#include <cstdint>

namespace witch::rhi {

struct Color { float r, g, b, a; };
struct ClearDesc { Color color; };

struct TextureHandle {
    uint32_t id = 0;
    bool IsValid() const { return id != 0; }
};

struct SpriteDrawDesc {
    TextureHandle texture;
    float x = 0, y = 0;
    float width = 0, height = 0;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
};

} // namespace witch::rhi
