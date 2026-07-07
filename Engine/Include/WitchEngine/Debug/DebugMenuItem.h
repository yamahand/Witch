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
/// 削除は AddItem が返した id で行うため、同一 path の上書き登録があっても
/// 「自分が登録した項目」以外を誤って消すことはない。
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
    DebugMenuItem(DebugMenu* menu, std::string path, DebugMenu::Callback callback) {
        if (menu) {
            id_ = menu->AddItem(std::move(path), std::move(callback));
            if (id_ != DebugMenu::kInvalidItemId) {
                menu_ = menu;
            }
        }
    }

    ~DebugMenuItem() { Reset(); }

    DebugMenuItem(const DebugMenuItem&) = delete;
    DebugMenuItem& operator=(const DebugMenuItem&) = delete;

    DebugMenuItem(DebugMenuItem&& other) noexcept
        : menu_(std::exchange(other.menu_, nullptr)),
          id_(std::exchange(other.id_, DebugMenu::kInvalidItemId)) {}

    DebugMenuItem& operator=(DebugMenuItem&& other) noexcept {
        if (this != &other) {
            Reset();
            menu_ = std::exchange(other.menu_, nullptr);
            id_ = std::exchange(other.id_, DebugMenu::kInvalidItemId);
        }
        return *this;
    }

    /// 登録を明示的に解除する（未登録なら何もしない）。
    void Reset() {
        if (menu_) {
            menu_->RemoveItem(id_);
            menu_ = nullptr;
            id_ = DebugMenu::kInvalidItemId;
        }
    }

    /// 登録済みなら true。
    bool IsRegistered() const { return menu_ != nullptr; }

private:
    DebugMenu* menu_ = nullptr;                        ///< 登録済みのときのみ非 null（非所有）。
    DebugMenu::ItemId id_ = DebugMenu::kInvalidItemId; ///< 登録した項目のハンドル。
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
