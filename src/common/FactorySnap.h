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
    MachineType              type           = MachineType::WoodSpawner;
    int                      health         = 10;
    int                      processingTime = 6;
    int                      progress       = 0;
    double                   breakdownProb  = 0.0;
    int                      outputCount    = 0;
    bool                     suspended      = false;   // Backpressure 일시정지 상태 (SmartFactory)
    std::vector<ProductSnap> inputBuffer;
    std::vector<ProductSnap> currentProduct;
    std::optional<std::string> assignedTechId;          // Factory.snapshot()이 Technician 목록 훑어 derive
};

struct ConveyorSnap {
    std::string                             id;
    std::string                             downstreamId;
    OverflowMode                            overflowMode = OverflowMode::Drop;
    std::vector<std::optional<ProductSnap>> slots;       // size = conveyor length, nullopt = 빈 슬롯
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

struct FactorySnap {
    int          tick            = 0;
    ScenarioType scenario        = ScenarioType::Normal;
    int          speedMultiplier = 1;
    bool         running         = false;

    std::vector<MachineSnap>    machines;
    std::vector<ConveyorSnap>   conveyors;
    std::vector<TechnicianSnap> technicians;

    StatisticsSnap stats;

    std::vector<LogEntry> logs;
    std::vector<Event>    pendingEvents;   // EventBroker 큐 잔량 (메멘토 정확도)
    std::string           rngState;        // std::mt19937 직렬화 문자열
};
