#include "Cutters.h"

#include <utility>

#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"

Cutter::Cutter(std::string   id,
               MachineType   type,
               int           processingTime,
               double        breakdownProb,
               OverflowMode  outputOverflowMode,
               EventBroker&  broker,
               std::mt19937& rng,
               ProductIdGen& idGen,
               int           maxHealth)
    : Machine(std::move(id),
              type,
              processingTime,
              breakdownProb,
              /*requiredCount=*/1,
              outputOverflowMode,
              broker,
              rng,
              idGen,
              maxHealth) {}

void Cutter::process(int tick) {
    if (currentProduct_.empty()) return;   // 정상 흐름 도달 불가, 방어

    auto       input  = std::move(currentProduct_.back());
    currentProduct_.pop_back();
    const int  newId  = idGen_.next();
    auto       output = makeOutput(std::move(input), newId);
    const auto ptype  = output->getType();

    if (tryPushOrDrop(std::move(output), tick)) {
        publishEvent(EventType::Completed, tick, newId, ptype);
    }
}

HeadCutter::HeadCutter(std::string id, int processingTime, double bp, OverflowMode mode,
                       EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                       int maxHealth)
    : Cutter(std::move(id), MachineType::HeadCutter,
             processingTime, bp, mode, broker, rng, idGen, maxHealth) {}

std::unique_ptr<Product> HeadCutter::makeOutput(std::unique_ptr<Product> /*input*/, int newId) {
    return std::make_unique<HeadPart>(newId);
}

NeckCutter::NeckCutter(std::string id, int processingTime, double bp, OverflowMode mode,
                       EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                       int maxHealth)
    : Cutter(std::move(id), MachineType::NeckCutter,
             processingTime, bp, mode, broker, rng, idGen, maxHealth) {}

std::unique_ptr<Product> NeckCutter::makeOutput(std::unique_ptr<Product> /*input*/, int newId) {
    return std::make_unique<NeckPart>(newId);
}

BodyCutter::BodyCutter(std::string id, int processingTime, double bp, OverflowMode mode,
                       EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                       int maxHealth)
    : Cutter(std::move(id), MachineType::BodyCutter,
             processingTime, bp, mode, broker, rng, idGen, maxHealth) {}

std::unique_ptr<Product> BodyCutter::makeOutput(std::unique_ptr<Product> /*input*/, int newId) {
    return std::make_unique<BodyPart>(newId);
}
