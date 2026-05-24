#include "MultiInputMachine.h"

#include <utility>

#include "model/conveyor/IConveyor.h"
#include "model/event/EventBroker.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"

MultiInputMachine::MultiInputMachine(std::string              id,
                                     MachineType              type,
                                     int                      processingTime,
                                     double                   breakdownProb,
                                     std::vector<ProductType> requiredTypes,
                                     OverflowMode             outputOverflowMode,
                                     EventBroker&             broker,
                                     std::mt19937&            rng,
                                     ProductIdGen&            idGen,
                                     int                      maxHealth)
    : Machine(std::move(id),
              type,
              processingTime,
              breakdownProb,
              /*requiredCount=*/static_cast<int>(requiredTypes.size()),
              outputOverflowMode,
              broker,
              rng,
              idGen,
              maxHealth),
      requiredTypes_(std::move(requiredTypes)) {}

void MultiInputMachine::acceptProduct(std::unique_ptr<Product> p) {
    const ProductType t = p->getType();
    typedBuffer_[t].push_back(std::move(p));
    // 정상 토폴로지가 보장하는 한 t는 requiredTypes_ 중 하나.
    // 비정상 type 유입 시 typedBuffer_에 누적만 됨 (canStart에 영향 없음).
}

bool MultiInputMachine::canStart() const {
    // Backpressure / Fault cascade 분기는 base와 동일
    if (pendingDownstreamFaults_ > 0) return false;
    if (outputConveyor_ != nullptr
        && outputOverflowMode_ == OverflowMode::Backpressure
        && !outputConveyor_->canAccept()) {
        return false;
    }
    // 모든 requiredType에 최소 1개씩 있어야 함
    for (ProductType t : requiredTypes_) {
        auto it = typedBuffer_.find(t);
        if (it == typedBuffer_.end() || it->second.empty()) return false;
    }
    return true;
}

int MultiInputMachine::getInputBufferSize() const {
    int total = 0;
    for (const auto& [_, q] : typedBuffer_) {
        total += static_cast<int>(q.size());
    }
    return total;
}

void MultiInputMachine::gatherInputs() {
    // canStart 통과 후 호출이라 각 큐에 ≥1 보장.
    // requiredTypes_ 순서대로 1개씩 currentProduct_로 move → publishStarted가 첫 input(=requiredTypes_[0])을 대표로.
    for (ProductType t : requiredTypes_) {
        auto& q = typedBuffer_[t];
        if (q.empty()) continue;   // 방어 (정상 흐름 도달 불가)
        currentProduct_.push_back(std::move(q.back()));
        q.pop_back();
    }
}

void MultiInputMachine::process(int tick) {
    if (currentProduct_.empty()) return;

    // currentProduct_의 모든 unique_ptr를 inputs vector로 이전
    std::vector<std::unique_ptr<Product>> inputs;
    inputs.reserve(currentProduct_.size());
    for (auto& p : currentProduct_) inputs.push_back(std::move(p));
    currentProduct_.clear();

    const int  newId  = idGen_.next();
    auto       output = makeOutput(std::move(inputs), newId);
    const auto ptype  = output->getType();

    if (tryPushOrDrop(std::move(output), tick)) {
        publishEvent(EventType::Completed, tick, newId, ptype);
    }
}
