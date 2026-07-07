#pragma once

namespace witch::log {

struct LogView;

/// ログ 1 件を通すかどうかを判定するフィルタ。
/// 複数スレッドから同時に呼ばれ得るため、実装は構築後不変（ステートレス）にすること。
class ILogFilter {
public:
    virtual ~ILogFilter() = default;
    [[nodiscard]] virtual bool Accept(const LogView& view) const = 0;
};

} // namespace witch::log
