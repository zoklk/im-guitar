#pragma once

#include <optional>
#include <string>
#include <vector>

#include "LogEntry.h"
#include "Types.h"

struct ProductSnap {
    int         id        = 0;
    ProductType type      = ProductType::RawWood;
    bool        isPainted = false;   // BodyPart에만 유효
};

struct MachineSnap {
    std::string  id;
    MachineState state          = MachineState::Idle;
    int          health         = 10;
    int          processingTime = 6;
    int          progress       = 0;
    double       breakdownProb  = 0.0;
    int          outputCount    = 0;
    std::optional<ProductSnap> currentProduct;
};

struct ConveyorSnap {
    std::string              id;
    int                      capacity = 0;
    std::vector<ProductSnap> items;
};

struct SpawnerSnap {
    std::string id;
    int         period   = 0;   // 한 번 spawn하기까지 걸리는 틱 수 (머신의 processingTime과 동일 의미)
    int         progress = 0;
    std::optional<CutterMachineType> nextRoundRobinTarget;   // WoodSpawner 전용. ElecPartSpawner는 nullopt
};

struct TechnicianSnap {
    std::string     id;
    TechnicianState state = TechnicianState::Waiting;
    std::string     targetMachineId;   // 없으면 빈 문자열
    int             progress = 0;
};

struct StatisticsSnap {
    int finished   = 0;
    int wip        = 0;
    int breakdowns = 0;
    int lost       = 0;
};

struct FactorySnap {
    int          tick            = 0;
    ScenarioType scenario        = ScenarioType::Normal;
    int          speedMultiplier = 1;
    bool         running         = false;

    std::vector<MachineSnap>    machines;
    std::vector<ConveyorSnap>   conveyors;
    std::vector<SpawnerSnap>    spawners;
    std::vector<TechnicianSnap> technicians;

    StatisticsSnap stats;

    std::vector<LogEntry> logs;
};
