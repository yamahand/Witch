// ComponentScheduler のフェーズ実行順の契約テスト。
// 仕様: UpdatePhase の宣言順（PreUpdate → Update → PostUpdate → Animation → Camera → Render）
// に実行し、GameObject::Update フックは Update フェーズの Component より前
// （Scene.h / ComponentScheduler.h / UpdatePhase.h のコメントが仕様）。
// 同一フェーズ内の GameObject 間の順序は契約上未規定のため、ここでは検証しない。
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Scene/UpdatePhase.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

using namespace witch;

using StepLog = std::vector<std::string>;

/// 固定ステップの dt。契約上 FixedUpdate には常にこの値を渡す（Time::kFixedDelta 相当）。
constexpr float kFixedDt = 1.0f / 60.0f;

/// 1 フレーム = 固定ステップ 1 回 + フレーム更新 1 回として回すヘルパ。
/// 固定側（PreUpdate → Hook → Update → PostUpdate）と毎フレーム側
/// （Animation → Camera → Render）を通しで実行する。
void StepFrame(Scene& scene) {
    scene.FixedUpdate(kFixedDt);
    scene.FrameUpdate(kFixedDt);
}

/// 実行されたフェーズを共有ログに記録する Component。フェーズごとに別の型になるため
/// 「フェーズは種類ごとに固定」の契約（Component.h）も満たす。
template<UpdatePhase P>
class PhaseProbe : public Component {
public:
    WITCH_COMPONENT(PhaseProbe, Component)

    PhaseProbe(StepLog* log, std::string name) : log_(log), name_(std::move(name)) {}
    UpdatePhase Phase() const override { return P; }
    void Update(float) override { log_->push_back(name_); }

private:
    StepLog* log_;
    std::string name_;
};

/// Update フックを共有ログに記録する GameObject。
class HookProbeObject : public GameObject {
public:
    explicit HookProbeObject(StepLog* log) : log_(log) {}
    void Update(float) override { log_->push_back("Hook"); }

private:
    StepLog* log_;
};

TEST_CASE("Phases run in declaration order with hook before Update phase", "[ComponentScheduler]") {
    Scene scene;
    StepLog log;

    auto* obj = scene.Spawn<HookProbeObject>(&log);
    // 宣言順と逆に登録し、実行順が追加順ではなくフェーズ順であることを確かめる。
    obj->AddComponent<PhaseProbe<UpdatePhase::Render>>(&log, "Render");
    obj->AddComponent<PhaseProbe<UpdatePhase::Camera>>(&log, "Camera");
    obj->AddComponent<PhaseProbe<UpdatePhase::Animation>>(&log, "Animation");
    obj->AddComponent<PhaseProbe<UpdatePhase::PostUpdate>>(&log, "PostUpdate");
    obj->AddComponent<PhaseProbe<UpdatePhase::Update>>(&log, "Update");
    obj->AddComponent<PhaseProbe<UpdatePhase::PreUpdate>>(&log, "PreUpdate");

    StepFrame(scene);

    const StepLog expected{
        "PreUpdate", "Hook", "Update", "PostUpdate", "Animation", "Camera", "Render",
    };
    CHECK(log == expected);
}

/// Update フェーズ中に Render フェーズの Component を追加する Component。
class MidFrameAdder : public Component {
public:
    WITCH_COMPONENT(MidFrameAdder, Component)

    explicit MidFrameAdder(StepLog* log) : log_(log) {}
    void Update(float) override {
        if (!added_) {
            added_ = true;
            log_->push_back("Adder");
            Owner()->AddComponent<PhaseProbe<UpdatePhase::Render>>(log_, "LateRender");
        }
    }

private:
    StepLog* log_;
    bool added_ = false;
};

TEST_CASE("Component added mid-frame runs in a later phase of the same frame", "[ComponentScheduler]") {
    Scene scene;
    StepLog log;

    auto* obj = scene.Spawn<GameObject>();
    obj->AddComponent<MidFrameAdder>(&log);

    StepFrame(scene);

    // ComponentScheduler.h の契約: 保留反映は各フェーズ実行直前に行うため、
    // 固定側の Update 中に追加した Render フェーズの Component は
    // 同一フレームの FrameUpdate の Render で走る
    // （Update 中に足した Sprite が 1 フレーム消える現象を防ぐ）。
    const StepLog expected{"Adder", "LateRender"};
    CHECK(log == expected);

    // 次フレームは通常どおり Render フェーズで走る。
    log.clear();
    StepFrame(scene);
    const StepLog expected2{"LateRender"};
    CHECK(log == expected2);
}

}  // namespace
