#pragma once

#include <memory>
#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class Product;
class ProductIdGen;

// Cutter: 1→1 변환. 모든 변환에 새 product id 발급 (input id 계승하지 않음).
class Cutter : public Machine {
public:
    Cutter(std::string   id,
           MachineType   type,
           int           processingTime,
           double        breakdownProb,
           OverflowMode  outputOverflowMode,
           EventBroker&  broker,
           std::mt19937& rng,
           ProductIdGen& idGen,
           int           maxHealth = 10);

protected:
    virtual std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) = 0;

    void process(int tick) override;
};

class HeadCutter : public Cutter {
public:
    HeadCutter(std::string id, int processingTime, double bp, OverflowMode mode,
               EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
               int maxHealth = 10);
protected:
    std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) override;
};

class NeckCutter : public Cutter {
public:
    NeckCutter(std::string id, int processingTime, double bp, OverflowMode mode,
               EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
               int maxHealth = 10);
protected:
    std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) override;
};

class BodyCutter : public Cutter {
public:
    BodyCutter(std::string id, int processingTime, double bp, OverflowMode mode,
               EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen,
               int maxHealth = 10);
protected:
    std::unique_ptr<Product> makeOutput(std::unique_ptr<Product> input, int newId) override;
};
