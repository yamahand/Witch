#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"

namespace witch {

std::expected<rhi::TextureHandle, std::string>
ResourceManager::LoadTexture(std::string_view path) {
    std::string key(path);

    if (auto it = textureCache_.find(key); it != textureCache_.end())
        return it->second;

    int w, h, ch;
    // Force 4-channel RGBA so the texture format is always R8G8B8A8.
    unsigned char* pixels = stbi_load(key.c_str(), &w, &h, &ch, 4);
    if (!pixels)
        return std::unexpected(std::string("stbi_load failed: ") + stbi_failure_reason());

    auto* renderer = Services::Instance().renderer;
    if (!renderer) {
        stbi_image_free(pixels);
        return std::unexpected(std::string("Renderer not available"));
    }

    auto result = renderer->CreateTexture(pixels, w, h);
    stbi_image_free(pixels);

    if (!result) return std::unexpected(result.error());

    log::Info("Texture loaded: {} ({}x{})", path, w, h);
    textureCache_[key] = *result;
    return *result;
}

} // namespace witch
