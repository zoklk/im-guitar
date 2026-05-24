#pragma once

#include <string>

class Machine;

// Machine ID → Machine* 조회 인터페이스.
// Factory(Phase 6)가 구현. TechnicianManager는 sourceId(string)으로 들어오는
// Fault 이벤트에서 대상 Machine*를 얻기 위해 이 인터페이스에 의존.
// Phase 5/6 의존성 역전 — TechnicianManager는 Factory 헤더를 include하지 않음.
class IMachineLookup {
public:
    virtual ~IMachineLookup() = default;

    // 일치하는 머신이 없으면 nullptr. 호출자가 분기 처리.
    virtual Machine* findMachine(const std::string& id) = 0;
};
