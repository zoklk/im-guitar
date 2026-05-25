#include "Machine.h"

#include <utility>

#include "EventBroker.h"
#include "MachineStates.h"
#include "model/product/Product.h"
#include "model/product/ProductSnap.h"

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
}

void Machine::update(int tick) {
    currentState_->update(*this, tick);
}

void Machine::acceptProduct(std::unique_ptr<Product> p) {
    inputBuffer_.push_back(std::move(p));
}

void Machine::handle(const Event& ev) {
    // sourceId 필터링은 broker의 토픽 매칭이 담당 → 여기선 type만 분기.
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

// ── 메멘토 ─────────────────────────────────────────────────────────
void Machine::serializeInputs(std::vector<ProductSnap>& out) const {
    out.reserve(out.size() + inputBuffer_.size());
    for (const auto& p : inputBuffer_) {
        if (p) out.push_back(productToSnap(*p));
    }
}

void Machine::clearInputs() {
    inputBuffer_.clear();
}

void Machine::serializeCurrentProduct(std::vector<ProductSnap>& out) const {
    out.reserve(out.size() + currentProduct_.size());
    for (const auto& p : currentProduct_) {
        if (p) out.push_back(productToSnap(*p));
    }
}

void Machine::restoreFromSnap(const MachineSnap& snap) {
    health_         = snap.health;
    processingTick_ = snap.progress;
    outputCount_    = snap.outputCount;
    pendingDownstreamFaults_ = 0;   // 토픽 구독이 재설치되며 자연 재계산되도록 0으로 시작

    // currentProduct_ 복원
    currentProduct_.clear();
    for (const auto& ps : snap.currentProduct) {
        auto p = productFromSnap(ps);
        if (p) currentProduct_.push_back(std::move(p));
    }

    // inputBuffer_ 복원 — 다형성 활용 (MultiInputMachine.acceptProduct가 typedBuffer로 분류)
    clearInputs();
    for (const auto& ps : snap.inputBuffer) {
        auto p = productFromSnap(ps);
        if (p) acceptProduct(std::move(p));
    }

    // currentState_ derive: health==0 → Broken, currentProduct/progress 있으면 Processing, else Idle
    if (health_ <= 0) {
        currentState_ = &MachineBrokenState::instance();
    } else if (!currentProduct_.empty() || processingTick_ > 0) {
        currentState_ = &MachineProcessingState::instance();
    } else {
        currentState_ = &MachineIdleState::instance();
    }
}
