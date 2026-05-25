#include "Painter.h"

#include <utility>

#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"

Painter::Painter(std::string   id,
                 int           processingTime,
                 double        breakdownProb,
                 OverflowMode  outputOverflowMode,
                 EventBroker&  broker,
                 std::mt19937& rng,
                 ProductIdGen& idGen,
                 int           maxHealth)
    : Machine(std::move(id),
              MachineType::Painter,
              processingTime,
              breakdownProb,
              /*requiredCount=*/1,
              outputOverflowMode,
              broker,
              rng,
              idGen,
              maxHealth) {}

void Painter::process(int tick) {
    if (currentProduct_.empty()) return;

    // input이 BodyPart인 것은 토폴로지가 보장 — 검사 생략
    currentProduct_.pop_back();
    const int newId = idGen_.next();

    auto painted = std::make_unique<BodyPart>(newId);
    painted->setPainted(true);
    const auto ptype = painted->getType();

    if (tryPushOrDrop(std::move(painted), tick)) {
        publishEvent(EventType::Completed, tick, newId, ptype);
    }
}
