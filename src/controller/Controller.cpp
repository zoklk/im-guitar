#include "Controller.h"

#include "model/factory/Factory.h"
#include "model/factory/SimulationRunner.h"
#include "model/memento/MementoStore.h"
#include "model/scenario/ScenarioConfig.h"
#include "model/scenario/ScenarioLoader.h"

Controller::Controller(Factory&          factory,
                       SimulationRunner& runner,
                       MementoStore&     mementoStore,
                       ScenarioLoader&   scenarioLoader)
    : factory_(factory),
      runner_(runner),
      mementoStore_(mementoStore),
      scenarioLoader_(scenarioLoader) {}

void Controller::dispatch(const MachineCmd& cmd) {
    switch (cmd.action) {
        case CmdAction::None:
            break;
        case CmdAction::Start:
            runner_.start();
            break;
        case CmdAction::Pause:
            runner_.pause();
            break;
        case CmdAction::Reset:
            runner_.reset();
            factory_.reset();
            break;
        case CmdAction::SetSpeed:
            runner_.setSpeed(cmd.speedMultiplier);
            break;
        case CmdAction::SetScenario: {
            factory_.reset();
            ScenarioConfig cfg = scenarioLoader_.load(cmd.scenario);
            factory_.applyConfig(cfg);
            mementoStore_.clear();
            break;
        }
        case CmdAction::ForceBreak:
            factory_.forceBreak(cmd.targetMachineId);
            break;
        case CmdAction::InstantRepair:
            factory_.instantRepair(cmd.targetMachineId);
            break;
        case CmdAction::Rewind: {
            if (!mementoStore_.empty()) {
                FactorySnap snap = mementoStore_.rewind(cmd.rewindTargetTick);
                factory_.restore(snap);
            }
            break;
        }
        case CmdAction::ClearLog:
            factory_.clearLog();
            break;
    }
}
