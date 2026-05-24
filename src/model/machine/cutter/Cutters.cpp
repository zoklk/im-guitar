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
    // currentProduct에 1개 들어있는 것이 ProcessingState.onEnter의 gatherInputs 결과로 보장.
    // 비정상 호출 방어: 비어있으면 skip (단, 이 경로는 정상 흐름에서 도달 불가).
    if (currentProduct_.empty()) return;

    auto       input  = std::move(currentProduct_.back());
    currentProduct_.pop_back();
    const int  newId  = idGen_.next();
    auto       output = makeOutput(std::move(input), newId);
    const auto ptype  = output->getType();

    if (tryPushOrDrop(std::move(output), tick)) {
        publishEvent(EventType::Completed, tick, newId, ptype);
    }
    // push 실패 시: tryPushOrDrop이 이미 Drop publish, Completed는 발행하지 않음
}

// ── 구체 Cutter 3종 ─────────────────────────────────────────

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
