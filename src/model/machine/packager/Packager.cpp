#include "Packager.h"

#include <utility>

#include "model/product/Product.h"

Packager::Packager(std::string   id,
                   int           processingTime,
                   double        breakdownProb,
                   EventBroker&  broker,
                   std::mt19937& rng,
                   ProductIdGen& idGen,
                   int           maxHealth)
    : Machine(std::move(id),
              MachineType::Packager,
              processingTime,
              breakdownProb,
              /*requiredCount=*/1,
              /*outputOverflowMode=*/OverflowMode::Drop,   // outputConveyor 없으므로 미사용
              broker,
              rng,
              idGen,
              maxHealth) {}

void Packager::process(int tick) {
    if (currentProduct_.empty()) return;

    auto       input = std::move(currentProduct_.back());
    currentProduct_.pop_back();
    const int  pid   = input->getId();
    const auto ptype = input->getType();

    // input은 함수 종료 시 unique_ptr 소멸로 폐기 (출하 = 시스템에서 제거)
    publishEvent(EventType::Packaged, tick, pid, ptype);
    publishEvent(EventType::Completed, tick, pid, ptype);
    ++outputCount_;
}
