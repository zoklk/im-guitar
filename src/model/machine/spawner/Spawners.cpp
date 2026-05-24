#include "Spawners.h"

#include <utility>

#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"

Spawner::Spawner(std::string   id,
                 MachineType   type,
                 int           spawnInterval,
                 double        breakdownProb,
                 OverflowMode  outputOverflowMode,
                 EventBroker&  broker,
                 std::mt19937& rng,
                 ProductIdGen& idGen,
                 int           maxHealth)
    : Machine(std::move(id),
              type,
              spawnInterval,
              breakdownProb,
              /*requiredCount=*/0,
              outputOverflowMode,
              broker,
              rng,
              idGen,
              maxHealth) {}

void Spawner::process(int tick) {
    const int  newId   = idGen_.next();
    auto       product = makeProduct(newId);
    const auto ptype   = product->getType();

    // Spawned 먼저 publish (WIP 회계: wip += sourceCount). push 결과와 무관.
    publishEvent(EventType::Spawned, tick, newId, ptype);

    // tryPushOrDrop: 성공 시 outputCount++, 실패(Drop 모드 한정) 시 Drop publish.
    // Backpressure 모드는 canStart가 사전 차단하므로 여기 도달 시 push는 성공.
    tryPushOrDrop(std::move(product), tick);
}

// ── 구체 Spawner 3종 ─────────────────────────────────────────

WoodSpawner::WoodSpawner(std::string id, int spawnInterval, double bp, OverflowMode mode,
                         EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                         int maxHealth)
    : Spawner(std::move(id), MachineType::WoodSpawner,
              spawnInterval, bp, mode, broker, rng, idGen, maxHealth) {}

std::unique_ptr<Product> WoodSpawner::makeProduct(int newId) {
    return std::make_unique<RawWood>(newId);
}

BridgeSpawner::BridgeSpawner(std::string id, int spawnInterval, double bp, OverflowMode mode,
                             EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                             int maxHealth)
    : Spawner(std::move(id), MachineType::BridgeSpawner,
              spawnInterval, bp, mode, broker, rng, idGen, maxHealth) {}

std::unique_ptr<Product> BridgeSpawner::makeProduct(int newId) {
    return std::make_unique<Bridge>(newId);
}

PickupSpawner::PickupSpawner(std::string id, int spawnInterval, double bp, OverflowMode mode,
                             EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                             int maxHealth)
    : Spawner(std::move(id), MachineType::PickupSpawner,
              spawnInterval, bp, mode, broker, rng, idGen, maxHealth) {}

std::unique_ptr<Product> PickupSpawner::makeProduct(int newId) {
    return std::make_unique<Pickup>(newId);
}
