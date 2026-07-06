#pragma once
#include "WitchEngine/Core/Log/Category.h"
#include "WitchEngine/Core/Log/LogLevel.h"
#include <concepts>
#include <format>
#include <memory>
#include <source_location>
#include <string_view>
#include <type_traits>

namespace witch::log {

class ILogSink;

/// ログサービス本体。Engine が全サービスの先頭で生成し、Shutdown で最後に破棄する
/// （他サービスが自身の初期化・終了処理中にログを出せるようにするため）。
/// 呼び出し側は通常このクラスを直接触らず、witch::log::Info() 等のファサードを使う。
///
/// スレッド安全: 全公開メソッドは内部 mutex で保護される。ロックは実装（pimpl）内部に
/// 完全に閉じているため、将来 LockFree 化しても公開 API・呼び出し側は一切変わらない。
class Logger {
public:
    struct Config {
        /// TextBuffer 1 面あたりの容量。ダブルバッファで 2 面確保される。
        /// 毎フレーム Flush される前提では 1 フレーム分のログしか溜まらないため、
        /// 既定は控えめにしてある（逼迫時は Log() 内から強制 Flush されるので安全）。
        size_t textBufferCapacity = 8u * 1024 * 1024;
        size_t recordCapacity = 10'000; ///< 未フラッシュ LogRecord の上限。超えると強制 Flush
    };

    explicit Logger(const Config& config = {});
    ~Logger(); ///< 破棄時に未フラッシュ分を Flush する

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /// 全ログの唯一の入口。ファサード関数はすべてここへ集約される。
    void Log(LogLevel level, Category category, std::string_view message,
             const std::source_location& loc);

    /// Sink を追加する（所有権を取る）。Engine::Init での構成を想定。
    void AddSink(std::unique_ptr<ILogSink> sink);

    /// このレベル未満のログを全 Sink 共通で入口で捨てる。既定 Trace（全部通す）。
    void SetGlobalLevel(LogLevel level);

    /// LogRecord に載せるフレーム番号を更新する。Engine が毎フレーム呼ぶ。
    void SetFrameNumber(uint64_t frame);

    /// Deferred Sink へ溜まったログを書き出し、TextBuffer をスワップする。
    /// Engine が毎フレーム末に呼ぶ（容量逼迫時は Log() 内部からも呼ばれる）。
    void Flush();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ── ファサード ───────────────────────────────────────────────────────────────
// Services に登録された Logger へ転送する自由関数群。Logger 未登録の間
// （Engine::Init 前後）は従来同様 stdout + デバッガ出力へフォールバックする。

/// std::format 書式と呼び出し元 source_location を同時に受け取るための補助型。
/// 可変長引数テンプレートの後ろにはデフォルト引数を置けないため、
/// 「第 1 引数の暗黙変換（consteval コンストラクタ）が呼び出し元の位置で評価される」
/// ことを利用して source_location を捕捉する。
template<typename... Args>
struct FormatWithLocation {
    std::format_string<Args...> fmt;
    std::source_location loc;

    template<typename S>
        requires std::convertible_to<const S&, std::string_view>
    consteval FormatWithLocation(const S& s,
                                 std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
};

void Log(LogLevel level, Category category, std::string_view msg,
         std::source_location loc = std::source_location::current());

void Info(std::string_view msg, std::source_location loc = std::source_location::current());
void Warn(std::string_view msg, std::source_location loc = std::source_location::current());
void Error(std::string_view msg, std::source_location loc = std::source_location::current());

void Info(Category category, std::string_view msg,
          std::source_location loc = std::source_location::current());
void Warn(Category category, std::string_view msg,
          std::source_location loc = std::source_location::current());
void Error(Category category, std::string_view msg,
           std::source_location loc = std::source_location::current());

// 書式付きオーバーロード。std::type_identity_t で第 1 引数からの型推論を止め、
// Args は後続引数からのみ推論させる（std::format と同じ構え）。
template<typename... Args>
void Info(FormatWithLocation<std::type_identity_t<Args>...> fmt, Args&&... args) {
    Info(std::format(fmt.fmt, std::forward<Args>(args)...), fmt.loc);
}

template<typename... Args>
void Warn(FormatWithLocation<std::type_identity_t<Args>...> fmt, Args&&... args) {
    Warn(std::format(fmt.fmt, std::forward<Args>(args)...), fmt.loc);
}

template<typename... Args>
void Error(FormatWithLocation<std::type_identity_t<Args>...> fmt, Args&&... args) {
    Error(std::format(fmt.fmt, std::forward<Args>(args)...), fmt.loc);
}

template<typename... Args>
void Info(Category category, FormatWithLocation<std::type_identity_t<Args>...> fmt,
          Args&&... args) {
    Info(category, std::format(fmt.fmt, std::forward<Args>(args)...), fmt.loc);
}

template<typename... Args>
void Warn(Category category, FormatWithLocation<std::type_identity_t<Args>...> fmt,
          Args&&... args) {
    Warn(category, std::format(fmt.fmt, std::forward<Args>(args)...), fmt.loc);
}

template<typename... Args>
void Error(Category category, FormatWithLocation<std::type_identity_t<Args>...> fmt,
           Args&&... args) {
    Error(category, std::format(fmt.fmt, std::forward<Args>(args)...), fmt.loc);
}

} // namespace witch::log
