#pragma once

#include "common/MachineCmd.h"

class Factory;
class SimulationRunner;
class MementoStore;
class ScenarioLoader;

// 얇은 dispatch: View가 작성한 MachineCmd를 Factory/Runner/Loader/MementoStore의
// 해당 메서드에 위임. 자체 상태 없음.
class Controller {
public:
    Controller(Factory&          factory,
               SimulationRunner& runner,
               MementoStore&     mementoStore,
               ScenarioLoader&   scenarioLoader);

    void dispatch(const MachineCmd& cmd);

private:
    Factory&          factory_;
    SimulationRunner& runner_;
    MementoStore&     mementoStore_;
    ScenarioLoader&   scenarioLoader_;
};
