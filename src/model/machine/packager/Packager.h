#pragma once

#include <random>
#include <string>

#include "common/Types.h"
#include "model/machine/Machine.h"

class EventBroker;
class ProductIdGen;

// FinishedGuitar 소비 (sink). 출력 conveyor 없음.
class Packager : public Machine {
public:
    Packager(std::string   id,
             int           processingTime,
             double        breakdownProb,
             EventBroker&  broker,
             std::mt19937& rng,
             ProductIdGen& idGen,
             int           maxHealth = 10);

protected:
    void process(int tick) override;
};
