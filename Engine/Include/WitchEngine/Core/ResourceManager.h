#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

namespace witch {

class ResourceManager {
public:
    // Returns a cached TextureHandle for the given file path.
    // Loads via stb_image + IRenderer::CreateTexture on first access.
    std::expected<rhi::TextureHandle, std::string> LoadTexture(std::string_view path);

private:
    std::unordered_map<std::string, rhi::TextureHandle> textureCache_;
};

} // namespace witch
