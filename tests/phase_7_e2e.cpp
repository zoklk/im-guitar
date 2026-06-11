// Phase 7 — end-to-end. 실제 시나리오 JSON 로드 → tick 진행 → 회계 검증.

#include "common/MachineCmd.h"
#include "common/Types.h"
#include "controller/Controller.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/factory/Factory.h"
#include "model/factory/SimulationRunner.h"
#include "model/machine/IMachineLookup.h"
#include "model/machine/Machine.h"
#include "model/memento/MementoStore.h"
#include "model/scenario/ScenarioLoader.h"
#include "model/repair_dispatcher/RepairDispatcher.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

class NullLookup : public IMachineLookup {
public:
    Machine* findMachine(const std::string&) override { return nullptr; }
};

struct Harness {
    EventBroker       broker;
    EventLog          eventLog{broker};
    Statistics        stats{broker};
    MementoStore      mementoStore;
    NullLookup        nullLookup;
    RepairDispatcher dispatcher{broker, nullLookup};
    Factory           factory{broker, eventLog, stats, dispatcher};
    SimulationRunner  runner{factory, broker, mementoStore};
    ScenarioLoader    loader;
    Controller        ctrl{factory, runner, mementoStore, loader};

    Harness() { dispatcher.setLookup(factory); }
};

void manualTicks(Harness& h, int n) {
    for (int i = 0; i < n; ++i) {
        h.factory.tick();
        h.broker.flush();
        h.mementoStore.push(h.factory.snapshot());
    }
}

}  // namespace

TEST(PhaseE2E, NormalScenarioProducesFinishedGuitars) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(setSc);

    manualTicks(h, 200);

    // Normal 시나리오 — pipeline latency 후 출하 발생
    EXPECT_GT(h.stats.getFinished(), 0);
}

TEST(PhaseE2E, BreakdownsScenarioAccumulatesBreakdownEvents) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Breakdowns;
    h.ctrl.dispatch(setSc);

    // bp=0.02 + 13 machines + maxHealth=10. 한 머신의 평균 break까지 기댓값은 ~500 처리 틱.
    // 300틱은 통계적으로 0이 될 수도 있어 1500틱으로 여유 확보.
    manualTicks(h, 1500);

    EXPECT_GT(h.stats.getBreakdowns(), 0);
}

TEST(PhaseE2E, RewindDeterministicReplay) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(setSc);

    manualTicks(h, 50);
    const int finishedAt50 = h.stats.getFinished();
    FactorySnap snapAt50   = h.factory.snapshot();
    h.mementoStore.push(snapAt50);

    manualTicks(h, 30);
    const int finishedAt80 = h.stats.getFinished();

    // restore → 30틱 더 진행 → 동일 결과
    h.factory.restore(snapAt50);
    EXPECT_EQ(h.factory.getTick(),       50);
    EXPECT_EQ(h.stats.getFinished(),     finishedAt50);

    manualTicks(h, 30);
    EXPECT_EQ(h.factory.getTick(),       80);
    EXPECT_EQ(h.stats.getFinished(),     finishedAt80);
}
