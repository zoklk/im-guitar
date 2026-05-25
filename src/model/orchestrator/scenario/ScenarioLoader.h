#pragma once

#include <string>

#include "ScenarioConfig.h"
#include "common/Types.h"

// 기본 baseDir = "scenarios" (CMake가 build output에 scenarios/ 복사).
class ScenarioLoader {
public:
    explicit ScenarioLoader(std::string baseDir = "scenarios");

    // 잘못된 enum / 파일 없음 / 파싱 실패 → std::runtime_error
    ScenarioConfig load(ScenarioType type);

private:
    std::string baseDir_;
};
