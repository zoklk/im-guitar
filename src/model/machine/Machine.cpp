#include "Machine.h"

#include <utility>

#include "EventBroker.h"
#include "MachineStates.h"

Machine::Machine(std::string   id,
                 MachineType   type,
                 int           processingTime,
                 double        breakdownProb,
                 int           requiredCount,
                 OverflowMode  outputOverflowMode,
                 EventBroker&  broker,
                 std::mt19937& rng,
                 ProductIdGen& idGen,
                 int           maxHealth)
    : SimulationObject(broker),
      id_(std::move(id)),
      type_(type),
      maxHealth_(maxHealth),
      health_(maxHealth),
      processingTime_(processingTime),
      breakdownProb_(breakdownProb),
      requiredCount_(requiredCount),
      outputOverflowMode_(outputOverflowMode),
      rng_(rng),
      idGen_(idGen) {
    currentState_ = &MachineIdleState::instance();
    // 초기 상태 진입 알림 (tick=0). onEnter 부수효과는 없음 (Idle::onEnter 미정의).
}

void Machine::update(int tick) {
    currentState_->update(*this, tick);
}

void Machine::acceptProduct(std::unique_ptr<Product> p) {
    inputBuffer_.push_back(std::move(p));
}

void Machine::handle(const Event& ev) {
    // Factory.applyConfig가 (Fault, downstream.id) / (Resume, downstream.id) 토픽으로
    // 구독시켜 호출. sourceId 필터링은 broker의 토픽 매칭이 처리하므로 여기서는
    // type만 분기.
    switch (ev.type) {
        case EventType::Fault:
            ++pendingDownstreamFaults_;
            break;
        case EventType::Resume:
            if (pendingDownstreamFaults_ > 0) --pendingDownstreamFaults_;
            break;
        default:
            break;
    }
}

void Machine::repair(int tick) {
    health_ = maxHealth_;
    publishEvent(EventType::Resume, tick);
    if (processingTick_ > 0) {
        transitionTo(MachineProcessingState::instance(), tick);
    } else {
        transitionTo(MachineIdleState::instance(), tick);
    }
}

void Machine::transitionTo(IMachineState& next, int tick) {
    if (currentState_) currentState_->onExit(*this, tick);
    currentState_ = &next;
    currentState_->onEnter(*this, tick);
}

bool Machine::canStart() const {
    if (static_cast<int>(inputBuffer_.size()) < requiredCount_) return false;
    if (pendingDownstreamFaults_ > 0) return false;
    if (outputConveyor_ != nullptr
        && outputOverflowMode_ == OverflowMode::Backpressure
        && !outputConveyor_->canAccept()) {
        return false;
    }
    return true;
}

bool Machine::isSuspendedByBackpressure() const {
    return outputOverflowMode_ == OverflowMode::Backpressure
           && outputConveyor_ != nullptr
           && !outputConveyor_->canAccept();
}

bool Machine::tryPushOrDrop(std::unique_ptr<Product> p, int tick) {
    if (outputConveyor_ != nullptr && outputConveyor_->canAccept()) {
        outputConveyor_->push(std::move(p), tick);
        ++outputCount_;
        return true;
    }
    // Drop 모드 (Backpressure는 canStart가 막아 여기 도달 못 함)
    const int pid = p->getId();
    const ProductType pt = p->getType();
    publishEvent(EventType::Drop, tick, pid, pt);
    // p는 함수 종료 시 unique_ptr 소멸로 자동 폐기
    return false;
}

void Machine::publishEvent(EventType type, int tick,
                           std::optional<int>         productId,
                           std::optional<ProductType> productType) {
    Event ev;
    ev.type        = type;
    ev.sourceId    = id_;
    ev.tick        = tick;
    ev.productId   = productId;
    ev.productType = productType;
    broker_.publish(ev);
}

void Machine::gatherInputs() {
    for (int i = 0; i < requiredCount_; ++i) {
        if (inputBuffer_.empty()) break;
        currentProduct_.push_back(std::move(inputBuffer_.back()));
        inputBuffer_.pop_back();
    }
}

void Machine::publishStarted(int tick) {
    std::optional<int>         pid;
    std::optional<ProductType> pt;
    if (!currentProduct_.empty()) {
        pid = currentProduct_.front()->getId();
        pt  = currentProduct_.front()->getType();
    }
    publishEvent(EventType::Started, tick, pid, pt);
}
