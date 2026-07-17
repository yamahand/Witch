#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace witch::audio {

/// PlayBgm のパラメータ。
struct BgmParams {
    float volume = 1.0f;
    bool loop = true;
    /// ループ折返し先（秒）。設定時は末尾到達後この位置へ戻る（イントロ付きループ）。
    /// 未設定なら曲頭へ戻る通常ループ。loop == false のときは無視される。
    std::optional<double> loopBeginSeconds;
};

/// オーディオサービス。Services::Instance().audio で取得する。
/// デバイス初期化に失敗した環境ではスロットが nullptr のまま（無音で続行）なので、
/// 呼び出し側は他サービスと同様に null チェックする。
/// 全メソッドはメインスレッドからのみ呼ぶこと（ミキシングはバックエンドの
/// オーディオスレッドで行われるが、API を呼ぶスレッドは 1 本に限定する）。
class IAudio {
public:
    virtual ~IAudio() = default;

    /// SE を撃ちっぱなしで再生する（同一 SE の多重再生可）。
    /// 失敗はログに出して無視する（ゲームロジックが SE 失敗を扱う必要はない）。
    /// @param path VFS マウントルートからの相対パス（例 "Audio/SE/click1.ogg"）
    virtual void PlaySe(std::string_view path, float volume = 1.0f) = 0;

    /// BGM を再生する（再生中の BGM は停止して差し替え）。逐次デコード＝ストリーミング相当。
    /// @param path VFS マウントルートからの相対パス（例 "Audio/BGM/Retro Beat.ogg"）
    virtual std::expected<void, std::string> PlayBgm(std::string_view path,
                                                     const BgmParams& params = {}) = 0;

    virtual void StopBgm() = 0;
    [[nodiscard]] virtual bool IsBgmPlaying() const = 0;

    virtual void SetMasterVolume(float volume) = 0;
    virtual void SetBgmVolume(float volume) = 0; ///< BGM バス
    virtual void SetSeVolume(float volume) = 0;  ///< SE バス

    /// 再生終了したボイスの回収。GameLoop が毎フレーム呼ぶ。
    virtual void Update() = 0;
};

} // namespace witch::audio
