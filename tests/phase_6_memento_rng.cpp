// Phase 6 — Factory.snapshot/restore + RNG 직렬화 round-trip.
// mt19937 streaming operator는 표준 — 같은 seed 후 sequence 재현 검증.

#include <random>
#include <sstream>

#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/factory/Factory.h"
#include "model/factory/SimulationRunner.h"
#include "model/machine/IMachineLookup.h"
#include "model/machine/Machine.h"
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
        {MachineType::WoodSpawner, "SPN",  1, 0.0, 0, 10, "C1"},
        {MachineType::HeadCutter,  "CUT",  2, 0.0, 1, 10, "C2"},
        {MachineType::Packager,    "PACK", 1, 0.0, 1, 10, ""},
    };
    cfg.conveyors = {
        {"C1", 3, "CUT",  OverflowMode::Drop},
        {"C2", 3, "PACK", OverflowMode::Drop},
    };
    cfg.technicians = {{"T1", "tech1", 2}};
    return cfg;
}

struct Harness {
    EventBroker       broker;
    EventLog          eventLog{broker};
    Statistics        stats{broker};
    NullLookup        nullLookup;
    TechnicianManager mgr{broker, nullLookup};
    Factory           factory{broker, eventLog, stats, mgr};

    Harness() {
        mgr.setLookup(factory);
        factory.applyConfig(makeTinyConfig());
    }
};

}  // namespace

TEST(PhaseMementoRng, Mt19937StreamRoundTrip) {
    std::mt19937 a(42);
    // a를 어느 정도 진행
    for (int i = 0; i < 100; ++i) (void)a();

    std::stringstream ss;
    ss << a;

    std::mt19937 b;
    ss >> b;

    // a와 b가 같은 sequence 생성
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(a(), b());
    }
}

TEST(PhaseMementoRng, SnapshotCapturesTickAndStats) {
    Harness h;
    for (int i = 0; i < 5; ++i) {
        h.factory.tick();
        h.broker.flush();
    }

    FactorySnap snap = h.factory.snapshot();
    EXPECT_EQ(snap.tick, h.factory.getTick());
    EXPECT_EQ(snap.machines.size(), 3u);
    EXPECT_EQ(snap.conveyors.size(), 2u);
    EXPECT_EQ(snap.technicians.size(), 1u);
    EXPECT_FALSE(snap.rngState.empty());
}

TEST(PhaseMementoRng, RestoreRecoversTick) {
    Harness h;
    for (int i = 0; i < 5; ++i) {
        h.factory.tick();
        h.broker.flush();
    }
    FactorySnap snapAt5 = h.factory.snapshot();

    // 5틱 더 진행
    for (int i = 0; i < 5; ++i) {
        h.factory.tick();
        h.broker.flush();
    }
    EXPECT_EQ(h.factory.getTick(), 10);

    // restore
    h.factory.restore(snapAt5);
    EXPECT_EQ(h.factory.getTick(), 5);
}

TEST(PhaseMementoRng, RestoreReproducesMachineOutputCount) {
    Harness h;
    for (int i = 0; i < 8; ++i) {
        h.factory.tick();
        h.broker.flush();
    }
    Machine* spnPre = h.factory.findMachine("SPN");
    ASSERT_NE(spnPre, nullptr);
    const int outCountAt8 = spnPre->getOutputCount();

    FactorySnap snapAt8 = h.factory.snapshot();

    // 진행 후 restore
    h.factory.tick();
    h.factory.tick();
    h.broker.flush();
    h.factory.restore(snapAt8);

    Machine* spnPost = h.factory.findMachine("SPN");
    ASSERT_NE(spnPost, nullptr);
    EXPECT_EQ(spnPost->getOutputCount(), outCountAt8);
}

TEST(PhaseMementoRng, RestoreReproducesRngSequence) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());

    // 시드 후 8틱 진행
    for (int i = 0; i < 8; ++i) {
        h.factory.tick();
        h.broker.flush();
    }
    FactorySnap snap = h.factory.snapshot();

    // 두 경로의 outputCount가 결정론적으로 일치해야 함
    h.factory.tick();
    h.broker.flush();
    Machine* spn = h.factory.findMachine("SPN");
    ASSERT_NE(spn, nullptr);
    const int outA = spn->getOutputCount();

    h.factory.restore(snap);
    h.factory.tick();
    h.broker.flush();
    spn = h.factory.findMachine("SPN");
    ASSERT_NE(spn, nullptr);
    const int outB = spn->getOutputCount();

    EXPECT_EQ(outA, outB);
}
