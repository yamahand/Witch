#pragma once
#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/DebugMenu.h"
#include <string>
#include <utility>

namespace witch::debug {

/// DebugMenu の項目を所有者の寿命に紐付ける RAII ヘルパー。
/// コンストラクタで AddItem し、デストラクタで RemoveItem する。
/// GameObject / Scene / Component のメンバとして持てば、所有者が消えるときに
/// メニュー項目も自動で消え、手動の RemoveItem 忘れを防げる。
///
/// DebugMenu（Engine 所有）より長生きしないこと。Engine::Shutdown はシーンを
/// デバッグ UI より先に破棄するため、シーン所有物のメンバなら自然に満たされる。
/// WITCH_DEBUG_UI 定義時のみ存在し、OFF ビルドではヘッダごとビルドから外れる。
class DebugMenuItem {
public:
    /// 未登録状態。後からムーブ代入で差し替える用途。
    DebugMenuItem() = default;

    /// menu に path で項目を登録する。menu が null、または AddItem が path を
    /// 不正として拒否した場合は未登録状態になる（デストラクタは何もしない）。
    DebugMenuItem(DebugMenu* menu, std::string path, DebugMenu::Callback callback)
        : path_(std::move(path)) {
        if (menu && menu->AddItem(path_, std::move(callback))) {
            menu_ = menu;
        }
    }

    ~DebugMenuItem() { Reset(); }

    DebugMenuItem(const DebugMenuItem&) = delete;
    DebugMenuItem& operator=(const DebugMenuItem&) = delete;

    DebugMenuItem(DebugMenuItem&& other) noexcept
        : menu_(std::exchange(other.menu_, nullptr)), path_(std::move(other.path_)) {}

    DebugMenuItem& operator=(DebugMenuItem&& other) noexcept {
        if (this != &other) {
            Reset();
            menu_ = std::exchange(other.menu_, nullptr);
            path_ = std::move(other.path_);
        }
        return *this;
    }

    /// 登録を明示的に解除する（未登録なら何もしない）。
    void Reset() {
        if (menu_) {
            menu_->RemoveItem(path_);
            menu_ = nullptr;
        }
    }

    /// 登録済みなら true。
    bool IsRegistered() const { return menu_ != nullptr; }

private:
    DebugMenu* menu_ = nullptr; ///< 登録済みのときのみ非 null（非所有）。
    std::string path_;
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
