#include "Assemblers.h"

#include <utility>

#include "model/product/Product.h"

// ── BodyAssembler ───────────────────────────────────────────

BodyAssembler::BodyAssembler(std::string   id,
                             int           processingTime,
                             double        breakdownProb,
                             OverflowMode  outputOverflowMode,
                             EventBroker&  broker,
                             std::mt19937& rng,
                             ProductIdGen& idGen,
                             int           maxHealth)
    : MultiInputMachine(std::move(id),
                        MachineType::BodyAssembler,
                        processingTime,
                        breakdownProb,
                        /*requiredTypes=*/{ProductType::HeadPart,
                                           ProductType::NeckPart,
                                           ProductType::BodyPart},
                        outputOverflowMode,
                        broker,
                        rng,
                        idGen,
                        maxHealth) {}

std::unique_ptr<Product> BodyAssembler::makeOutput(
    std::vector<std::unique_ptr<Product>> /*inputs*/, int newId) {
    return std::make_unique<AssembledBody>(newId);
}

// ── PartAssembler ───────────────────────────────────────────

PartAssembler::PartAssembler(std::string   id,
                             int           processingTime,
                             double        breakdownProb,
                             OverflowMode  outputOverflowMode,
                             EventBroker&  broker,
                             std::mt19937& rng,
                             ProductIdGen& idGen,
                             int           maxHealth)
    : MultiInputMachine(std::move(id),
                        MachineType::PartAssembler,
                        processingTime,
                        breakdownProb,
                        /*requiredTypes=*/{ProductType::AssembledBody,
                                           ProductType::ElecPartSet},
                        outputOverflowMode,
                        broker,
                        rng,
                        idGen,
                        maxHealth) {}

std::unique_ptr<Product> PartAssembler::makeOutput(
    std::vector<std::unique_ptr<Product>> /*inputs*/, int newId) {
    return std::make_unique<FinishedGuitar>(newId);
}
