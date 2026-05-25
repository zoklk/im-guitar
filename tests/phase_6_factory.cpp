// Phase 6 — Factory 기본 동작.
// applyConfig 후 객체 개수 / find / tick 진행 / forceBreak·instantRepair 검증.

#include <memory>
#include <vector>

#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/factory/Factory.h"
#include "model/machine/IMachineLookup.h"
#include "model/machine/Machine.h"
#include "model/machine/MachineStates.h"
#include "model/scenario/ScenarioConfig.h"
#include "model/technician_manager/TechnicianManager.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

// Factory가 IMachineLookup을 구현하지만 생성 순환을 끊기 위해 NullLookup으로 초기화 후
// Factory 생성 직후 setLookup으로 갱신.
class NullLookup : public IMachineLookup {
public:
    Machine* findMachine(const std::string&) override { return nullptr; }
};

ScenarioConfig makeTinyConfig() {
    ScenarioConfig cfg;
    cfg.type = ScenarioType::Normal;
    cfg.name = "Tiny";

    cfg.machines = {
        {MachineType::WoodSpawner, "SPN",  /*pt=*/1, /*bp=*/0.0, /*req=*/0, /*hp=*/10, "C1"},
        {MachineType::HeadCutter,  "CUT",  /*pt=*/2, /*bp=*/0.0, /*req=*/1, /*hp=*/10, "C2"},
        {MachineType::Packager,    "PACK", /*pt=*/1, /*bp=*/0.0, /*req=*/1, /*hp=*/10, ""},
    };
    cfg.conveyors = {
        {"C1", /*len=*/3, "CUT",  OverflowMode::Drop},
        {"C2", /*len=*/3, "PACK", OverflowMode::Drop},
    };
    cfg.technicians = {
        {"T1", "tech1", /*rt=*/2},
    };
    return cfg;
}

struct Harness {
    EventBroker       broker;
    EventLog          eventLog{broker};
    Statistics        stats{broker};
    NullLookup        nullLookup;
    TechnicianManager mgr{broker, nullLookup};
    Factory           factory{broker, eventLog, stats, mgr};

    Harness() { mgr.setLookup(factory); }
};

}  // namespace

TEST(PhaseFactory, ApplyConfigCreatesAllObjects) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());
    EXPECT_EQ(h.factory.getMachines().size(), 3u);
    EXPECT_EQ(h.factory.getConveyors().size(), 2u);
    EXPECT_EQ(h.factory.getTechnicians().size(), 1u);
}

TEST(PhaseFactory, FindByIdWorks) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());

    EXPECT_NE(h.factory.findMachine("SPN"),  nullptr);
    EXPECT_NE(h.factory.findMachine("CUT"),  nullptr);
    EXPECT_NE(h.factory.findMachine("PACK"), nullptr);
    EXPECT_NE(h.factory.findConveyor("C1"), nullptr);
    EXPECT_NE(h.factory.findConveyor("C2"), nullptr);
    EXPECT_NE(h.factory.findTechnician("T1"), nullptr);

    EXPECT_EQ(h.factory.findMachine("NOPE"), nullptr);
    EXPECT_EQ(h.factory.findConveyor("NOPE"), nullptr);
}

TEST(PhaseFactory, TickAdvancesSpawnerOutput) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());

    for (int i = 0; i < 10; ++i) {
        h.factory.tick();
        h.broker.flush();
    }

    Machine* spn = h.factory.findMachine("SPN");
    ASSERT_NE(spn, nullptr);
    EXPECT_GT(spn->getOutputCount(), 0);
    EXPECT_GT(h.factory.getTick(), 0);
}

TEST(PhaseFactory, ForceBreakTransitionsToBrokenAndPublishesFault) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());

    h.factory.forceBreak("CUT");
    h.factory.tick();
    h.broker.flush();

    Machine* cut = h.factory.findMachine("CUT");
    ASSERT_NE(cut, nullptr);
    EXPECT_EQ(cut->getHealth(), 0);
    EXPECT_EQ(cut->getCurrentState(), &MachineBrokenState::instance());
    EXPECT_GT(h.stats.getBreakdowns(), 0);
}

TEST(PhaseFactory, InstantRepairRestoresHealth) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());

    h.factory.forceBreak("CUT");
    h.factory.tick();
    h.broker.flush();

    h.factory.instantRepair("CUT");
    h.broker.flush();

    Machine* cut = h.factory.findMachine("CUT");
    ASSERT_NE(cut, nullptr);
    EXPECT_EQ(cut->getHealth(), cut->getMaxHealth());
    EXPECT_NE(cut->getCurrentState(), &MachineBrokenState::instance());
}

TEST(PhaseFactory, ResetClearsObjectsAndCounters) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());

    h.factory.tick();
    h.factory.tick();
    h.broker.flush();
    EXPECT_GT(h.factory.getTick(), 0);

    h.factory.reset();
    EXPECT_EQ(h.factory.getTick(), 0);
    EXPECT_TRUE(h.factory.getMachines().empty());
    EXPECT_TRUE(h.factory.getConveyors().empty());
    EXPECT_TRUE(h.factory.getTechnicians().empty());
}

TEST(PhaseFactory, PriorityMapBfsAssignsPackagerZero) {
    Harness h;
    h.factory.applyConfig(makeTinyConfig());

    // BFS: PACK=0, CUT=1, SPN=2
    EXPECT_EQ(h.mgr.priorityOf("PACK"), 0);
    EXPECT_EQ(h.mgr.priorityOf("CUT"),  1);
    EXPECT_EQ(h.mgr.priorityOf("SPN"),  2);
}
