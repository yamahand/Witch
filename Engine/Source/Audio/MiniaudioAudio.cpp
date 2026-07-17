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

    Voice() = default;
    Voice(Voice&&) noexcept = default; ///< move 元は空になる（unique_ptr/shared_ptr に従う）
    /// 代入先が再生中でも安全なよう、先に自分を解放してから受け取る
    /// （BGM 差し替えや erase_if の詰め直しで再生中スロットへ move されるため）。
    Voice& operator=(Voice&& other) noexcept {
        if (this != &other) {
            Release();
            clip = std::move(other.clip);
            decoder = std::move(other.decoder);
            sound = std::move(other.sound);
        }
        return *this;
    }
    /// どの経路（早期 return・例外・コンテナ破棄）でも ma_*_uninit を必ず経由させる。
    ~Voice() { Release(); }

    /// sound → decoder → clip の順で解放する（冪等）。ma_sound_uninit がノードグラフ
    /// から切り離した後は、オーディオスレッドが decoder に触ることはない。
    void Release() {
        if (sound) ma_sound_uninit(sound.get());
        sound.reset();
        if (decoder) ma_decoder_uninit(decoder.get());
        decoder.reset();
        clip.reset();
    }

    explicit operator bool() const { return static_cast<bool>(sound); }
};

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
        // メンバのデストラクタは本体（この関数）より後に走るため、エンジンを
        // uninit する前にここで明示的に解放しておく必要がある。
        // ma_engine_uninit がデバイススレッドを停止して join する。
        bgm_.Release();
        seVoices_.clear();
        if (seGroupReady_) ma_sound_group_uninit(&seGroup_);
        if (bgmGroupReady_) ma_sound_group_uninit(&bgmGroup_);
        if (engineReady_) ma_engine_uninit(&engine_);
    }

    void PlaySe(std::string_view path, float volume) override {
        auto voice = MakeVoice(path, &seGroup_);
        if (!voice) {
            log::Warn("PlaySe failed: {}", voice.error());
            return;
        }
        ma_sound_set_volume(voice->sound.get(), volume);
        if (ma_result r = ma_sound_start(voice->sound.get()); r != MA_SUCCESS) {
            log::Warn("PlaySe: ma_sound_start failed: {} ({})", ma_result_description(r), path);
            return; // voice は ~Voice が解放する
        }
        seVoices_.push_back(std::move(*voice));
    }

    std::expected<void, std::string> PlayBgm(std::string_view path,
                                             const BgmParams& params) override {
        auto voice = MakeVoice(path, &bgmGroup_);
        if (!voice)
            return std::unexpected(voice.error());

        ma_sound_set_volume(voice->sound.get(), params.volume);
        if (params.loop) {
            ma_sound_set_looping(voice->sound.get(), MA_TRUE);
            if (params.loopBeginSeconds) {
                // 負値や NaN は ma_uint64 への変換で巨大なフレーム値になるため先に弾く。
                if (!(*params.loopBeginSeconds >= 0.0))
                    return std::unexpected(std::format(
                        "loopBeginSeconds must be >= 0 (got {}) ({})", *params.loopBeginSeconds,
                        path));
                // ループ折返し先をフレームに換算する。デコーダはエンジンのサンプルレートに
                // 固定して初期化しているので、換算もエンジンレートで行えばよい。
                // 終端は ~0 = データ末尾（末尾まで再生 → loopBegin へ戻るイントロ付きループ）。
                const auto loopBegin = static_cast<ma_uint64>(
                    *params.loopBeginSeconds * ma_engine_get_sample_rate(&engine_));
                if (ma_result r = ma_data_source_set_loop_point_in_pcm_frames(
                        voice->decoder.get(), loopBegin, ~static_cast<ma_uint64>(0));
                    r != MA_SUCCESS) {
                    return std::unexpected(
                        std::format("ma_data_source_set_loop_point_in_pcm_frames failed: {} ({})",
                                    ma_result_description(r), path));
                }
            }
        }

        if (ma_result r = ma_sound_start(voice->sound.get()); r != MA_SUCCESS) {
            return std::unexpected(
                std::format("ma_sound_start failed: {} ({})", ma_result_description(r), path));
        }

        // 新しい BGM の開始が成功してから旧 BGM を止める（失敗時に無音にしないため。
        // move 代入が代入先＝旧 BGM を先に解放する）。
        bgm_ = std::move(*voice);
        log::Info("BGM started: {} (volume={}, loop={}, loopBegin={}s)", path, params.volume,
                  params.loop, params.loopBeginSeconds.value_or(0.0));
        return {};
    }

    void StopBgm() override { bgm_.Release(); }

    [[nodiscard]] bool IsBgmPlaying() const override {
        return bgm_ && ma_sound_is_playing(bgm_.sound.get());
    }

    void SetMasterVolume(float volume) override { ma_engine_set_volume(&engine_, volume); }
    void SetBgmVolume(float volume) override { ma_sound_group_set_volume(&bgmGroup_, volume); }
    void SetSeVolume(float volume) override { ma_sound_group_set_volume(&seGroup_, volume); }

    void Update() override {
        // 再生し終えた SE ボイスの回収（解放は ~Voice / move 代入が行う）。
        // ma_sound_at_end はオーディオスレッドが立てるアトミックフラグの読み取りで、
        // メインスレッドから安全に呼べる。
        std::erase_if(seVoices_,
                      [](const Voice& voice) { return ma_sound_at_end(voice.sound.get()) != 0; });

        // 非ループ BGM が終わっていたらスロットを空ける。
        if (bgm_ && ma_sound_at_end(bgm_.sound.get()))
            bgm_.Release();
    }

private:
    /// クリップのロード（ResourceManager 経由・キャッシュ済みならヒット）から
    /// デコーダ生成・ノードグラフへの接続までを行う。開始（ma_sound_start）は呼び出し側。
    /// デコーダはエンジンの format / channels / sampleRate に固定して初期化する
    /// （ミックス時の変換を無くし、ループポイントのフレーム換算もエンジンレートで統一）。
    std::expected<Voice, std::string> MakeVoice(std::string_view path, ma_sound_group* group) {
        auto* resources = Services::Instance().resources;
        if (!resources)
            return std::unexpected(std::string("ResourceManager not available"));

        auto clip = resources->LoadAudio(path);
        if (!clip)
            return std::unexpected(clip.error());

        Voice voice;
        voice.clip = *clip;
        voice.decoder = std::make_unique<ma_decoder>();
        const ma_decoder_config config = ma_decoder_config_init(
            ma_format_f32, ma_engine_get_channels(&engine_), ma_engine_get_sample_rate(&engine_));
        if (ma_result r = ma_decoder_init_memory(voice.clip->encodedBytes.data(),
                                                 voice.clip->encodedBytes.size(), &config,
                                                 voice.decoder.get());
            r != MA_SUCCESS) {
            voice.decoder.reset(); // init 失敗した decoder を uninit しないよう外す
            return std::unexpected(
                std::format("ma_decoder_init_memory failed: {} ({})", ma_result_description(r), path));
        }

        voice.sound = std::make_unique<ma_sound>();
        if (ma_result r = ma_sound_init_from_data_source(&engine_, voice.decoder.get(), 0, group,
                                                         voice.sound.get());
            r != MA_SUCCESS) {
            voice.sound.reset(); // 同上（decoder 以降は ~Voice が解放する）
            return std::unexpected(std::format("ma_sound_init_from_data_source failed: {} ({})",
                                               ma_result_description(r), path));
        }
        return voice;
    }

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
