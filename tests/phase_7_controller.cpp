// Phase 7 — Controller dispatch.
// 각 CmdAction이 해당 도메인 메서드를 호출하는지 실제 객체로 verify.

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

}  // namespace

TEST(PhaseController, NoneIsNoop) {
    Harness h;
    MachineCmd cmd;
    cmd.action = CmdAction::None;
    h.ctrl.dispatch(cmd);   // 예외 없이 통과
    EXPECT_FALSE(h.runner.isRunning());
}

TEST(PhaseController, StartTogglesRunnerOn) {
    Harness h;
    MachineCmd cmd;
    cmd.action = CmdAction::Start;
    h.ctrl.dispatch(cmd);
    EXPECT_TRUE(h.runner.isRunning());
}

TEST(PhaseController, PauseTogglesRunnerOff) {
    Harness h;
    h.runner.start();
    MachineCmd cmd;
    cmd.action = CmdAction::Pause;
    h.ctrl.dispatch(cmd);
    EXPECT_FALSE(h.runner.isRunning());
}

TEST(PhaseController, SetSpeedUpdatesRunner) {
    Harness h;
    MachineCmd cmd;
    cmd.action          = CmdAction::SetSpeed;
    cmd.speedMultiplier = 3;
    h.ctrl.dispatch(cmd);
    EXPECT_EQ(h.runner.getSpeed(), 3);
}

TEST(PhaseController, SetScenarioLoadsAndApplies) {
    Harness h;
    MachineCmd cmd;
    cmd.action   = CmdAction::SetScenario;
    cmd.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(cmd);

    EXPECT_EQ(h.factory.getMachines().size(),   13u);
    EXPECT_EQ(h.factory.getConveyors().size(),  12u);
    EXPECT_EQ(h.factory.getTechnicians().size(), 2u);
    EXPECT_EQ(h.factory.getScenario(), ScenarioType::Normal);
}

TEST(PhaseController, ForceBreakSetsHealthZero) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(setSc);

    MachineCmd fb;
    fb.action          = CmdAction::ForceBreak;
    fb.targetMachineId = "MCH_PACK";
    h.ctrl.dispatch(fb);

    Machine* m = h.factory.findMachine("MCH_PACK");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getHealth(), 0);
}

TEST(PhaseController, InstantRepairRestoresHealth) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(setSc);

    MachineCmd fb;
    fb.action          = CmdAction::ForceBreak;
    fb.targetMachineId = "MCH_PACK";
    h.ctrl.dispatch(fb);

    MachineCmd ir;
    ir.action          = CmdAction::InstantRepair;
    ir.targetMachineId = "MCH_PACK";
    h.ctrl.dispatch(ir);

    Machine* m = h.factory.findMachine("MCH_PACK");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getHealth(), m->getMaxHealth());
}

TEST(PhaseController, ClearLogEmptiesEventLog) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(setSc);

    // Spawner pt=6 — 첫 Spawned 이벤트가 발행되려면 약 7~8틱 필요
    for (int i = 0; i < 10; ++i) {
        h.factory.tick();
        h.broker.flush();
    }
    EXPECT_GT(h.eventLog.size(), 0u);

    MachineCmd cl;
    cl.action = CmdAction::ClearLog;
    h.ctrl.dispatch(cl);
    EXPECT_EQ(h.eventLog.size(), 0u);
}

TEST(PhaseController, RewindRestoresFactoryState) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(setSc);

    // Runner.tryAdvance 통해 mementoStore에 push 누적
    h.runner.start();
    h.runner.setSpeed(5);
    h.runner.tryAdvance(2.0);
    ASSERT_GT(h.mementoStore.size(), 1u);

    const int tickAtEarly = h.mementoStore.firstTick();

    MachineCmd rw;
    rw.action           = CmdAction::Rewind;
    rw.rewindTargetTick = tickAtEarly;
    h.ctrl.dispatch(rw);

    EXPECT_EQ(h.factory.getTick(), tickAtEarly);
}

TEST(PhaseController, ResetClearsFactoryAndRunner) {
    Harness h;
    MachineCmd setSc;
    setSc.action   = CmdAction::SetScenario;
    setSc.scenario = ScenarioType::Normal;
    h.ctrl.dispatch(setSc);

    h.runner.start();
    EXPECT_TRUE(h.runner.isRunning());

    MachineCmd rs;
    rs.action = CmdAction::Reset;
    h.ctrl.dispatch(rs);

    EXPECT_FALSE(h.runner.isRunning());
    EXPECT_EQ(h.factory.getTick(), 0);
    EXPECT_TRUE(h.factory.getMachines().empty());
}
