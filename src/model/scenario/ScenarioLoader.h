#pragma once

#include <string>

#include "ScenarioConfig.h"
#include "common/Types.h"

// JSON 시나리오 파일을 ScenarioConfig로 파싱.
// 파일 경로는 baseDir + scenarioFileName으로 조립. 기본 baseDir = "scenarios"
// (CMake가 build output에 scenarios/ 디렉토리를 복사하므로 실행 작업 디렉토리 기준 상대경로).
//
// Controller가 setScenario cmd 처리 시 ScenarioLoader.load → Factory.applyConfig 호출.
class ScenarioLoader {
public:
    explicit ScenarioLoader(std::string baseDir = "scenarios");

    // 잘못된 enum 문자열 / 파일 없음 / JSON 파싱 실패 → std::runtime_error.
    ScenarioConfig load(ScenarioType type);

private:
    std::string baseDir_;
};
