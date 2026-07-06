#pragma once
#include "WitchEngine/Scene/UpdatePhase.h"
#include <array>
#include <vector>

namespace witch {

class Component;
class GameObject;

/// Component をフェーズ別リストで管理し、UpdatePhase の宣言順に更新するスケジューラ。
/// Scene が 1 つ所有し、Scene::Update のフェーズ実行ステージから駆動される。
/// Component の追加順に依存しないフェーズ間の順序保証（Animation → Render 等）を提供する。
///
/// 【並列化契約】同一フェーズ内での GameObject 間の実行順は**未規定**。
/// 順序が必要なら別フェーズに分けること。現実装は登録順の逐次実行だが、
/// これに依存したコードを書いてはならない（将来フェーズ単位で並列化するため）。
///
/// 【ライフタイム】保持するのは非所有ポインタ（所有は GameObject → unique_ptr）。
/// 除去は Scene::Update の破棄回収ステージからの UnregisterAll のみ。
/// ~Scene 経路（シーン破棄）ではスケジューラも一緒に破棄され以後走らないため、
/// dangling を掃除する必要はなく、デストラクタから触ってはならない。
class ComponentScheduler {
public:
    /// 保留リストに積むだけ。フェーズリストへの反映は各フェーズ実行直前に行う
    /// （フェーズリスト走査中の push_back による無効化を避ける）。
    void Register(Component* component);

    /// obj が所有する全 Component をフェーズリストと保留リストから除去する。
    /// Scene::Update の破棄回収ステージから、OnDespawn より前に呼ぶ。
    void UnregisterAll(GameObject* obj);

    /// 保留を反映してから指定フェーズの全 Component を更新する。
    /// Owner()->IsDestroyed() の Component はスキップする。
    /// 反映を実行直前に行うため、フェーズ X の最中に AddComponent された Component も
    /// 同一フレームの X より後のフェーズで走る（Update 中に足した Sprite が
    /// 同フレームの Render で提出され、1 フレーム消える現象を防ぐ）。
    /// ただし PreUpdate はそのフレームで最初に実行されるフェーズのため対象外:
    /// GameObject::Update フックや Update 以降で Phase()==PreUpdate な Component を
    /// 追加しても、その Component は次フレームの PreUpdate まで実行されない。
    void RunPhase(UpdatePhase phase, float dt);

private:
    /// pendingAdd_ を Phase() 別のフェーズリストへ振り分ける。
    void FlushPending();

    std::array<std::vector<Component*>, kUpdatePhaseCount> phases_;
    std::vector<Component*> pendingAdd_;
};

} // namespace witch
