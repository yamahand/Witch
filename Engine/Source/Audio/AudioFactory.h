#pragma once

#include <expected>
#include <memory>
#include <string>

namespace witch::audio {

class IAudio;

/// miniaudio 具象を生成しデバイスを初期化する。miniaudio の型・ヘッダは
/// MiniaudioAudio.cpp の 1 TU に閉じ込める（Rhi/D3D12 と同じ境界隔離）。
/// 失敗時はエラーメッセージを返す。呼び出し側（Engine::Init）は警告ログに落として
/// 無音で続行する＝非致命（Services::Instance().audio は nullptr のまま）。
std::expected<std::unique_ptr<IAudio>, std::string> CreateAudioEngine();

} // namespace witch::audio
