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

// BodyAssembler: HeadPart + NeckPart + BodyPart(painted) → AssembledBody
class BodyAssembler : public MultiInputMachine {
public:
    BodyAssembler(std::string   id,
                  int           processingTime,
                  double        breakdownProb,
                  OverflowMode  outputOverflowMode,
                  EventBroker&  broker,
                  std::mt19937& rng,
                  ProductIdGen& idGen,
                  int           maxHealth = 10);

protected:
    std::unique_ptr<Product> makeOutput(std::vector<std::unique_ptr<Product>> inputs,
                                        int newId) override;
};

// PartAssembler: AssembledBody + ElecPartSet → FinishedGuitar
class PartAssembler : public MultiInputMachine {
public:
    PartAssembler(std::string   id,
                  int           processingTime,
                  double        breakdownProb,
                  OverflowMode  outputOverflowMode,
                  EventBroker&  broker,
                  std::mt19937& rng,
                  ProductIdGen& idGen,
                  int           maxHealth = 10);

protected:
    std::unique_ptr<Product> makeOutput(std::vector<std::unique_ptr<Product>> inputs,
                                        int newId) override;
};
