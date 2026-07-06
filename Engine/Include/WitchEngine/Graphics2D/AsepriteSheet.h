#pragma once
#include "WitchEngine/Core/TextureInfo.h"
#include <string>
#include <string_view>
#include <vector>

namespace witch {

/// Aseprite タグのループ方向（.ase の Tags チャンク準拠）。
enum class AsepriteLoopDir : uint8_t {
    Forward = 0,
    Reverse = 1,
    PingPong = 2,
    PingPongReverse = 3,
};

/// アトラス上の 1 コマ。矩形サイズは全コマ共通（AsepriteSheet::frameWidth/Height）。
struct AsepriteFrame {
    int x = 0;               ///< アトラス上の px 位置（左上）
    int y = 0;
    float duration = 0.1f;   ///< 表示時間（秒）。Aseprite のコマごとの duration 由来。
};

/// .ase 内の 1 アニメーション区間（Aseprite のタグ）。
struct AsepriteTag {
    std::string name;
    int from = 0;            ///< 開始フレーム index（両端含む）
    int to   = 0;
    AsepriteLoopDir direction = AsepriteLoopDir::Forward;
    int repeat = 0;          ///< 再生回数。0 = 無限ループ（Aseprite 準拠）
};

/// .ase ファイル 1 個をロードした結果。全フレームを 1 枚のアトラスに焼いた
/// テクスチャと、コマごとの duration・タグ定義を持つ。
/// ResourceManager::LoadAseprite がキャッシュして shared_ptr で返す。
struct AsepriteSheet {
    TextureInfo texture;     ///< 全フレームを格子状に並べたアトラス
    int frameWidth  = 0;     ///< 1 コマの px 幅（= キャンバス幅）
    int frameHeight = 0;     ///< 1 コマの px 高さ（= キャンバス高さ）
    std::vector<AsepriteFrame> frames;
    std::vector<AsepriteTag> tags;

    /// タグ名で線形探索する。見つからなければ nullptr。
    const AsepriteTag* FindTag(std::string_view name) const {
        for (const auto& tag : tags)
            if (tag.name == name) return &tag;
        return nullptr;
    }
};

} // namespace witch
