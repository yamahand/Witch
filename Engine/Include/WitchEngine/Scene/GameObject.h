#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/DebugUI.h"
#include "WitchEngine/Scene/Transform.h"
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#ifdef WITCH_DEBUG_UI
#include <span>
#endif

namespace witch {

class Scene;

/// シーン内でオブジェクトを弱参照するための識別子。
using ObjectId = uint64_t;
/// 未設定を示す番兵値。
static constexpr ObjectId kInvalidId = 0;

/// ゲームに登場するすべての実体の基底。振る舞いは Component に分解する。
/// サブクラスは「種別」だけを表し、継承を縦横に広げない。
/// DrawDebugUI() は DebugUI 基底から継承する（WITCH_DEBUG_UI 定義時のみ存在）。
class GameObject : public DebugUI {
public:
    virtual ~GameObject();

    /// スポーン直後に呼ばれる。サブクラスはここに初期化処理を書く。
    virtual void OnSpawn() {}
    /// 毎フレーム Update フェーズの先頭で呼ばれるオブジェクト単位のフック。既定は空。
    /// Component の更新はここでは行わない（Scene の ComponentScheduler がフェーズ順に回す）。
    virtual void Update(float dt);
    /// 破棄直前に呼ばれる。
    virtual void OnDespawn() {}

    /// コンポーネントを生成して所有し、OnAttach を呼ぶ。
    /// @tparam T Component の派生クラス
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    /// 型 T の最初のコンポーネントを線形探索で返す。見つからなければ nullptr。
    template<typename T>
    T* GetComponent() const;

    /// 即時 delete せず末尾フラグを立てるだけ。
    /// 更新中の自己破棄でイテレータが壊れないよう、Scene がフレーム末に回収する。
    void Destroy() { pendingDestroy_ = true; }
    bool IsDestroyed() const { return pendingDestroy_; }

    ObjectId Id() const { return id_; }
    Scene* GetScene() const { return scene_; }

    /// デバッグ表示・レベルエディタ連携用の名前。未設定（空）を許す。
    /// ヒエラルキーウィンドウは空のとき型名にフォールバックして表示する。
    void SetName(std::string name) { name_ = std::move(name); }
    const std::string& Name() const { return name_; }

#ifdef WITCH_DEBUG_UI
    /// HierarchyWindow が Component を列挙するための読み取り専用ビュー。
    std::span<const std::unique_ptr<Component>> DebugComponents() const { return components_; }
#endif

    Transform transform;

private:
    friend class Scene;

    /// AddComponent から呼ぶ非テンプレートヘルパ。spawned_ が立っている（= 生成反映済み）
    /// 場合のみ Scene の ComponentScheduler へ登録する。未スポーン時（コンストラクタや
    /// OnSpawn 中の AddComponent）は Scene の生成反映ステージが components_ を一括登録
    /// するため、ここでは登録しない（二重登録の防止）。実装は GameObject.cpp
    /// （テンプレートヘッダに Scene.h への依存を持ち込まないため）。
    void RegisterComponent(Component* component);

    ObjectId id_ = kInvalidId;
    Scene* scene_ = nullptr;
    std::string name_;
    bool pendingDestroy_ = false;
    bool spawned_ = false;  ///< Scene の生成反映ステージ通過済みか（Scene が立てる）。
    std::vector<std::unique_ptr<Component>> components_;
};

// ── Template implementations ────────────────────────────────────────────────

template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of_v<Component, T>,
                  "T must derive from witch::Component");
    auto comp = std::make_unique<T>(std::forward<Args>(args)...);
    comp->owner_ = this;
    comp->OnAttach();
    T* ptr = comp.get();
    components_.push_back(std::move(comp));
    RegisterComponent(ptr);
    return ptr;
}

template<typename T>
T* GameObject::GetComponent() const {
    static_assert(std::is_base_of_v<Component, T>,
                  "T must derive from witch::Component");
    // マクロ付け忘れ検出: WITCH_COMPONENT が無いと T::StaticTypeId は Component の
    // ものにフォールバックし、id が基底 ID になって無関係な型に誤マッチ→不正な
    // static_cast（未定義動作）になる。旧 dynamic_cast は安全に nullptr を返していた
    // ため、この後退をコンパイル時に弾く。
    static_assert(kHasComponentTypeId<T>,
                  "GetComponent<T>: T is missing WITCH_COMPONENT(T, Base) in its class body");
    // IsA は「T そのもの or T の派生」で真になるため、基底型での取得も成立する
    // （dynamic_cast の階層探索・RTTI を使わず、型 ID の比較だけで判定する）。
    const ComponentTypeId id = T::StaticTypeId();
    for (const auto& comp : components_) {
        if (comp->IsA(id)) {
            return static_cast<T*>(comp.get());
        }
    }
    return nullptr;
}

} // namespace witch
