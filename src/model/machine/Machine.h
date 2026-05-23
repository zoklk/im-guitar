#pragma once

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/IConveyor.h"
#include "model/machine/IMachine.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"
#include "model/sim/SimulationObject.h"

class EventBroker;
class IMachineState;

// 모든 머신의 추상 base. State 패턴(MachineIdleState/Processing/Broken) +
// 다중 입력 typedBuffer는 자식(Assembler/Collector) 책임.
//
// 발행 정책:
//   - Started/Completed: ProcessingState.onEnter / process() 성공 경로 (가상 훅 publishStarted)
//   - Spawned/Packaged/Drop: 자식 process()가 직접 발행
//   - Fault: BrokenState.onEnter
//   - Resume: repair() 본체
//
// Overflow 정책은 outputOverflowMode_ 멤버가 결정. Backpressure 모드면 canStart가
// outputConveyor.canAccept() 폴링으로 사전 차단 → push 실패 자체가 발생하지 않음.
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
            ProductIdGen& idGen);

    ~Machine() override = default;

    // SimulationObject
    void               update(int tick) override;
    const std::string& getId() const override { return id_; }

    // IMachine
    void acceptProduct(std::unique_ptr<Product> p) override;

    // IEventHandler (Fault cascade 토픽 구독자; Factory.applyConfig가 구독 등록)
    void handle(const Event& ev) override;

    // 외부 와이어링
    void setOutputConveyor(IConveyor* c) { outputConveyor_ = c; }

    // 외부 명령 (Technician.수리완료 / Controller.instantRepair / forceBreak)
    void repair(int tick);
    void forceBreak() { health_ = 0; }

    // 다형 훅
    virtual bool canStart() const;
    virtual void process(int tick) = 0;

    // State 전이 (Phase 6 Factory.snapshot에서 currentState_ 이름 노출 필요 시 getter 추가)
    void transitionTo(IMachineState& next, int tick);

    // Read-only 접근자 (Factory.snapshot / 테스트)
    MachineType  getType() const                { return type_; }
    int          getHealth() const              { return health_; }
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
    // 자식 클래스 (Cutter / Assembler / Spawner 등)가 process()에서 호출.
    // push 성공 시 true 반환. 가득 차면 (Drop 모드 전제) Drop 이벤트 publish 후 false.
    bool tryPushOrDrop(std::unique_ptr<Product> p, int tick);

    // 이벤트 발행 헬퍼.
    void publishEvent(EventType type, int tick,
                      std::optional<int>         productId   = std::nullopt,
                      std::optional<ProductType> productType = std::nullopt);

    // ProcessingState.onEnter에서 호출되는 훅 — 자식이 override 가능.
    // - 기본 gatherInputs: inputBuffer 끝에서 requiredCount만큼 currentProduct로 move.
    //   (Assembler / Collector는 typedBuffer에서 종류별로 1개씩 모음)
    // - 기본 publishStarted: 첫 currentProduct로 Started 발행.
    //   (Spawner는 no-op override — Started 의미 없음, Spawned는 process()에서 발행)
    virtual void gatherInputs();
    virtual void publishStarted(int tick);

    std::string                            id_;
    MachineType                            type_;
    int                                    health_                  = 10;
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
