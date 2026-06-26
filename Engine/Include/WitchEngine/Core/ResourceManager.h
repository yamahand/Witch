#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>

namespace witch {

/// テクスチャキャッシュを管理するサービス。Services 経由で取得する。
class ResourceManager {
public:
    /// パスに対応するテクスチャハンドルを返す。
    /// 初回アクセス時は stb_image でロードして IRenderer::CreateTexture に渡し、以降はキャッシュから返す。
    /// @param path アセットルートからの相対パス
    /// @return 失敗時はエラーメッセージを返す
    std::expected<rhi::TextureHandle, std::string> LoadTexture(std::string_view path);

private:
    std::unordered_map<std::string, rhi::TextureHandle> textureCache_;
};

} // namespace witch
