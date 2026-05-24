#pragma once

#include <string>
#include <vector>

#include "common/Types.h"

// ScenarioLoader가 JSON에서 파싱해 반환하는 데이터 객체.
// Factory(Phase 6) applyConfig가 이 구조체를 받아 객체 생성 시퀀스로 변환.

struct MachineDef {
    MachineType type             = MachineType::WoodSpawner;
    std::string id;
    int         processingTime   = 1;
    double      breakdownProb    = 0.0;
    int         requiredCount    = 1;
    int         maxHealth        = 10;     // JSON에 명시 안 하면 default 10
    std::string outputConveyorId;          // Packager는 ""
};

struct ConveyorDef {
    std::string  id;
    int          length        = 5;
    std::string  downstreamId;
    OverflowMode overflowMode  = OverflowMode::Drop;
};

struct TechnicianDef {
    std::string id;                       // 내부 식별자 (예: "TECH_1")
    std::string name;                     // UI 표시용 (예: "jincheol"). 미지정 시 id 사용.
    int         repairTime = 3;
};

struct ScenarioConfig {
    ScenarioType               type = ScenarioType::Normal;
    std::string                name;
    std::vector<MachineDef>    machines;
    std::vector<ConveyorDef>   conveyors;
    std::vector<TechnicianDef> technicians;
};
