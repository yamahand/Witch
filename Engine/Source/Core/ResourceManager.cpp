#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Vfs/Vfs.h"
#include "Graphics2D/AsepriteLoader.h"

namespace witch {

std::expected<TextureInfo, std::string>
ResourceManager::LoadTexture(std::string_view path) {
    std::string key(path);

    if (auto it = textureCache_.find(key); it != textureCache_.end())
        return it->second;

    auto* vfs = Services::Instance().vfs;
    if (!vfs)
        return std::unexpected(std::string("VFS not available"));

    auto fileData = vfs->Read(path);
    if (!fileData)
        return std::unexpected(std::string("VFS read failed: ") + fileData.error());

    int w, h, ch;
    // Force 4-channel RGBA so the texture format is always R8G8B8A8.
    unsigned char* pixels = stbi_load_from_memory(
        fileData->Data(), static_cast<int>(fileData->Size()), &w, &h, &ch, 4);
    if (!pixels)
        return std::unexpected(std::string("stbi_load_from_memory failed: ") + stbi_failure_reason());

    auto* renderer = Services::Instance().renderer;
    if (!renderer) {
        stbi_image_free(pixels);
        return std::unexpected(std::string("Renderer not available"));
    }

    auto result = renderer->CreateTexture(pixels, w, h);
    stbi_image_free(pixels);

    if (!result) return std::unexpected(result.error());

    log::Info("Texture loaded: {} ({}x{})", path, w, h);
    TextureInfo info{*result, w, h};
    textureCache_[key] = info;
    return info;
}

std::expected<std::shared_ptr<const AsepriteSheet>, std::string>
ResourceManager::LoadAseprite(std::string_view path) {
    std::string key(path);

    if (auto it = asepriteCache_.find(key); it != asepriteCache_.end())
        return it->second;

    auto* vfs = Services::Instance().vfs;
    if (!vfs)
        return std::unexpected(std::string("VFS not available"));

    auto fileData = vfs->Read(path);
    if (!fileData)
        return std::unexpected(std::string("VFS read failed: ") + fileData.error());

    auto parsed = aseprite::ParseAse(
        std::span<const uint8_t>(fileData->Data(), fileData->Size()), path);
    if (!parsed)
        return std::unexpected(parsed.error());

    auto* renderer = Services::Instance().renderer;
    if (!renderer)
        return std::unexpected(std::string("Renderer not available"));

    auto texture = renderer->CreateTexture(parsed->atlasPixels.data(),
                                           parsed->atlasWidth, parsed->atlasHeight);
    if (!texture)
        return std::unexpected(texture.error());

    auto sheet = std::make_shared<AsepriteSheet>();
    sheet->texture     = TextureInfo{*texture, parsed->atlasWidth, parsed->atlasHeight};
    sheet->frameWidth  = parsed->frameWidth;
    sheet->frameHeight = parsed->frameHeight;
    sheet->frames      = std::move(parsed->frames);
    sheet->tags        = std::move(parsed->tags);

    log::Info("Aseprite loaded: {} ({} frames {}x{}, {} tags, atlas {}x{})",
              path, sheet->frames.size(), sheet->frameWidth, sheet->frameHeight,
              sheet->tags.size(), parsed->atlasWidth, parsed->atlasHeight);
    asepriteCache_[key] = sheet;
    return std::shared_ptr<const AsepriteSheet>(sheet);
}

} // namespace witch
