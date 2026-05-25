# Phase 3 — Machine + State 패턴

## 목표

머신 계층 전체 (Spawner 포함). 상태는 State 패턴 (싱글톤 + DI). Overflow 정책(Drop/Backpressure)도 Machine이 보유.

## 적용 패턴

- **State** (싱글톤 + DI): `IMachineState` 인터페이스 + `MachineIdleState` / `MachineProcessingState` / `MachineBrokenState` 싱글톤. Machine은 `currentState_: IMachineState*` 참조 보유. 전이는 `transitionTo(IMachineState&)`
- **Template Method**: `process()` 자식 구현, 공통 update 골격은 Machine + State에서

> 클래스명 접두사 (`Machine*`): Technician의 `TechnicianIdleState` / `TechnicianWorkingState`와 글로벌 네임스페이스에서 이름 충돌 회피.

## 구성요소

### `IMachineState`

```
virtual void update(Machine&, int tick) = 0
virtual void onEnter(Machine&, int tick) {}
virtual void onExit(Machine&, int tick)  {}
virtual const char* name() const = 0
static <Concrete>& instance()       // 각 구체 클래스가 자기 싱글톤 반환
```

onEnter/onExit에도 tick을 받는 이유: 상태 전이 시점에 이벤트 발행 필요 (예: BrokenState.onEnter의 Fault publish). 모든 시그니처가 tick을 일관되게 받음.

자식:
- **MachineIdleState**: `m.canStart()` 호출 (자식 override 가능, 기본은 `inputBuffer.size() >= requiredCount`). Backpressure 모드면 `m.outputConveyor->canAccept()`도 함께 검사 (false면 머무름). true → `MachineProcessingState` 전이
- **MachineProcessingState**:
  - `onEnter`: currentProduct가 비어있으면 새 처리 사이클 (gatherInputs + processingTick=0 + Started publish). 비어있지 않으면 Broken에서 repair 복귀로 간주, 상태 보존하고 no-op
  - Started publish: sourceId=machine.id, productId/Type=대표 input 1개 (MultiInputMachine은 requiredTypes_[0] 종류)
  - `update`: `m.processingTick++`. 매 틱 health drop 체크 (`m.rng()` 사용 — 아래 알고리즘). processingTime 도달 시 → `m.process()` 호출 → push 결과에 따라:
    - **push 성공**: `Completed` publish (productId/Type=output), `outputCount++`, `MachineIdleState` 복귀
    - **push 실패 + Drop 모드**: Machine이 직접 `Drop` publish (productId/Type=output), product 폐기, `Completed`는 발행하지 않음, `MachineIdleState` 복귀
    - **push 실패 + Backpressure 모드**: 발생 불가 — canStart가 막아서 process까지 안 옴 (만약 발생하면 Conveyor.push가 std::abort)
  - health 0 도달 시 → `MachineBrokenState` 전이 (processingTick / currentProduct 유지)
- **MachineBrokenState**:
  - `onEnter`: `Fault` publish (sourceId=machine.id)
  - `update`: no-op (외부 repair 대기)
  - Machine에 `repair()` 호출 시 → health 10 복원 → `Resume` publish → processingTick > 0이면 ProcessingState 복귀 / else IdleState

### Health drop 알고리즘

ProcessingState.update 매 틱마다:
```
if (uniform_real(rng) < m.breakdownProb) {
    m.health -= 1;
    if (m.health == 0) transitionTo(MachineBrokenState);
}
```

`breakdownProb`는 머신 인스턴스별 시나리오 JSON 파라미터. Breakdowns 시나리오에서 큰 값. health=10에서 시작, breakdownProb 적용으로 점진 감소. health bar UI가 시각화.

### `Machine` (추상, SimulationObject 자식, IEventHandler 자식)

```
id_, type_
health_: int (0~10, default 10)
processingTick_, processingTime_
breakdownProb_: double
requiredCount_: int
inputBuffer_: vector<unique_ptr<Product>>
currentProduct_: vector<unique_ptr<Product>>  // 처리 중 입력들 (requiredCount만큼)
outputConveyor_: Conveyor*
outputOverflowMode_: OverflowMode             // Factory가 시나리오 JSON의 conveyor 모드 읽어서 주입
currentState_: IMachineState*
outputCount_: int
pendingDownstreamFaults_: int                 // SmartFactory에서만 사용 — 고장난 하류 머신 수 (refcount)

update(tick): currentState_->update(*this, tick)
transitionTo(IMachineState&): cur.onExit → 갱신 → new.onEnter

virtual bool canAcceptProduct(ProductType) const // 기본: inputBuffer_.empty()일 때만 true (1머신 1product). Conveyor가 사전 폴링.
virtual void acceptProduct(unique_ptr<Product>)  // 기본: inputBuffer_.push_back. MultiInputMachine이 typedBuffer 분류로 override
virtual bool canStart() const                    // 기본: inputBuffer.size() >= requiredCount && pendingDownstreamFaults_ == 0 && (Drop모드 || outputConveyor->canAccept()). MultiInputMachine이 typedBuffer 검사로 override
virtual void process(int tick) = 0               // currentProduct → output 생성 → tryPushOrDrop()으로 출력
virtual void gatherInputs()                      // 기본: inputBuffer 끝에서 requiredCount만큼 currentProduct로 move. MultiInputMachine이 typedBuffer 종류별 1개씩으로 override
virtual void publishStarted(int tick)            // 기본: 첫 currentProduct 정보로 Started publish. Spawner가 no-op으로 override
repair(int tick)                                 // Technician / InstantRepair 호출용, Resume publish
forceBreak()                                     // health = 0 설정 (다음 update에서 자연스레 Broken 전이)

// helper (Drop 모드 push 분기 한 곳에 모음)
bool tryPushOrDrop(unique_ptr<Product> out, int tick)

// Phase 6 메멘토 — 자기 자신을 *Snap으로/으로부터 복원 (friend 없이 캡슐화 유지)
virtual void serializeInputs(vector<ProductSnap>& out) const   // 기본: inputBuffer_. MultiInputMachine override (typedBuffer 합치기)
virtual void clearInputs()                                     // 기본: inputBuffer_.clear. MultiInputMachine override (typedBuffer_.clear)
void         serializeCurrentProduct(vector<ProductSnap>& out) const
void         restoreFromSnap(const MachineSnap&)               // 필드 일괄 + currentState_ derive
```

`tryPushOrDrop`:
```
if (outputConveyor->canAccept()) {
    outputConveyor->push(std::move(out), tick);
    return true;          // 호출자가 Completed publish
}
// Drop 모드 (Backpressure는 canStart가 막아 여기 못 옴)
broker.publish({Drop, this->id_, tick, out.id, out.type});
// out은 함수 종료 시 unique_ptr 소멸로 자동 폐기
return false;             // 호출자가 Completed 발행 skip
```

`rng()`는 Factory에서 주입받은 `mt19937&` 반환.

### Backpressure 처리 (polling-only)

Backpressure 모드 머신은 매 IdleState 진입 시 `outputConveyor->canAccept()` 폴링:
- false → `canStart` false → IdleState 머무름. **이벤트 구독 없음**.
- true → 정상 처리 진행

Backpressure 정지/재개를 위한 EventBroker 토픽 구독은 없음. dataflow는 전적으로 polling으로 결정. Backpressure 이벤트 자체는 EventLog 가시성 목적의 보조 신호로만 (실제 publisher는 phase 6 결정).

> 기존 설계 (Conveyor가 publish, Machine이 구독)와 비교: cascade는 물리적으로 자연 전파 (하류 conveyor 가득 → 머신 B 정지 → 머신 B 입력 conveyor 차오름 → 머신 A 출력 conveyor 차오름 → 머신 A polling으로 정지). 이벤트 페어링 없이도 동작.

### Fault Cascade 구독 (SmartFactory)

하류 머신 고장 시 상류 전체를 정지시키는 메커니즘. **SmartFactory 한정** — Drop 시나리오는 lost 카운트로 표현, cascade 없음.

파이프라인이 DAG (예: BridgeSpawner / PickupSpawner → ElecPartCollector ↗ PartAssembler, BodyAssembler ↗ PartAssembler)라 단순 Packager-거리 metric으로는 cascade 범위를 결정 못 함. 대신 **DAG 하류 transitive closure**를 Factory가 구축 시점에 계산하여 토픽 구독을 일괄 등록.

- 구독 등록 책임: Phase 6 `Factory.applyConfig` (Machine 생성자 X — 와이어링 완료 후 그래프 traversal이 필요해서)
- 각 Machine M에 대해 하류 closure `D(M) = M의 직접/간접 하류 머신 전체` 계산 → `X ∈ D(M)`마다:
  - `broker.subscribe(EventType::Fault,  X.id, M)`
  - `broker.subscribe(EventType::Resume, X.id, M)`

`handle(Event)` 분기:
- `Fault`  + sourceId ∈ D(self) → `pendingDownstreamFaults_++`
- `Resume` + sourceId ∈ D(self) → `pendingDownstreamFaults_--` (≥ 0 유지)

> **카운터인 이유**: 복수 하류가 동시에 고장날 수 있음. bool 플래그면 먼저 Resume 받은 쪽이 잘못 해제. refcount로 안전.

Resume 발행 시점은 Phase 4 (Technician.수리완료) / Phase 6 (instantRepair) 양쪽에서 `Machine.repair()` 본체가 `broker.publish(Resume, machine.id)` 호출.

ProcessingState 진입 중 cascade 수신 시: 진행 중인 제품은 마저 완료, 다음 IdleState에서 canStart가 false라 머무는 방식.

### Spawner 흡수

Spawner = `requiredCount_ = 0`인 Machine. **단 ProcessingState 동작이 일반 머신과 미세하게 다름** — onEnter에서 inputBuffer move가 no-op이고 Started/Completed 대신 Spawned를 publish.

- `canStart()`: 기본 `inputBuffer.size() >= 0` (즉 항상 true) + Backpressure 모드면 `outputConveyor->canAccept()` 추가 → **Backpressure 모드에서는 가득찼으면 spawn skip**
- `process()`: 새 Product 생성 (`make_unique`), **Spawned publish** (sourceId=machine.id, productId=new product id, productType=new product type), 그다음 `tryPushOrDrop` 호출
  - push 성공 → 끝
  - push 실패 (Drop 모드만) → tryPushOrDrop이 Drop 이벤트 publish + product 폐기 → wip 회계 net 0 (Spawned +1, Drop -1)
- Started/Completed는 Spawner 발행 안 함 (의미 중복 회피)

### Packager 특이사항

Packager는 일반 Machine이지만 `process()`가 input을 소비하고 output을 생성하지 않음. 대신 **Packaged publish** (sourceId=machine.id, productId=consumed FinishedGuitar.id, productType=FinishedGuitar). Started/Completed도 정상 발행 (라이프사이클).

### 구체 클래스 11종

| 카테고리 (디렉토리) | 클래스 | requiredCount | process() 동작 |
|---|---|---|---|
| Spawner (추상, `spawner/`) | WoodSpawner | 0 | make_unique<RawWood>(newId), Spawned publish, tryPushOrDrop |
| | BridgeSpawner | 0 | make_unique<Bridge>(newId), Spawned publish, tryPushOrDrop |
| | PickupSpawner | 0 | make_unique<Pickup>(newId), Spawned publish, tryPushOrDrop |
| Cutter (추상, `cutter/`) | HeadCutter | 1 | RawWood 소비 → HeadPart(newId) 생성, tryPushOrDrop |
| | NeckCutter | 1 | RawWood → NeckPart(newId), tryPushOrDrop |
| | BodyCutter | 1 | RawWood → BodyPart(newId), tryPushOrDrop |
| Painter (`painter/`) | Painter | 1 | BodyPart 받아서 isPainted=true, **새 id로 BodyPart 재발급 후** tryPushOrDrop |
| MultiInputMachine 자식 (`multiple/collector/`) | ElecPartCollector | 2 (Bridge + Pickup) | ElecPartSet(newId) 생성, tryPushOrDrop |
| MultiInputMachine 자식 (`multiple/assembler/`) | BodyAssembler | 3 (Head + Neck + paintedBody) | AssembledBody(newId) 생성, tryPushOrDrop |
| | PartAssembler | 2 (AssembledBody + ElecPartSet) | FinishedGuitar(newId) 생성, tryPushOrDrop |
| Packager (`packager/`) | Packager | 1 | FinishedGuitar 소비, Packaged + Completed publish, 출력 없음 |

### 새 product ID 발급 규칙

모든 변환·생성 시점에 `ProductIdGen.next()`로 새 id 발급. 이유:
- Assembler처럼 n→1 변환에서는 input ids 중 무엇을 계승할지 결정 불가
- 일관성을 위해 1→1 변환(Cutter, Painter)도 새 id 발급
- input product는 currentProduct에 들어가며 `std::move`로 소비되어 사라짐

> Painter도 입력 BodyPart의 id를 계승하지 않고 새 id로 재발급 (isPainted=true인 BodyPart). product 인스턴스 자체를 `make_unique<BodyPart>(newId)` + `setPainted(true)`로 새로 만든다.

### 다중 입력 처리

BodyAssembler / PartAssembler / ElecPartCollector는 종류별 입력 필요. 셋이 동일한 typedBuffer 패턴을 쓰므로 `MultiInputMachine` 공통 추상 base로 추출 (machine/multiple/).

- `MultiInputMachine`은 `unordered_map<ProductType, vector<unique_ptr<Product>>>` typedBuffer 보유
- `canAcceptProduct(type)` override: `typedBuffer_[type].empty()`일 때만 true. type별 1슬롯 모델 — 한 type에 이미 하나 있어도 다른 type은 받음 (각 컨베이어 라인 독립)
- `acceptProduct` override: product type 보고 해당 typedBuffer push
- `canStart` override: 모든 필요 type에 1개 이상 있는지 검사 + base의 Backpressure 분기 유지
- `gatherInputs` override: typedBuffer에서 requiredTypes 순서대로 1개씩 currentProduct로 move
- `process` override: currentProduct 모두를 inputs vector로 이전 → 자식 `makeOutput(inputs, newId)` 호출 → tryPushOrDrop → 성공 시 Completed publish
- `serializeInputs` / `clearInputs` override (Phase 6 메멘토): typedBuffer 전체를 평탄화/clear. 복원 시 base의 `restoreFromSnap`이 `clearInputs() + acceptProduct(...) 다형 호출`로 다시 typedBuffer로 분류됨
- 자식 (`ElecPartCollector` / `BodyAssembler` / `PartAssembler`)은 생성자에서 requiredTypes 지정 + `makeOutput`만 override
- BodyAssembler는 정상 토폴로지에서 Painter 거친 painted BodyPart만 도착 (검증 없음)

> 동타입 다중 입력 (예: head-head-neck) 가드 / 잘못된 type 유입 가드 추가 안 함 — 현재 파이프라인 구조상 발생 불가능

### 이벤트 발행 정리

| 시점 | EventType | sourceId | productId / productType |
|---|---|---|---|
| Spawner.process() | **Spawned** | spawner.id | new product |
| Packager.process() 끝 | **Packaged** | packager.id | consumed FinishedGuitar |
| 일반 Machine ProcessingState.onEnter | Started | machine.id | 대표 input 1개 |
| 일반 Machine ProcessingState push 성공 | Completed | machine.id | output |
| 일반 Machine tryPushOrDrop의 push 실패 (Drop 모드) | **Drop** | machine.id | output (잃은 product) |
| Spawner의 tryPushOrDrop push 실패 (Drop 모드) | **Drop** | spawner.id | spawn된 product |
| BrokenState.onEnter | Fault | machine.id | nullopt |
| `repair()` 본체 | Resume | machine.id | nullopt |

> Statistics는 Spawned / Packaged / Drop / Fault만 구독. Started/Completed는 EventLog용 라이프사이클 로깅 전용.

## WIP 회계 규칙 (중요)

WIP = "시스템에 남아있는 원자재 단위 수". 각 ProductType은 정적 `sourceCount` 매핑을 가짐:

| ProductType | sourceCount |
|---|---|
| RawWood, HeadPart, NeckPart, BodyPart, Bridge, Pickup | 1 |
| ElecPartSet | 2 (Bridge + Pickup) |
| AssembledBody | 3 (Head + Neck + BodyPart) |
| FinishedGuitar | 5 (3 wood parts + Bridge + Pickup) |

Statistics가 룩업:
- `Spawned`  → wip += sourceCount(productType)  // 항상 1
- `Packaged` → wip -= sourceCount(productType), finished++  // 5
- `Drop`     → wip -= sourceCount(productType), lost += sourceCount(productType)
- `Fault`    → breakdowns++

회계 검증: 총 spawn sourceCount = 총 packaged sourceCount + 총 drop sourceCount. 항상 0으로 닫힘.

## 의존성

- Phase 1 (EventBroker, Product, Statistics, EventLog)
- Phase 2 (SimulationObject, Conveyor — 단순 슬롯 버퍼 + abort on overflow)
- RNG는 Factory에서 주입 (Phase 6)

## 테스트

phase별 분리:

| 파일 | 대상 | 케이스 수 |
|---|---|---|
| `tests/phase_3_machine.cpp` | State 패턴 + Machine 추상 (TestMachine 스텁) | 12 |
| `tests/phase_3_spawners.cpp` | Spawner 3종 | 9 |
| `tests/phase_3_cutters.cpp` | Cutter 3종 | 8 |
| `tests/phase_3_painter.cpp` | Painter | 3 |
| `tests/phase_3_collector.cpp` | ElecPartCollector | 8 |
| `tests/phase_3_assemblers.cpp` | BodyAssembler + PartAssembler | 7 |
| `tests/phase_3_packager.cpp` | Packager + Statistics 통합 | 6 |
| `tests/phase_3_integration.cpp` | 13머신 파이프라인 end-to-end | 2 |

핵심 시나리오:

- 상태 전이 (Idle → Processing → Idle/Broken, Broken → repair → 복귀)
- Health drop (bp=1.0 시 매 틱 -1, 0 도달 시 Broken)
- forceBreak → Idle/Processing에서 자연스러운 Broken 전이
- repair(tick): processingTick > 0 시 ProcessingState 복귀 + processingTick/currentProduct 보존 (resume), 아니면 Idle
- Backpressure 폴링: canStart 사전 차단, conveyor 빈 후 자동 재개 (이벤트 구독 검증 없음)
- Fault cascade handle(): `pendingDownstreamFaults_` 증감으로 canStart 영향
- tryPushOrDrop Drop 모드: 가득찬 conveyor 시 Drop publish + Completed 미발행
- Spawner Drop 모드: Spawned 먼저 + tryPushOrDrop으로 Drop 추가 (회계 net 0)
- Spawner Backpressure 모드: spawn skip (Spawned 미발행)
- MultiInputMachine: 종류별 충족 검사, gatherInputs 종류별 1개씩, output 생성 시 새 id
- Drop 시 productType이 정확해서 Statistics가 sourceCount 룩업으로 정확한 lost/wip 가감 (AssembledBody → -3, FinishedGuitar → -5)
- Packager 통합: 5번 Spawned → 1 Packaged FinishedGuitar → `wip=0, finished=1` 회계 닫힘
- Full 파이프라인: 13머신 + 12conveyor wire, 200 틱 → `finished > 0, lost=0, breakdowns=0`, 총 spawn = finished×5 + lost + wip

## 산출 브랜치

`back/feat/machines`

## 후속

- Backpressure 이벤트의 실제 publisher 결정 (phase 6 — 후보: Machine이 suspended 전이 시점에 1회 publish)
- Fault cascade 토픽 구독 등록 (phase 6 Factory.applyConfig)
- RNG / ProductIdGen 보유 주체 확정 (phase 6 Factory)
- 메멘토 직렬화 메서드(`restoreFromSnap` / `serializeInputs` 등)는 Phase 6 도입 — Phase 3에서는 시그니처만 알아두면 됨
- **ProcessingState.update의 `health<=0` 체크가 `breakdownProb > 0.0` 가드 안에 있음** — bp=0인 머신이 Processing 중 forceBreak 받으면 다음 cycle 종료까지 Broken 전이 안 됨. IdleState는 가드 밖 체크라 즉시 전이. 동작상 큰 문제 없으나 후속 보정 여지
