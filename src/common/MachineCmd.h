#pragma once

#include <string>

#include "Types.h"

struct MachineCmd {
    CmdAction    action           = CmdAction::None;
    std::string  targetMachineId;                            // ForceBreak / InstantRepair
    int          speedMultiplier  = 1;                       // SetSpeed (1~5)
    ScenarioType scenario         = ScenarioType::Normal;    // SetScenario
    int          rewindTargetTick = 0;                       // Rewind
};
