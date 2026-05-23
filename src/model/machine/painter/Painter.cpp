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
                 ProductIdGen& idGen)
    : Machine(std::move(id),
              MachineType::Painter,
              processingTime,
              breakdownProb,
              /*requiredCount=*/1,
              outputOverflowMode,
              broker,
              rng,
              idGen) {}

void Painter::process(int tick) {
    if (currentProduct_.empty()) return;

    // input은 BodyPart여야 하나, 정상 토폴로지에서만 보장. 검사 없이 진행.
    currentProduct_.pop_back();
    const int newId = idGen_.next();

    auto painted = std::make_unique<BodyPart>(newId);
    painted->setPainted(true);
    const auto ptype = painted->getType();

    if (tryPushOrDrop(std::move(painted), tick)) {
        publishEvent(EventType::Completed, tick, newId, ptype);
    }
}
