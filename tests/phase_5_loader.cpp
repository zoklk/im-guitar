// Phase 5 — ScenarioLoader 단위 테스트.
// 5종 JSON 파싱, enum 매핑, 잘못된 type 문자열 예외 검증.

#include <fstream>
#include <stdexcept>
#include <string>

#include "common/Types.h"
#include "model/scenario/ScenarioConfig.h"
#include "model/scenario/ScenarioLoader.h"

#include <gtest/gtest.h>

namespace {

// 기본 baseDir = "scenarios" (ctest는 build/ 작업 디렉토리에서 실행되므로
// CMake가 build/scenarios/로 복사한 파일을 참조).
ScenarioConfig loadOne(ScenarioType type) {
    ScenarioLoader loader;
    return loader.load(type);
}

}  // namespace

// 모든 시나리오 공통: 13개 머신, 12개 컨베이어, 2명 Technician.

TEST(PhaseLoader, LoadsNormalScenario) {
    auto cfg = loadOne(ScenarioType::Normal);
    EXPECT_EQ(cfg.type, ScenarioType::Normal);
    EXPECT_EQ(cfg.name, "Normal");
    EXPECT_EQ(cfg.machines.size(), 13u);
    EXPECT_EQ(cfg.conveyors.size(), 12u);
    EXPECT_EQ(cfg.technicians.size(), 2u);
}

TEST(PhaseLoader, LoadsBreakdownsScenario) {
    auto cfg = loadOne(ScenarioType::Breakdowns);
    EXPECT_EQ(cfg.name, "Breakdowns");
    EXPECT_EQ(cfg.machines.size(), 13u);
    // breakdownProb가 0.02로 설정되었는지 확인
    for (const auto& m : cfg.machines) {
        EXPECT_DOUBLE_EQ(m.breakdownProb, 0.02);
    }
}

TEST(PhaseLoader, LoadsBottleneckScenarioWithPainterPt12) {
    auto cfg = loadOne(ScenarioType::Bottleneck);
    EXPECT_EQ(cfg.name, "Bottleneck");
    bool found = false;
    for (const auto& m : cfg.machines) {
        if (m.type == MachineType::Painter) {
            EXPECT_EQ(m.processingTime, 12);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PhaseLoader, LoadsOverflowScenarioWithWoodSpawnerPt1Drop) {
    auto cfg = loadOne(ScenarioType::Overflow);
    EXPECT_EQ(cfg.name, "Overflow");
    int woodSpawnerCount = 0;
    for (const auto& m : cfg.machines) {
        if (m.type == MachineType::WoodSpawner) {
            EXPECT_EQ(m.processingTime, 1);
            ++woodSpawnerCount;
        }
    }
    EXPECT_EQ(woodSpawnerCount, 3);
    // 전부 drop 모드
    for (const auto& c : cfg.conveyors) {
        EXPECT_EQ(c.overflowMode, OverflowMode::Drop);
    }
}

TEST(PhaseLoader, LoadsSmartFactoryScenarioWithBackpressureMode) {
    auto cfg = loadOne(ScenarioType::SmartFactory);
    EXPECT_EQ(cfg.name, "SmartFactory");
    for (const auto& c : cfg.conveyors) {
        EXPECT_EQ(c.overflowMode, OverflowMode::Backpressure);
    }
}

// enum 매핑 확인 — MachineType 11종 전부 적어도 한 번 등장.

TEST(PhaseLoader, AllMachineTypesPresentInNormal) {
    auto cfg = loadOne(ScenarioType::Normal);

    bool hasWood = false, hasBridge = false, hasPickup = false;
    bool hasHead = false, hasNeck = false, hasBodyCut = false;
    bool hasPaint = false, hasElec = false;
    bool hasBodyAsm = false, hasPartAsm = false, hasPack = false;
    for (const auto& m : cfg.machines) {
        switch (m.type) {
            case MachineType::WoodSpawner:       hasWood    = true; break;
            case MachineType::BridgeSpawner:     hasBridge  = true; break;
            case MachineType::PickupSpawner:     hasPickup  = true; break;
            case MachineType::HeadCutter:        hasHead    = true; break;
            case MachineType::NeckCutter:        hasNeck    = true; break;
            case MachineType::BodyCutter:        hasBodyCut = true; break;
            case MachineType::Painter:           hasPaint   = true; break;
            case MachineType::ElecPartCollector: hasElec    = true; break;
            case MachineType::BodyAssembler:     hasBodyAsm = true; break;
            case MachineType::PartAssembler:     hasPartAsm = true; break;
            case MachineType::Packager:          hasPack    = true; break;
        }
    }
    EXPECT_TRUE(hasWood && hasBridge && hasPickup);
    EXPECT_TRUE(hasHead && hasNeck && hasBodyCut);
    EXPECT_TRUE(hasPaint && hasElec);
    EXPECT_TRUE(hasBodyAsm && hasPartAsm && hasPack);
}

// 잘못된 baseDir → 파일 못 찾음 → runtime_error.

TEST(PhaseLoader, MissingFileThrows) {
    ScenarioLoader bogus("nonexistent_scenarios_dir_xyz");
    EXPECT_THROW(bogus.load(ScenarioType::Normal), std::runtime_error);
}

// 잘못된 type / overflowMode 문자열 → runtime_error.
// std::filesystem으로 격리된 임시 디렉토리에 normal.json만 생성하고 그 디렉토리를
// baseDir로 지정. CMake가 복사한 진짜 scenarios/와 분리.

#include <filesystem>

namespace {

namespace fs = std::filesystem;

class TempScenarioDir {
public:
    explicit TempScenarioDir(const std::string& suffix) {
        path_ = fs::temp_directory_path() / ("im_guitar_test_" + suffix);
        fs::create_directories(path_);
    }
    ~TempScenarioDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    std::string str() const { return path_.string(); }
    void write(const std::string& filename, const std::string& content) {
        std::ofstream ofs(path_ / filename);
        ofs << content;
    }
private:
    fs::path path_;
};

}  // namespace

TEST(PhaseLoader, UnknownMachineTypeThrows) {
    TempScenarioDir dir("unknownmachine");
    dir.write("normal.json", R"({
        "name": "Bad",
        "machines": [
            { "type": "NotAMachine", "id": "X", "processingTime": 1, "breakdownProb": 0.0, "requiredCount": 0, "outputConveyorId": "" }
        ],
        "conveyors": [],
        "technicians": []
    })");

    ScenarioLoader loader(dir.str());
    EXPECT_THROW(loader.load(ScenarioType::Normal), std::runtime_error);
}

TEST(PhaseLoader, UnknownOverflowModeThrows) {
    TempScenarioDir dir("unknownoverflow");
    dir.write("normal.json", R"({
        "name": "Bad",
        "machines": [],
        "conveyors": [
            { "id": "C", "length": 5, "downstreamId": "X", "overflowMode": "unknown" }
        ],
        "technicians": []
    })");

    ScenarioLoader loader(dir.str());
    EXPECT_THROW(loader.load(ScenarioType::Normal), std::runtime_error);
}
