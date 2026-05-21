# Phase 3 — Machine + State 패턴

## 목표

머신 계층 전체 (Spawner 포함). 상태는 State 패턴 (싱글톤 + DI).

## 적용 패턴

- **State** (싱글톤 + DI): `IMachineState` 인터페이스 + `IdleState` / `ProcessingState` / `BrokenState` 싱글톤. Machine은 `currentState_: IMachineState*` 참조 보유. 전이는 `transitionTo(IMachineState&)`
- **Template Method**: `process()` 자식 구현, 공통 update 골격은 Machine + State에서

## 구성요소

### `IMachineState`

```
virtual void update(Machine&, int tick) = 0
virtual void onEnter(Machine&)  {}
virtual void onExit(Machine&)   {}
virtual const char* name() const = 0
static <Concrete>& instance()       // 각 구체 클래스가 자기 싱글톤 반환
```

자식:
- **IdleState**: `m.canStart()` 호출 (자식 override 가능, 기본은 `inputBuffer.size() >= requiredCount`). SmartFactory에서는 `m.outputConveyor->canAccept()`도 함께 검사. true → `ProcessingState` 전이
- **ProcessingState**:
  - `onEnter`: inputBuffer에서 requiredCount만큼 move → currentProduct, `Started` publish
  - `update`: `m.processingTick++`. 매 틱 health drop 체크 (`m.rng()` 사용). processingTime 도달 시 → `m.process()` → outputConveyor push → `outputCount++` → `Completed` publish → `IdleState` 전이. health 0 도달 시 → `BrokenState` 전이 (processingTick / currentProduct 유지)
- **BrokenState**:
  - `onEnter`: `Fault` publish, `statistics_.breakdowns++`
  - `update`: no-op (외부 repair 대기)
  - Machine에 `repair()` 호출 시 → health 10 복원 → processingTick > 0이면 ProcessingState 복귀 / else IdleState

### `Machine` (추상, SimulationObject 자식)

```
id_, type_
health_: int (0~10)
processingTick_, processingTime_
breakdownProb_: double
requiredCount_: int
inputBuffer_: vector<unique_ptr<Product>>
currentProduct_: vector<unique_ptr<Product>>  // 처리 중 입력들 (requiredCount만큼)
outputConveyor_: Conveyor*
currentState_: IMachineState*
outputCount_: int
suspendedByBackpressure_: bool                // SmartFactory에서만 사용 — 하류 conveyor 포화
pendingDownstreamFaults_: int                 // SmartFactory에서만 사용 — 고장난 하류 머신 수 (refcount)

update(tick): currentState_->update(*this, tick)
transitionTo(IMachineState&): cur.onExit → 갱신 → new.onEnter

virtual void acceptProduct(unique_ptr<Product>)  // 기본: inputBuffer_.push_back. Assembler/Collector override
virtual bool canStart() const                    // 기본: inputBuffer.size() >= requiredCount && !suspendedByBackpressure && pendingDownstreamFaults_ == 0. Assembler/Collector override
virtual void process() = 0                       // currentProduct → 출력물 생성 → outputConveyor push
repair()                                         // Technician / InstantRepair 호출용
forceBreak()                                     // health = 0 설정 (다음 update에서 자연스레 Broken 전이)
```

`rng()`는 Factory에서 주입받은 `mt19937&` 반환. ProcessingState가 health drop 체크에 사용.

### Backpressure 구독 (SmartFactory)

Machine이 `IEventHandler` 구현. `outputConveyor->getOverflowMode() == Backpressure`일 때 생성 시 `broker.subscribe(Backpressure, outputConveyor->getId(), this)` 토픽 구독:

- `handle(Event)`: type == Backpressure && sourceId == outputConveyor.id → `suspendedByBackpressure_ = true`
- Resume 메커니즘: 매 IdleState.update에서 `outputConveyor->canAccept()` 폴링. true면 `suspendedByBackpressure_ = false`로 자체 해제 (이벤트 페어링 불필요)
- ProcessingState 진입 중일 때 일시정지는: 다음 IdleState 진입 시 canStart가 false면 머무는 방식. 진행 중인 제품은 마저 완료

### Fault Cascade 구독 (SmartFactory)

하류 머신 고장 시 상류 전체를 정지시키는 메커니즘. **SmartFactory 한정** — Drop 시나리오는 lost 카운트로 표현, cascade 없음.

파이프라인이 DAG (예: BridgeSpawner / PickupSpawner → ElecPartCollector ↗ PartAssembler, BodyAssembler ↗ PartAssembler)라 단순 Packager-거리 metric으로는 cascade 범위를 결정 못 함. 대신 **DAG 하류 transitive closure**를 Factory가 구축 시점에 계산하여 토픽 구독을 일괄 등록.

- 구독 등록 책임: Phase 6 `Factory.applyConfig` (Machine 생성자 X — 와이어링 완료 후 그래프 traversal이 필요해서)
- 각 Machine M에 대해 하류 closure `D(M) = M의 직접/간접 하류 머신 전체` 계산 → `X ∈ D(M)`마다:
  - `broker.subscribe(EventType::Fault,  X.id, M)`
  - `broker.subscribe(EventType::Resume, X.id, M)`

`handle(Event)` 분기:
- `Backpressure` + sourceId == outputConveyor.id → `suspendedByBackpressure_ = true`
- `Fault`  + sourceId ∈ D(self) → `pendingDownstreamFaults_++`
- `Resume` + sourceId ∈ D(self) → `pendingDownstreamFaults_--` (≥ 0 유지)

> **카운터인 이유**: 복수 하류가 동시에 고장날 수 있음 (예: ElecPartCollector + PartAssembler 동시 Fault → BridgeSpawner는 둘 다 Resume될 때까지 정지 유지). bool 플래그면 먼저 Resume 받은 쪽이 잘못 해제할 수 있음. refcount로 안전.

Resume 발행 시점은 Phase 4 (Technician.수리완료) / Phase 6 (instantRepair) 양쪽에서 `broker.publish(Resume, machine.id)` 호출. Resume 이벤트 자체는 Phase 4 명세상 "EventLog 마커"였으나, fault cascade 도입으로 **실제 dispatch 의미를 갖게 됨** (EventLog는 그대로 수신, 추가로 상류 Machine들이 카운터 감소).

ProcessingState 진입 중 cascade 수신 시: Backpressure와 동일하게 진행 중인 제품은 마저 완료, 다음 IdleState에서 canStart가 false라 머무는 방식.

### Spawner 흡수

Spawner = `requiredCount_ = 0`인 Machine. `canStart()` 기본 동작상 항상 true → 매 주기 ProcessingState. `processingTime_` = spawn 주기. `process()`가 새 Product 생성 (`make_unique`). inputBuffer는 항상 비어있음. 고장/수리 로직 일반 Machine과 동일. SmartFactory에서는 outputConveyor 포화 시 Backpressure 구독으로 정지.

### 구체 클래스 11종

| 카테고리 | 클래스 | requiredCount | process() |
|---|---|---|---|
| Spawner | WoodSpawner | 0 | make_unique<RawWood> |
| | BridgeSpawner | 0 | make_unique<Bridge> |
| | PickupSpawner | 0 | make_unique<Pickup> |
| Cutter (추상) | HeadCutter | 1 | RawWood → HeadPart |
| | NeckCutter | 1 | RawWood → NeckPart |
| | BodyCutter | 1 | RawWood → BodyPart |
| | Painter | 1 | BodyPart.isPainted=true |
| | ElecPartCollector | 2 (종류별 1) | Bridge + Pickup → ElecPartSet |
| Assembler (추상) | BodyAssembler | 3 (종류별 1) | Head + Neck + paintedBody → AssembledBody |
| | PartAssembler | 2 (종류별 1) | AssembledBody + ElecPartSet → FinishedGuitar |
| | Packager | 1 | FinishedGuitar → 출고 (unique_ptr 소멸), `statistics_.finished++` |

### 다중 입력 처리

BodyAssembler / PartAssembler / ElecPartCollector는 종류별 입력 필요.

- `Assembler` / `Collector` 추상이 `unordered_map<ProductType, vector<unique_ptr<Product>>>` typedBuffer 보유
- `acceptProduct` override: product type 보고 해당 typedBuffer push
- `canStart` override: 모든 필요 type에 1개 이상 있는지 검사
- ProcessingState.onEnter에서 currentProduct 채울 때도 자식이 typedBuffer에서 종류별 1개씩 move (Machine에 `gatherInputs()` 가상 메서드 별도 필요)

> 동타입 다중 입력 (예: head-head-neck) 가드는 추가 안 함 — 현재 파이프라인 구조상 발생 불가능. `acceptProduct`에서 typedBuffer가 무한 증식할 가능성만 존재하나 정상 시나리오 아님

### 이벤트 발행 정리

| 시점 | EventType | sourceId |
|---|---|---|
| ProcessingState.onEnter | Started | machine.id |
| ProcessingState.update (완료 직후) | Completed | machine.id |
| BrokenState.onEnter | Fault | machine.id |
| `repair()` 완료 직후 | Resume | machine.id |

> Resume 발행 위치는 `Machine.repair()` 본체 — Technician 경로(Phase 4)와 instantRepair 경로(Phase 6) 모두 같은 메서드 거치므로 한 곳에서 처리.

## 의존성

- Phase 1 (EventBroker, Product, Statistics, EventLog)
- Phase 2 (SimulationObject, Conveyor)
- RNG는 Factory에서 주입 (Phase 6)

## 테스트

`tests/phase_3_machine.cpp` (mock Conveyor + 더미 broker):

- IdleState → ProcessingState 전이: inputBuffer에 requiredCount만큼 push → 1틱 후 ProcessingState
- ProcessingState 진행: processingTick 증가, processingTime 도달 시 process() 호출 + Completed publish + IdleState 복귀
- BrokenState 전이: forceBreak() → 다음 update에서 BrokenState 진입, Fault publish, statistics.breakdowns++
- repair() 호출: BrokenState → ProcessingState (processingTick > 0) 혹은 IdleState 복귀
- Backpressure 구독: SmartFactory 모드 Machine, broker.publish(Backpressure, outputConveyor.id) → 다음 IdleState에서 정지. outputConveyor.canAccept → true 되면 자동 재개
- Fault cascade: SmartFactory 모드, broker.publish(Fault, downstreamMachine.id) → 대상 Machine `pendingDownstreamFaults_++` → canStart false. broker.publish(Resume, downstreamMachine.id) → 카운터-- → 0이면 재개. 복수 하류 동시 Fault 시 모두 Resume될 때까지 정지 유지 (refcount 검증)
- 다중 입력 (BodyAssembler): typedBuffer에 head/neck/paintedBody 각 1 → canStart true, process() 호출. 한 종류만 있으면 canStart false
- Spawner: 매 processingTime 주기마다 1개 생성, outputConveyor.push 호출

## 산출 브랜치

`back/feat/machines`

## 후속

- `gatherInputs` 가상 메서드 시그니처 확정 (구현 시)
