// Phase 0 — common 헤더가 모두 컴파일되고 기본 생성 가능한지 검증.
//
// 후속 phase에서 로직 테스트가 들어오면 이 파일은 사라지거나 통합됨.

#include <type_traits>

#include "common/Event.h"
#include "common/FactorySnap.h"
#include "common/LogEntry.h"
#include "common/MachineCmd.h"
#include "common/Types.h"

#include <gtest/gtest.h>

static_assert(std::is_default_constructible_v<Event>);
static_assert(std::is_default_constructible_v<ProductSnap>);
static_assert(std::is_default_constructible_v<MachineSnap>);
static_assert(std::is_default_constructible_v<ConveyorSnap>);
static_assert(std::is_default_constructible_v<TechnicianSnap>);
static_assert(std::is_default_constructible_v<StatisticsSnap>);
static_assert(std::is_default_constructible_v<FactorySnap>);
static_assert(std::is_default_constructible_v<LogEntry>);
static_assert(std::is_default_constructible_v<MachineCmd>);

TEST(Phase0Scaffold, DefaultsAreSane) {
    FactorySnap snap;
    EXPECT_EQ(snap.tick, 0);
    EXPECT_EQ(snap.scenario, ScenarioType::Normal);
    EXPECT_EQ(snap.speedMultiplier, 1);
    EXPECT_FALSE(snap.running);
    EXPECT_TRUE(snap.machines.empty());
    EXPECT_TRUE(snap.conveyors.empty());
    EXPECT_TRUE(snap.technicians.empty());
    EXPECT_TRUE(snap.logs.empty());
    EXPECT_TRUE(snap.pendingEvents.empty());
    EXPECT_TRUE(snap.rngState.empty());
}

TEST(Phase0Scaffold, EnumValuesDistinct) {
    EXPECT_NE(static_cast<int>(MachineType::WoodSpawner),
              static_cast<int>(MachineType::Packager));
    EXPECT_NE(static_cast<int>(EventType::Fault),
              static_cast<int>(EventType::Resume));
    EXPECT_NE(static_cast<int>(OverflowMode::Drop),
              static_cast<int>(OverflowMode::Backpressure));
    EXPECT_NE(static_cast<int>(ScenarioType::Normal),
              static_cast<int>(ScenarioType::SmartFactory));
}
