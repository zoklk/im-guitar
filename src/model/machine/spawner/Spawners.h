#pragma once

#include <memory>
#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class Product;
class ProductIdGen;

// Spawner: 입력 없이 새 Product를 생성. requiredCount=0이라 inputBuffer 무관하게
// 매 사이클 ProcessingState 진입 → process()에서 makeProduct → Spawned publish →
// tryPushOrDrop.
//
// Started/Completed는 발행하지 않음 (Spawned가 시스템 진입 마커 역할).
// Backpressure 모드: outputConveyor.canAccept false → canStart false → spawn skip.
// Drop 모드: 무조건 spawn, push 실패 시 tryPushOrDrop이 Drop publish.
class Spawner : public Machine {
public:
    Spawner(std::string   id,
            MachineType   type,
            int           spawnInterval,
            double        breakdownProb,
            OverflowMode  outputOverflowMode,
            EventBroker&  broker,
            std::mt19937& rng,
            ProductIdGen& idGen,
            int           maxHealth = 10);

protected:
    // 자식이 새 Product 생성. id는 호출 시점에 발급 완료된 값.
    virtual std::unique_ptr<Product> makeProduct(int newId) = 0;

    // Spawner는 Started 발행 안 함 + inputBuffer 안 다룸.
    void publishStarted(int /*tick*/) override {}
    void gatherInputs() override {}

    // 공통 process: id 발급 → makeProduct → Spawned publish → tryPushOrDrop.
    void process(int tick) override;
};

class WoodSpawner : public Spawner {
public:
    WoodSpawner(std::string id, int spawnInterval, double bp, OverflowMode mode,
                EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                int maxHealth = 10);
protected:
    std::unique_ptr<Product> makeProduct(int newId) override;
};

class BridgeSpawner : public Spawner {
public:
    BridgeSpawner(std::string id, int spawnInterval, double bp, OverflowMode mode,
                  EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                  int maxHealth = 10);
protected:
    std::unique_ptr<Product> makeProduct(int newId) override;
};

class PickupSpawner : public Spawner {
public:
    PickupSpawner(std::string id, int spawnInterval, double bp, OverflowMode mode,
                  EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
                  int maxHealth = 10);
protected:
    std::unique_ptr<Product> makeProduct(int newId) override;
};
