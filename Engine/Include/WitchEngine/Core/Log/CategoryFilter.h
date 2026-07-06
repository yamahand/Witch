#pragma once
#include "WitchEngine/Core/Log/Category.h"
#include "WitchEngine/Core/Log/ILogFilter.h"
#include "WitchEngine/Core/Log/LogView.h"
#include <initializer_list>
#include <unordered_set>

namespace witch::log {

/// カテゴリハッシュの許可/拒否リストで判定するフィルタ。文字列比較は行わない。
class CategoryFilter final : public ILogFilter {
public:
    enum class Mode : uint8_t {
        Allow, ///< リストに含まれるカテゴリのみ通す
        Deny,  ///< リストに含まれるカテゴリを弾く
    };

    CategoryFilter(Mode mode, std::initializer_list<Category> categories) : mode_(mode) {
        for (const auto& category : categories) {
            hashes_.insert(category.hash);
        }
    }

    [[nodiscard]] bool Accept(const LogView& view) const override {
        const bool contains = hashes_.contains(view.categoryHash);
        return mode_ == Mode::Allow ? contains : !contains;
    }

private:
    Mode mode_;
    std::unordered_set<uint64_t> hashes_;
};

} // namespace witch::log
