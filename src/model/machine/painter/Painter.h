#pragma once

#include <memory>
#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class Product;
class ProductIdGen;

// Painter: BodyPart → isPainted=true인 새 BodyPart (입력 id 폐기, 새 id 발급).
class Painter : public Machine {
public:
    Painter(std::string   id,
            int           processingTime,
            double        breakdownProb,
            OverflowMode  outputOverflowMode,
            EventBroker&  broker,
            std::mt19937& rng,
            ProductIdGen& idGen,
            int           maxHealth = 10);

protected:
    void process(int tick) override;
};
