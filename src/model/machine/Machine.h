#pragma once

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "common/Event.h"
#include "common/FactorySnap.h"
#include "common/Types.h"
#include "model/conveyor/IConveyor.h"
#include "model/machine/IMachine.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"
#include "model/SimulationObject.h"

class EventBroker;
class IMachineState;

class Machine : public SimulationObject, public IMachine, public IEventHandler {
public:
    Machine(std::string   id,
            MachineType   type,
            int           processingTime,
            double        breakdownProb,
            int           requiredCount,
            OverflowMode  outputOverflowMode,
            EventBroker&  broker,
            std::mt19937& rng,
            ProductIdGen& idGen,
            int           maxHealth = 10);

    ~Machine() override = default;

    void               update(int tick) override;
    const std::string& getId() const override { return id_; }

    void acceptProduct(std::unique_ptr<Product> p) override;

    // Fault cascade 토픽 구독자 (Factory.applyConfig가 (Fault/Resume, downstream.id) 구독 등록)
    void handle(const Event& ev) override;

    void setOutputConveyor(IConveyor* c) { outputConveyor_ = c; }

    void repair(int tick);
    void forceBreak() { health_ = 0; }

    virtual bool canStart() const;
    virtual void process(int tick) = 0;

    void transitionTo(IMachineState& next, int tick);

    // ── 메멘토 직렬화/복원 (Factory snapshot/restore 용) ────────────
    virtual void serializeInputs(std::vector<ProductSnap>& out) const;
    virtual void clearInputs();
    void         serializeCurrentProduct(std::vector<ProductSnap>& out) const;
    void         restoreFromSnap(const MachineSnap& snap);   // currentState_ derive 포함

    MachineType  getType() const                { return type_; }
    int          getHealth() const              { return health_; }
    int          getMaxHealth() const           { return maxHealth_; }
    int          getProcessingTick() const      { return processingTick_; }
    int          getProcessingTime() const      { return processingTime_; }
    double       getBreakdownProb() const       { return breakdownProb_; }
    int          getRequiredCount() const       { return requiredCount_; }
    int          getOutputCount() const         { return outputCount_; }
    OverflowMode getOutputOverflowMode() const  { return outputOverflowMode_; }
    IConveyor*   getOutputConveyor() const      { return outputConveyor_; }
    virtual int  getInputBufferSize() const     { return static_cast<int>(inputBuffer_.size()); }
    int          getCurrentProductSize() const  { return static_cast<int>(currentProduct_.size()); }
    int          getPendingDownstreamFaults() const { return pendingDownstreamFaults_; }
    const IMachineState* getCurrentState() const { return currentState_; }
    bool         isSuspendedByBackpressure() const;

protected:
    // 자식 process()가 출력 전달에 사용. push 성공 시 true.
    // 가득 차면 (Drop 모드 전제) Drop 이벤트 publish 후 false 반환.
    bool tryPushOrDrop(std::unique_ptr<Product> p, int tick);

    void publishEvent(EventType type, int tick,
                      std::optional<int>         productId   = std::nullopt,
                      std::optional<ProductType> productType = std::nullopt);

    // ProcessingState.onEnter 훅 — Spawner / MultiInputMachine이 override.
    virtual void gatherInputs();
    virtual void publishStarted(int tick);

    std::string                            id_;
    MachineType                            type_;
    int                                    maxHealth_;
    int                                    health_;
    int                                    processingTick_          = 0;
    int                                    processingTime_;
    double                                 breakdownProb_;
    int                                    requiredCount_;
    int                                    outputCount_             = 0;
    int                                    pendingDownstreamFaults_ = 0;
    OverflowMode                           outputOverflowMode_;
    std::vector<std::unique_ptr<Product>>  inputBuffer_;
    std::vector<std::unique_ptr<Product>>  currentProduct_;
    IConveyor*                             outputConveyor_ = nullptr;
    IMachineState*                         currentState_   = nullptr;
    std::mt19937&                          rng_;
    ProductIdGen&                          idGen_;

private:
    friend class MachineIdleState;
    friend class MachineProcessingState;
    friend class MachineBrokenState;
};
