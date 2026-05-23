#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "common/Types.h"
#include "model/machine/multiple/MultiInputMachine.h"

class EventBroker;
class Product;
class ProductIdGen;

// ElecPartCollector: Bridge + Pickup → ElecPartSet (sourceCount 2).
class ElecPartCollector : public MultiInputMachine {
public:
    ElecPartCollector(std::string   id,
                      int           processingTime,
                      double        breakdownProb,
                      OverflowMode  outputOverflowMode,
                      EventBroker&  broker,
                      std::mt19937& rng,
                      ProductIdGen& idGen);

protected:
    std::unique_ptr<Product> makeOutput(std::vector<std::unique_ptr<Product>> inputs,
                                        int newId) override;
};
