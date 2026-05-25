// Phase 6 — SimulationRunner.
// start/pause/setSpeed/tryAdvance 동작 검증.

#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/factory/Factory.h"
#include "model/factory/SimulationRunner.h"
#include "model/machine/IMachineLookup.h"
#include "model/memento/MementoStore.h"
#include "model/scenario/ScenarioConfig.h"
#include "model/technician_manager/TechnicianManager.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

class NullLookup : public IMachineLookup {
public:
    Machine* findMachine(const std::string&) override { return nullptr; }
};

ScenarioConfig makeTinyConfig() {
    ScenarioConfig cfg;
    cfg.type = ScenarioType::Normal;
    cfg.machines = {
        {MachineType::WoodSpawner, "SPN", 1, 0.0, 0, 10, "C1"},
        {MachineType::Packager,    "P",   1, 0.0, 1, 10, ""},
    };
    cfg.conveyors = {
        {"C1", 3, "P", OverflowMode::Drop},
    };
    cfg.technicians = {};
    return cfg;
}

struct Harness {
    EventBroker       broker;
    EventLog          eventLog{broker};
    Statistics        stats{broker};
    NullLookup        nullLookup;
    TechnicianManager mgr{broker, nullLookup};
    Factory           factory{broker, eventLog, stats, mgr};
    MementoStore      mementoStore;
    SimulationRunner  runner{factory, broker, mementoStore};

    Harness() {
        mgr.setLookup(factory);
        factory.applyConfig(makeTinyConfig());
    }
};

}  // namespace

TEST(PhaseRunner, NotRunningByDefault) {
    Harness h;
    EXPECT_FALSE(h.runner.isRunning());
}

TEST(PhaseRunner, TryAdvanceNoopWhenPaused) {
    Harness h;
    // 가상의 5초 dt — 멈춰있으니 진행 안 함
    h.runner.tryAdvance(5.0);
    EXPECT_EQ(h.factory.getTick(), 0);
}

TEST(PhaseRunner, StartTickAfterIntervalReached) {
    Harness h;
    h.runner.setSpeed(1);   // 600ms
    h.runner.start();

    // 0.5초 dt → 아직 1틱 도달 안 함
    h.runner.tryAdvance(0.5);
    EXPECT_EQ(h.factory.getTick(), 0);

    // 추가 0.2초 누적 → 0.7초 ≥ 0.6초 → 1틱 진행
    h.runner.tryAdvance(0.2);
    EXPECT_EQ(h.factory.getTick(), 1);
    EXPECT_EQ(h.mementoStore.size(), 1u);
}

TEST(PhaseRunner, MultipleTicksInOneTryAdvance) {
    Harness h;
    h.runner.setSpeed(1);   // 600ms
    h.runner.start();

    // 1.9초 → 3틱 (0.6 * 3 = 1.8 ≤ 1.9 < 0.6 * 4)
    h.runner.tryAdvance(1.9);
    EXPECT_EQ(h.factory.getTick(), 3);
}

TEST(PhaseRunner, SetSpeedChangesInterval) {
    Harness h;
    h.runner.setSpeed(5);   // 600ms / 5 = 120ms
    h.runner.start();

    h.runner.tryAdvance(0.12);
    EXPECT_EQ(h.factory.getTick(), 1);
}

TEST(PhaseRunner, SetSpeedClampedTo1To5) {
    Harness h;
    h.runner.setSpeed(0);
    EXPECT_EQ(h.runner.getSpeed(), 1);
    h.runner.setSpeed(99);
    EXPECT_EQ(h.runner.getSpeed(), 5);
}

TEST(PhaseRunner, ResetClearsMementoAndState) {
    Harness h;
    h.runner.setSpeed(1);
    h.runner.start();
    h.runner.tryAdvance(1.2);   // 2틱
    EXPECT_GT(h.mementoStore.size(), 0u);
    EXPECT_TRUE(h.runner.isRunning());

    h.runner.reset();
    EXPECT_FALSE(h.runner.isRunning());
    EXPECT_TRUE(h.mementoStore.empty());
}
