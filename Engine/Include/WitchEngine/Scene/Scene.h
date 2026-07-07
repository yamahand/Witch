#pragma once
#include "WitchEngine/Scene/ComponentScheduler.h"
#include "WitchEngine/Scene/DebugUI.h"
#include "WitchEngine/Scene/GameObject.h"
#include <memory>
#include <string_view>
#include <vector>
#ifdef WITCH_DEBUG_UI
#include <span>
#endif

namespace witch {

/// オブジェクト所有ツリーの根。1 シーン = 1 画面を表す。
/// DrawDebugUI() は DebugUI 基底から継承する（WITCH_DEBUG_UI 定義時のみ存在）。
class Scene : public DebugUI {
public:
    virtual ~Scene() = default;

    /// シーンがアクティブになる直前に呼ばれる。
    virtual void OnEnter() {}
    /// シーンが非アクティブになる直前に呼ばれる。
    virtual void OnExit() {}

    /// 生成反映 → フェーズ実行 → 破棄回収 の 3 段階で更新する。順序は厳守。
    /// フェーズ実行は ComponentScheduler が UpdatePhase の宣言順
    /// （PreUpdate → Update → PostUpdate → Animation → Camera → Render）に回す。
    /// GameObject::Update フックは Update フェーズの Component より前に呼ばれる。
    /// サブクラスがオーバーライドする場合は必ず Scene::Update(dt) を呼ぶ。
    virtual void Update(float dt);

#ifdef WITCH_DEBUG_UI
    /// 生存オブジェクトの DrawDebugUI（毎フレーム自由描画フック）を呼ぶ。
    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）に呼ぶこと。
    /// インスペクター表示（DrawInspector）はここではなく HierarchyWindow が
    /// 選択中オブジェクトに対してのみ行う。
    void DrawDebugUI() override;

    /// HierarchyWindow が GameObject を列挙するための読み取り専用ビュー。
    /// pendingSpawn_（未反映）は含まない。
    std::span<const std::unique_ptr<GameObject>> DebugObjects() const { return objects_; }
#endif

    /// 更新中に呼んでも安全。保留リストに積み、次の生成フェーズで反映する。
    template<typename T, typename... Args>
    T* Spawn(Args&&... args);

    /// ObjectId で線形探索する。現状 O(n) で許容している（最適化は必要になってから）。
    /// 見つからなければ nullptr を返す。
    GameObject* Find(ObjectId id) const;

    /// ObjectRegistry 経由でレベルファイルを実体化する（M6 実装予定）。
    void LoadLevel(std::string_view path);

private:
    /// GameObject::RegisterComponent（スポーン後の AddComponent）が scheduler_ に触るため。
    friend class GameObject;

    static ObjectId NextId();

    ComponentScheduler scheduler_;
    std::vector<std::unique_ptr<GameObject>> objects_;
    std::vector<std::unique_ptr<GameObject>> pendingSpawn_;
};

// ── Template implementation ──────────────────────────────────────────────────

template<typename T, typename... Args>
T* Scene::Spawn(Args&&... args) {
    static_assert(std::is_base_of_v<GameObject, T>,
                  "T must derive from witch::GameObject");
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    obj->id_ = NextId();
    obj->scene_ = this;
    T* ptr = obj.get();
    pendingSpawn_.push_back(std::move(obj));
    return ptr;
}

} // namespace witch
