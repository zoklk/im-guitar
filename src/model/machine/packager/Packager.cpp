#include "Packager.h"

#include <utility>

#include "model/product/Product.h"

Packager::Packager(std::string   id,
                   int           processingTime,
                   double        breakdownProb,
                   EventBroker&  broker,
                   std::mt19937& rng,
                   ProductIdGen& idGen)
    : Machine(std::move(id),
              MachineType::Packager,
              processingTime,
              breakdownProb,
              /*requiredCount=*/1,
              /*outputOverflowMode=*/OverflowMode::Drop,   // outputConveyor 없으므로 미사용
              broker,
              rng,
              idGen) {}

void Packager::process(int tick) {
    if (currentProduct_.empty()) return;

    auto       input = std::move(currentProduct_.back());
    currentProduct_.pop_back();
    const int  pid   = input->getId();
    const auto ptype = input->getType();   // 정상 토폴로지에서 FinishedGuitar

    // input은 함수 종료 시 unique_ptr 소멸로 자동 폐기 (출하 = 시스템에서 제거)

    // 시스템 출하 마커 — Statistics가 wip -= sourceCount(FinishedGuitar)=5 + finished++
    publishEvent(EventType::Packaged, tick, pid, ptype);
    // 라이프사이클 마커 — output 없지만 처리 완료 의미
    publishEvent(EventType::Completed, tick, pid, ptype);
    ++outputCount_;   // UI에서 "출하된 guitar 수"로 표시
}
