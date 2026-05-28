#pragma once

#include <memory>
#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class Product;
class ProductIdGen;

// Spawner: requiredCount=0. inputBuffer 무관하게 매 사이클 진입 → spawn.
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
    virtual std::unique_ptr<Product> makeProduct(int newId) = 0;

    // Spawner는 Started 미발행 + inputBuffer 미사용
    void publishStarted(int /*tick*/) override {}
    void gatherInputs() override {}

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
