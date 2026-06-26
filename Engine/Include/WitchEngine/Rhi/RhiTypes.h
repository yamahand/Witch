#pragma once

namespace witch::rhi {

struct Color {
    float r, g, b, a;
};

struct ClearDesc {
    Color color;
};

} // namespace witch::rhi
