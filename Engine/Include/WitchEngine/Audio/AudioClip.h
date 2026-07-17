#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace witch::audio {

/// エンコード済み（Ogg 等）音声バイト列。デコードは再生時に IAudio 実装が行う。
/// ResourceManager がキャッシュし、再生中のボイスも shared_ptr で保持するため、
/// シーン切替の UnloadAll 後も再生が終わるまで生存する
/// （AsepriteSheet と違い use_count > 1 は正常な状態）。
struct AudioClip {
    std::string path;                  ///< ログ用
    std::vector<uint8_t> encodedBytes; ///< VFS から読んだままのエンコード済みデータ
};

} // namespace witch::audio
