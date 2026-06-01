#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Event.h"
#include "LogEntry.h"
#include "Types.h"

struct ProductSnap {
    int         id        = 0;
    ProductType type      = ProductType::RawWood;
    bool        isPainted = false;   // BodyPart에만 유효
};

struct MachineSnap {
    std::string              id;
    MachineType              type               = MachineType::WoodSpawner;
    int                      health             = 10;
    int                      maxHealth          = 10;
    int                      processingTime     = 6;
    int                      progress           = 0;
    double                   breakdownProb      = 0.0;
    int                      outputCount        = 0;
    OverflowMode             outputOverflowMode = OverflowMode::Drop;
    bool                     suspended          = false;   // Backpressure 모드에서 outputConveyor 포화 시 true
    std::vector<ProductSnap> inputBuffer;
    std::vector<ProductSnap> currentProduct;
    std::optional<std::string> assignedTechId;             // Factory.snapshot()이 Technician 목록 훑어 derive
};

struct ConveyorSnap {
    std::string                             id;
    std::string                             downstreamId;
    int                                     length = 5;
    std::vector<std::optional<ProductSnap>> slots;
};

struct TechnicianSnap {
    std::string                id;
    int                        repairTime     = 3;
    int                        repairProgress = 0;
    std::optional<std::string> targetMachineId;
};

struct StatisticsSnap {
    int finished   = 0;
    int wip        = 0;
    int breakdowns = 0;
    int lost       = 0;
};

// TechnicianManager.repairQueue_ 한 entry의 직렬화 표현 (rewind 시 큐 잔량 복원용).
struct RepairOrderSnap {
    std::string machineId;
    int         priority  = 0;
    int         faultTick = 0;
    int         seq       = 0;
};

struct FactorySnap {
    int          tick            = 0;
    ScenarioType scenario        = ScenarioType::Normal;
    int          speedMultiplier = 1;
    bool         running         = false;

    std::vector<MachineSnap>     machines;
    std::vector<ConveyorSnap>    conveyors;
    std::vector<TechnicianSnap>  technicians;
    std::vector<RepairOrderSnap> pendingRepairs;

    StatisticsSnap stats;

    std::vector<LogEntry> logs;
    std::vector<Event>    pendingEvents;     // EventBroker 큐 잔량
    std::string           rngState;          // std::mt19937 직렬화
    int                   productIdCounter   = 0;   // rewind 후 ID 단조성 보존
};
