#pragma once
#include "WitchEngine/Scene/GameObject.h"
#include <memory>
#include <string_view>
#include <vector>

namespace witch {

/// オブジェクト所有ツリーの根。1 シーン = 1 画面を表す。
class Scene {
public:
    virtual ~Scene() = default;

    /// シーンがアクティブになる直前に呼ばれる。
    virtual void OnEnter() {}
    /// シーンが非アクティブになる直前に呼ばれる。
    virtual void OnExit() {}

    /// 生成反映 → 全更新 → 破棄回収 の 3 段階で更新する。順序は厳守。
    /// サブクラスがオーバーライドする場合は必ず Scene::Update(dt) を呼ぶ。
    virtual void Update(float dt);

    /// 生存オブジェクトの DrawDebugUI を呼ぶ。ImGui フレーム内（BeginDebugUI 後・
    /// RenderDebugUI 前）に呼ぶこと。
    virtual void DrawDebugUI();

    /// 更新中に呼んでも安全。保留リストに積み、次の生成フェーズで反映する。
    template<typename T, typename... Args>
    T* Spawn(Args&&... args);

    /// ObjectId で線形探索する。現状 O(n) で許容している（最適化は必要になってから）。
    /// 見つからなければ nullptr を返す。
    GameObject* Find(ObjectId id) const;

    /// ObjectRegistry 経由でレベルファイルを実体化する（M6 実装予定）。
    void LoadLevel(std::string_view path);

private:
    static ObjectId NextId();

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
