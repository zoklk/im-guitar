// Phase 6 — Fault cascade 토픽 구독 (SmartFactory 한정).
// mini DAG: SPN_A + SPN_B → ELEC → ASM (sink)
// ELEC fault → SPN_A/B의 pendingDownstreamFaults_ == 1 검증.

#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/factory/Factory.h"
#include "model/machine/IMachineLookup.h"
#include "model/machine/Machine.h"
#include "model/scenario/ScenarioConfig.h"
#include "model/repair_dispatcher/RepairDispatcher.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

class NullLookup : public IMachineLookup {
public:
    Machine* findMachine(const std::string&) override { return nullptr; }
};

// SPN_BRIDGE → CONV_BR ─┐
//                        ├→ ELEC → CONV_ELEC → PA(sink)
// SPN_PICKUP → CONV_PK ─┘
ScenarioConfig makeMiniDag(ScenarioType type) {
    ScenarioConfig cfg;
    cfg.type = type;
    cfg.machines = {
        {MachineType::BridgeSpawner,     "SPN_BR",   1, 0.0, 0, 10, "CONV_BR"},
        {MachineType::PickupSpawner,     "SPN_PK",   1, 0.0, 0, 10, "CONV_PK"},
        {MachineType::ElecPartCollector, "ELEC",     1, 0.0, 2, 10, "CONV_EL"},
        {MachineType::PartAssembler,     "PA",       1, 0.0, 2, 10, ""},  // sink
    };
    const OverflowMode mode =
        (type == ScenarioType::SmartFactory) ? OverflowMode::Backpressure : OverflowMode::Drop;
    cfg.conveyors = {
        {"CONV_BR", 3, "ELEC", mode},
        {"CONV_PK", 3, "ELEC", mode},
        {"CONV_EL", 3, "PA",   mode},
    };
    return cfg;
}

struct Harness {
    EventBroker       broker;
    EventLog          eventLog{broker};
    Statistics        stats{broker};
    NullLookup        nullLookup;
    RepairDispatcher dispatcher{broker, nullLookup};
    Factory           factory{broker, eventLog, stats, dispatcher};

    Harness() { dispatcher.setLookup(factory); }
};

}  // namespace

TEST(PhaseCascade, SmartFactoryPropagatesDownstreamFault) {
    Harness h;
    h.factory.applyConfig(makeMiniDag(ScenarioType::SmartFactory));

    h.factory.forceBreak("ELEC");
    h.factory.tick();
    h.broker.flush();

    Machine* spnBr = h.factory.findMachine("SPN_BR");
    Machine* spnPk = h.factory.findMachine("SPN_PK");
    ASSERT_NE(spnBr, nullptr);
    ASSERT_NE(spnPk, nullptr);
    EXPECT_EQ(spnBr->getPendingDownstreamFaults(), 1);
    EXPECT_EQ(spnPk->getPendingDownstreamFaults(), 1);
    EXPECT_FALSE(spnBr->canStart());
    EXPECT_FALSE(spnPk->canStart());
}

TEST(PhaseCascade, SmartFactoryResumeClearsCounter) {
    Harness h;
    h.factory.applyConfig(makeMiniDag(ScenarioType::SmartFactory));

    h.factory.forceBreak("ELEC");
    h.factory.tick();
    h.broker.flush();

    h.factory.instantRepair("ELEC");
    h.broker.flush();

    Machine* spnBr = h.factory.findMachine("SPN_BR");
    ASSERT_NE(spnBr, nullptr);
    EXPECT_EQ(spnBr->getPendingDownstreamFaults(), 0);
}

TEST(PhaseCascade, SmartFactoryRefcountWithMultipleFaults) {
    Harness h;
    h.factory.applyConfig(makeMiniDag(ScenarioType::SmartFactory));

    // ELEC + PA 동시 고장 → SPN의 counter는 2
    h.factory.forceBreak("ELEC");
    h.factory.forceBreak("PA");
    h.factory.tick();
    h.broker.flush();

    Machine* spnBr = h.factory.findMachine("SPN_BR");
    ASSERT_NE(spnBr, nullptr);
    EXPECT_EQ(spnBr->getPendingDownstreamFaults(), 2);

    // PA만 복구 → 1로 감소, 여전히 정지
    h.factory.instantRepair("PA");
    h.broker.flush();
    EXPECT_EQ(spnBr->getPendingDownstreamFaults(), 1);
    EXPECT_FALSE(spnBr->canStart());

    // ELEC 복구 → 0, 재개 가능
    h.factory.instantRepair("ELEC");
    h.broker.flush();
    EXPECT_EQ(spnBr->getPendingDownstreamFaults(), 0);
}

TEST(PhaseCascade, DropScenarioSkipsCascadeSubscriptions) {
    Harness h;
    h.factory.applyConfig(makeMiniDag(ScenarioType::Normal));   // Drop 모드 시나리오

    h.factory.forceBreak("ELEC");
    h.factory.tick();
    h.broker.flush();

    // Drop 시나리오는 cascade 구독을 skip → counter 변동 없음
    Machine* spnBr = h.factory.findMachine("SPN_BR");
    ASSERT_NE(spnBr, nullptr);
    EXPECT_EQ(spnBr->getPendingDownstreamFaults(), 0);
}
