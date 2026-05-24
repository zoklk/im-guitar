#include "ElecPartCollector.h"

#include <utility>

#include "model/product/Product.h"

ElecPartCollector::ElecPartCollector(std::string   id,
                                     int           processingTime,
                                     double        breakdownProb,
                                     OverflowMode  outputOverflowMode,
                                     EventBroker&  broker,
                                     std::mt19937& rng,
                                     ProductIdGen& idGen,
                                     int           maxHealth)
    : MultiInputMachine(std::move(id),
                        MachineType::ElecPartCollector,
                        processingTime,
                        breakdownProb,
                        /*requiredTypes=*/{ProductType::Bridge, ProductType::Pickup},
                        outputOverflowMode,
                        broker,
                        rng,
                        idGen,
                        maxHealth) {}

std::unique_ptr<Product> ElecPartCollector::makeOutput(
    std::vector<std::unique_ptr<Product>> /*inputs*/, int newId) {
    return std::make_unique<ElecPartSet>(newId);
}
