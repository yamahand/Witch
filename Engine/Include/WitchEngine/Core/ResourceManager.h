#pragma once
#include "WitchEngine/Core/TextureInfo.h"
#include "WitchEngine/Graphics2D/AsepriteSheet.h"
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace witch {

/// テクスチャ・Aseprite シートのキャッシュを管理するサービス。Services 経由で取得する。
class ResourceManager {
public:
    /// パスに対応するテクスチャ情報（ハンドル + ピクセルサイズ）を返す。
    /// 初回アクセス時は VFS 経由でバイト列を読み stb_image でデコードして
    /// IRenderer::CreateTexture に渡し、以降はキャッシュから返す。
    /// @param path VFS マウントルートからの相対パス（Services::Instance().vfs 経由で解決）
    /// @return 失敗時はエラーメッセージを返す
    std::expected<TextureInfo, std::string> LoadTexture(std::string_view path);

    /// .ase / .aseprite ファイルをロードし、全フレームを 1 枚のアトラスに焼いた
    /// AsepriteSheet を返す。初回アクセス時は VFS 経由で読んでパース + テクスチャ化し、
    /// 以降はキャッシュから返す。AsepriteComponent に渡して使う。
    /// @param path VFS マウントルートからの相対パス
    /// @return 失敗時はエラーメッセージを返す
    std::expected<std::shared_ptr<const AsepriteSheet>, std::string>
    LoadAseprite(std::string_view path);

private:
    std::unordered_map<std::string, TextureInfo> textureCache_;
    std::unordered_map<std::string, std::shared_ptr<const AsepriteSheet>> asepriteCache_;
};

} // namespace witch
