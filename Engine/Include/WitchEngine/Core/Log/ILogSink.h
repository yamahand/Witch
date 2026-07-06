#pragma once
#include "WitchEngine/Core/Log/LogView.h"

namespace witch::log {

/// Sink の出力タイミング。
enum class SinkMode : uint8_t {
    Immediate, ///< Logger::Log() の中で同期的に Write される
    Deferred,  ///< Logger::Flush() まで Write が遅延される
};

/// ログの出力先。Logger が unique_ptr で所有し、Write/Flush は Logger のロック下で
/// 直列に呼ばれる（Sink 自身のロックは、外部スレッドへ読み取り API を公開する場合のみ必要）。
class ILogSink {
public:
    virtual ~ILogSink() = default;

    [[nodiscard]] virtual SinkMode Mode() const = 0;

    /// ログ 1 件を受け取る。view 内の string_view はこの呼び出し中のみ有効。
    virtual void Write(const LogView& view) = 0;

    /// Deferred Sink が溜めた出力を実際に書き出す。Immediate Sink は既定の no-op で良い。
    virtual void Flush() {}
};

} // namespace witch::log
