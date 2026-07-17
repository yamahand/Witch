// miniaudio を閉じ込める唯一の TU（Rhi/D3D12・Level/LdtkLoader.cpp と同じ境界隔離）。
// ma_* の型・関数・ヘッダをこのファイルの外へ出さないこと。
//
// Ogg Vorbis: miniaudio の内蔵デコーダは WAV / FLAC / MP3 のみ。miniaudio.h より先に
// stb_vorbis のヘッダ部を include しておくと miniaudio が検出して Vorbis 対応が有効になり、
// stb_vorbis の実装部は miniaudio 実装の後に置く（miniaudio.h 冒頭ドキュメント記載の手順。
// stb_vorbis.c は vcpkg の stb ポートから取得。ResourceManager.cpp の stb_image とは
// 別ライブラリなので実装マクロは衝突しない）。
// 外部ライブラリの実装を丸ごと含む TU のため、/W4 /WX はこの範囲だけ抑制する。
#pragma warning(push)
#pragma warning(disable : 4100 4244 4245 4456 4457 4701 4702)
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#pragma warning(pop)

#include "Audio/AudioFactory.h"
#include "WitchEngine/Audio/AudioClip.h"
#include "WitchEngine/Audio/IAudio.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Core/Services.h"

#include <format>
#include <memory>
#include <utility>
#include <vector>

namespace witch::audio {
namespace {

/// 再生中の 1 ボイス。miniaudio オブジェクトは move 不可のためヒープに置き、
/// seVoices_ の再配置でアドレスが動かないようにする。
/// clip はエンコード済みバイト列の生存保証（デコードは再生中に逐次行われるため、
/// ResourceManager::UnloadAll 後もボイスが生きている間はバイト列を保持し続ける）。
struct Voice {
    std::shared_ptr<const AudioClip> clip;
    std::unique_ptr<ma_decoder> decoder;
    std::unique_ptr<ma_sound> sound;

    explicit operator bool() const { return static_cast<bool>(sound); }
};

/// sound → decoder → clip の順で解放する。ma_sound_uninit がノードグラフから
/// 切り離した後は、オーディオスレッドが decoder に触ることはない。
void ReleaseVoice(Voice& voice) {
    if (voice.sound) ma_sound_uninit(voice.sound.get());
    voice.sound.reset();
    if (voice.decoder) ma_decoder_uninit(voice.decoder.get());
    voice.decoder.reset();
    voice.clip.reset();
}

class MiniaudioAudio final : public IAudio {
public:
    /// デバイスとバス（BGM / SE のサウンドグループ）を初期化する。
    /// 失敗時も部分初期化状態でデストラクタが安全に呼べる（Ready フラグで防御）。
    std::expected<void, std::string> Init() {
        if (ma_result r = ma_engine_init(nullptr, &engine_); r != MA_SUCCESS)
            return std::unexpected(
                std::format("ma_engine_init failed: {}", ma_result_description(r)));
        engineReady_ = true;

        if (ma_result r = ma_sound_group_init(&engine_, 0, nullptr, &bgmGroup_);
            r != MA_SUCCESS)
            return std::unexpected(
                std::format("ma_sound_group_init (BGM) failed: {}", ma_result_description(r)));
        bgmGroupReady_ = true;

        if (ma_result r = ma_sound_group_init(&engine_, 0, nullptr, &seGroup_);
            r != MA_SUCCESS)
            return std::unexpected(
                std::format("ma_sound_group_init (SE) failed: {}", ma_result_description(r)));
        seGroupReady_ = true;

        log::Info("Audio initialized: {} ch, {} Hz", ma_engine_get_channels(&engine_),
                  ma_engine_get_sample_rate(&engine_));
        return {};
    }

    ~MiniaudioAudio() override {
        // ボイス（グループにぶら下がるノード）→ グループ → エンジンの順。
        // ma_engine_uninit がデバイススレッドを停止して join する。
        ReleaseVoice(bgm_);
        for (Voice& voice : seVoices_) ReleaseVoice(voice);
        seVoices_.clear();
        if (seGroupReady_) ma_sound_group_uninit(&seGroup_);
        if (bgmGroupReady_) ma_sound_group_uninit(&bgmGroup_);
        if (engineReady_) ma_engine_uninit(&engine_);
    }

    void PlaySe(std::string_view path, [[maybe_unused]] float volume) override {
        log::Warn("PlaySe: not implemented yet ({})", path);
    }

    std::expected<void, std::string> PlayBgm(std::string_view path,
                                             [[maybe_unused]] const BgmParams& params) override {
        return std::unexpected(std::format("PlayBgm: not implemented yet ({})", path));
    }

    void StopBgm() override {}

    [[nodiscard]] bool IsBgmPlaying() const override { return false; }

    void SetMasterVolume(float volume) override { ma_engine_set_volume(&engine_, volume); }
    void SetBgmVolume(float volume) override { ma_sound_group_set_volume(&bgmGroup_, volume); }
    void SetSeVolume(float volume) override { ma_sound_group_set_volume(&seGroup_, volume); }

    void Update() override {}

private:
    ma_engine engine_{};
    ma_sound_group bgmGroup_{};
    ma_sound_group seGroup_{};
    bool engineReady_ = false;
    bool bgmGroupReady_ = false;
    bool seGroupReady_ = false;

    Voice bgm_;                  ///< BGM は単一スロット（差し替え再生）
    std::vector<Voice> seVoices_; ///< 撃ちっぱなし SE。Update() で終了分を回収
};

} // namespace

std::expected<std::unique_ptr<IAudio>, std::string> CreateAudioEngine() {
    auto audio = std::make_unique<MiniaudioAudio>();
    if (auto result = audio->Init(); !result)
        return std::unexpected(result.error());
    return std::unique_ptr<IAudio>(std::move(audio));
}

} // namespace witch::audio
