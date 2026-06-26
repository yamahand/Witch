#pragma once

namespace witch {

struct Transform {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;  // radians, CCW positive
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

} // namespace witch
