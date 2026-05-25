#pragma once

#include <string>
#include <vector>

#include "common/Types.h"

struct MachineDef {
    MachineType type             = MachineType::WoodSpawner;
    std::string id;
    int         processingTime   = 1;
    double      breakdownProb    = 0.0;
    int         requiredCount    = 1;
    int         maxHealth        = 10;
    std::string outputConveyorId;          // Packager는 ""
};

struct ConveyorDef {
    std::string  id;
    int          length        = 5;
    std::string  downstreamId;
    OverflowMode overflowMode  = OverflowMode::Drop;
};

struct TechnicianDef {
    std::string id;                       // 내부 식별자
    std::string name;                     // UI 표시용. 미지정 시 id 사용
    int         repairTime = 3;
};

struct ScenarioConfig {
    ScenarioType               type = ScenarioType::Normal;
    std::string                name;
    std::vector<MachineDef>    machines;
    std::vector<ConveyorDef>   conveyors;
    std::vector<TechnicianDef> technicians;
};
