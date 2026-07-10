#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/ObjectRegistry.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Graphics2D/TilemapComponent.h"
#include "WitchEngine/Vfs/Vfs.h"
#include "Level/LdtkLoader.h"
#include <algorithm>
#include <atomic>
#include <format>
#include <utility>

namespace witch {

namespace {
std::atomic<ObjectId> sNextId{1};

/// タイルマップのルート GameObject に割り当てる描画レイヤーの基点。
/// 奥のレイヤーから kTilemapBaseLayer + i を割り当てるため、ゲームスプライトの
/// 既定 layer 0 より必ず奥に描かれる（レイヤー数が 100 を超える想定はしない）。
constexpr int16_t kTilemapBaseLayer = -100;
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

GameObject* Scene::AdoptSpawn(std::unique_ptr<GameObject> obj) {
    obj->id_ = NextId();
    obj->scene_ = this;
    GameObject* ptr = obj.get();
    if (inEnter_) {
        // OnEnter 中は更新イテレーションが走っていないため即時反映できる。
        // OnSpawn 内の入れ子 Spawn はここへ同期再帰する（イテレーション無しで安全）。
        CommitSpawn(std::move(obj));
    } else {
        pendingSpawn_.push_back(std::move(obj));
    }
    return ptr;
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

void Scene::DestroyLevelObjects() {
    for (ObjectId id : levelObjectIds_) {
        if (GameObject* obj = Find(id)) {
            obj->Destroy();
            continue;
        }
        // 直前の LoadLevel が更新外（保留モード）だった場合はまだ pendingSpawn_ に
        // いる。Destroy フラグだけ立てておけば、反映（OnSpawn）後にフレーム末で
        // 回収される（遅延破棄の通常契約に合流する）。
        for (auto& pending : pendingSpawn_) {
            if (pending->Id() == id) {
                pending->Destroy();
                break;
            }
        }
    }
    levelObjectIds_.clear();
}

GameObject* Scene::Find(ObjectId id) const {
    for (const auto& obj : objects_) {
        if (obj->Id() == id)
            return obj.get();
    }
    return nullptr;
}

std::expected<void, std::string> Scene::LoadLevel(std::string_view path) {
    auto* vfs = Services::Instance().vfs;
    if (!vfs) {
        return std::unexpected("Scene::LoadLevel: VFS service is not available");
    }
    auto file = vfs->Read(path);
    if (!file) {
        return std::unexpected(file.error());
    }

    // 拡張子で形式を選ぶ。現状 .ldtk のみ（Tiled 対応時はここに分岐を足す）。
    const std::string ext = vfs::Vfs::Extension(path);
    if (ext != ".ldtk") {
        return std::unexpected(
            std::format("Scene::LoadLevel: unsupported level format '{}' ({})", ext, path));
    }
    auto parsed = ldtk::ParseLdtk(file->bytes, path);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    auto level = std::make_unique<LevelData>(std::move(*parsed));

    // 再呼び出し時: 前回のレベル由来オブジェクトを破棄してから新レベルを生成する
    // （level_ だけ差し替わって描画・実体が新旧混在するのを防ぐ）。
    DestroyLevelObjects();

    SetClearColor(level->bgColor);

    // タイルレイヤーを 1 つのルート GameObject へまとめて載せる（ヒエラルキーを
    // 散らかさない）。テクスチャロードに失敗したレイヤーは警告してスキップする。
    if (!level->tileLayers.empty()) {
        auto* root = Spawn<GameObject>();
        root->SetName(level->identifier);
        levelObjectIds_.push_back(root->Id());
        auto* resources = Services::Instance().resources;
        for (size_t i = 0; i < level->tileLayers.size(); ++i) {
            const LevelTileLayer& layer = level->tileLayers[i];
            if (!resources) {
                log::Warn("Scene::LoadLevel: ResourceManager not available — tile layer "
                          "'{}' skipped",
                          layer.identifier);
                continue;
            }
            auto texture = resources->LoadTexture(layer.tilesetPath);
            if (!texture) {
                log::Warn("Scene::LoadLevel: failed to load tileset '{}': {} — tile "
                          "layer '{}' skipped",
                          layer.tilesetPath, texture.error(), layer.identifier);
                continue;
            }
            auto* tilemap = root->AddComponent<TilemapComponent>(*texture, layer);
            // 奥のレイヤーから順に割り当て、ゲームスプライト（layer 0）より奥に描く。
            tilemap->SetLayer(static_cast<int16_t>(kTilemapBaseLayer + static_cast<int>(i)));
        }
    }

    // エンティティは ObjectRegistry で実体化する。transform / Name の設定は
    // AdoptSpawn（= OnSpawn）より前に行い、即時/遅延どちらの反映モードでも
    // OnSpawn が見る状態を同じにする（GameObject.h の自己完結契約と両立）。
    for (const LevelEntity& entity : level->entities) {
        auto obj = ObjectRegistry::Instance().Create(entity.identifier);
        if (!obj) {
            log::Warn("Scene::LoadLevel: entity type '{}' is not registered — skipped",
                      entity.identifier);
            continue;
        }
        obj->transform.x = entity.x;
        obj->transform.y = entity.y;
        obj->SetName(entity.identifier);
        levelObjectIds_.push_back(AdoptSpawn(std::move(obj))->Id());
    }

    level_ = std::move(level);
    log::Info("Scene::LoadLevel: '{}' loaded ({} tile layers, {} entities)",
              level_->identifier, level_->tileLayers.size(), level_->entities.size());
    return {};
}

} // namespace witch
