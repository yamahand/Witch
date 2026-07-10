#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Core/Logger.h"
#include <algorithm>
#include <atomic>

namespace witch {

namespace {
std::atomic<ObjectId> sNextId{1};
} // namespace

ObjectId Scene::NextId() {
    return sNextId.fetch_add(1, std::memory_order_relaxed);
}

void Scene::Enter() {
    // inEnter_ の復帰は RAII で保証する。OnEnter から呼ばれる OSS アダプタ層に
    // 例外→expected の翻訳漏れがあり例外がここまで伝播した場合でも、即時反映モードが
    // 立ちっぱなしになるのを防ぐ（残ると以降の更新中 Spawn が objects_ の
    // イテレーション中 push_back = UB に化けるため、手動復帰にしない）。
    struct EnterGuard {
        Scene& scene;
        ~EnterGuard() { scene.inEnter_ = false; }
    } guard{*this};
    inEnter_ = true;
    OnEnter();
}

void Scene::Exit() {
    OnExit();
}

void Scene::CommitSpawn(std::unique_ptr<GameObject> obj) {
    // OnSpawn 完了後に spawned_ を立てて components_ を一括登録する。OnSpawn 中の
    // AddComponent は spawned_ が false なので個別登録されず、ここで漏れなく拾われる
    // （spawned_ 以降の AddComponent は GameObject::RegisterComponent が個別登録する）。
    obj->OnSpawn();
    obj->spawned_ = true;
    for (auto& comp : obj->components_) {
        scheduler_.Register(comp.get());
    }
    objects_.push_back(std::move(obj));
}

void Scene::FlushPendingSpawns() {
    // Swap to local first: OnSpawn() may call Spawn(), which pushes to pendingSpawn_.
    // Iterating and push_back-ing the same vector causes reallocation UB.
    auto spawning = std::move(pendingSpawn_);
    for (auto& obj : spawning) {
        CommitSpawn(std::move(obj));
    }
}

void Scene::CollectDestroyed() {
    // スケジューラからの除去は delete（erase_if）より前に行い、dangling を残さない。
    for (auto& obj : objects_) {
        if (obj->IsDestroyed()) {
            scheduler_.UnregisterAll(obj.get());
            obj->OnDespawn();
        }
    }
    std::erase_if(objects_, [](const std::unique_ptr<GameObject>& o) {
        return o->IsDestroyed();
    });
}

void Scene::FixedUpdate(float fixedDt) {
    // 生成反映 → 固定側フェーズ。ステップ中の Spawn は次の FixedUpdate または
    // 同フレームの FrameUpdate（先に来る方）で反映される。
    FlushPendingSpawns();

    // GameObject::Update フックは Update フェーズの Component より前に呼ぶ。
    scheduler_.RunPhase(UpdatePhase::PreUpdate, fixedDt);
    for (auto& obj : objects_) {
        if (!obj->IsDestroyed()) {
            obj->Update(fixedDt);
        }
    }
    scheduler_.RunPhase(UpdatePhase::Update, fixedDt);
    scheduler_.RunPhase(UpdatePhase::PostUpdate, fixedDt);
}

void Scene::FrameUpdate(float dt) {
    // 生成反映を再度行う: 固定ステップ 0 回のフレームでも Spawn 済みオブジェクトが
    // 同フレームの描画（Render フェーズ）に乗ることを保証する。
    FlushPendingSpawns();

    scheduler_.RunPhase(UpdatePhase::Animation, dt);
    scheduler_.RunPhase(UpdatePhase::Camera, dt);
    scheduler_.RunPhase(UpdatePhase::Render, dt);

    // 破棄回収はフレーム末。固定ステップ中に Destroy されたオブジェクトは
    // 同フレームの残りフェーズをスキップされ、ここで回収される。
    CollectDestroyed();
}

#ifdef WITCH_DEBUG_UI
void Scene::DrawDebugUI() {
    for (auto& obj : objects_) {
        if (!obj->IsDestroyed()) obj->DrawDebugUI();
    }
}
#endif

GameObject* Scene::Find(ObjectId id) const {
    for (const auto& obj : objects_) {
        if (obj->Id() == id)
            return obj.get();
    }
    return nullptr;
}

void Scene::LoadLevel(std::string_view /*path*/) {
    log::Warn("Scene::LoadLevel not implemented yet (M6).");
}

} // namespace witch
