# Phase 2 — SimulationObject & Conveyor

## 목표

능동 객체 공통 base + 컨베이어 1종. 시뮬레이션 루프의 다형성 디스패치 토대.

## 적용 패턴

- **Template Method**: `SimulationObject::update(tick)`는 자식에서 구현, Factory의 base 포인터 루프에서 통일 호출
- **Conveyor overflow 분기**: enum + 메서드 분리. 클래스 분리한 Strategy는 과함 — 분기 두 가지, 생성 시 고정
- **EventBroker 단일화**: Conveyor는 EventLog / Statistics를 직접 참조하지 않음. Drop / Backpressure 둘 다 `broker.publish`만 호출. 로그 기록은 EventLog가 `subscribeAll`로, 카운터 갱신은 Statistics가 `Drop`/`Started`/`Completed`/`Fault` 구독으로 자율 처리 → 발행자/수신자 결합도 0
- **Interface 분리 (결합도/TDD)**: `IConveyor` / `IMachine` 추상 인터페이스 도입. Conveyor는 downstream을 `IMachine*`로 보유하고, 후속 Phase 3 Machine은 outputConveyor를 `IConveyor*`로 보유. 양쪽 다 mock 가능 → 단위 테스트에서 상대 객체 없이 단독 검증
- **와이어링 분리**: Conveyor 생성자는 자기 의존만 받음. downstream은 후-와이어링 setter로 주입. 이유는 (1) Conveyor↔Machine 순환 의존을 생성자 단계에서 해소, (2) 테스트 fixture가 mock IMachine을 자유롭게 끼워넣기 위함

## 구성요소

### `SimulationObject` (추상)

```
virtual void update(int tick) = 0
virtual const std::string& getId() const = 0
EventBroker& broker_                  // 생성자 주입
```

자식 능동 객체가 공통으로 필요한 의존은 EventBroker 하나. EventLog / Statistics 같은 부수 객체는 broker 구독자이므로 발행자(자식)가 직접 참조하지 않음.

### `IConveyor` (인터페이스)

```
virtual bool canAccept() const = 0
virtual void push(std::unique_ptr<Product>, int tick) = 0
virtual OverflowMode getOverflowMode() const = 0
virtual const std::string& getId() const = 0
virtual ~IConveyor() = default
```

Phase 3 Machine이 `outputConveyor_: IConveyor*`로 보유. 테스트에서 MockConveyor로 대체 가능.

### `IMachine` (인터페이스, forward-decl 대용)

```
virtual void acceptProduct(std::unique_ptr<Product>) = 0
virtual const std::string& getId() const = 0
virtual ~IMachine() = default
```

Conveyor가 `downstream_: IMachine*`로 보유. Phase 2에선 Machine 구현체가 없으므로 테스트에서 MockMachine으로만 사용. Phase 3에서 실제 Machine 추상이 IMachine을 상속.

### `Conveyor` (SimulationObject + IConveyor)

데이터:
```
slots_: vector<unique_ptr<Product>>   // size = length_, nullptr = 빈 슬롯
length_: int
overflowMode_: OverflowMode
id_: string
downstream_: IMachine*                // setter로 주입 (생성자 X)
```

EventLog / Statistics 멤버 없음. SimulationObject가 들고 있는 `broker_`만으로 모든 부수 효과 전달.

슬롯 인덱스 규약: `slots_[0]` = 입구, `slots_[length-1]` = 출구. 제품은 입구→출구 방향 진행.

생성자:
```
Conveyor(id, length, mode, broker)
  // downstream은 받지 않음 — setDownstream으로 후-주입
```

동작:
```
setDownstream(IMachine* m)            // Factory.applyConfig / 테스트 fixture가 호출
canAccept() const → bool              // slots_[0] 비어있는지. 상류 Machine의 폴링용
push(unique_ptr<Product>, int tick)   // slots_[0]에 적재. 막혀있으면 onOverflow(p, tick)
update(tick):
  1. 출구 방출: slots_[length-1] != null && downstream_ != null
     → downstream_->acceptProduct(move(slots_[length-1]))
  2. 응축 시프트: i = length-1 → 1 역방향 순회
     - if slots_[i] == null && slots_[i-1] != null:
         slots_[i] = move(slots_[i-1])     // 빈 칸이 출구쪽으로 못 새도록 끌어당김
onOverflow(unique_ptr<Product> p, int tick):
  - mode == Drop          → publishDrop(tick)  (p는 스코프 종료로 자동 소멸)
  - mode == Backpressure  → publishBackpressure(tick) + p 소멸
publishDrop(int tick):
  - broker_.publish({Drop, getId(), tick, nullptr})
  - lost/wip 갱신, 로그 기록은 Statistics / EventLog가 알아서 처리
publishBackpressure(int tick):
  - broker_.publish({Backpressure, getId(), tick, nullptr})
```

판단:
- **push(tick) 시그니처**: tick을 인자로 받는 이유는 발행되는 Event에 정확한 tick을 실어야 하기 때문. Conveyor에 currentTick_ 멤버를 두면 update() 시점의 tick을 기억해야 하는데, 호출 순서 의존 (push가 update 사이에 끼어들면 이전 tick 사용) — 호출자가 명시 전달하는 게 안전
- **Drop도 broker 경유**: 초기 설계에선 EventLog.appendDirect로 우회 기록했으나, 카프카-스타일 결합도 0 원칙에 맞춰 broker.publish로 일원화. EventLog는 subscribeAll, Statistics는 Drop 구독으로 자동 반영. 발행자(Conveyor)는 수신자 존재를 모름
- Backpressure 모드: 일반 시나리오엔 미사용. SmartFactory에서 상류 Machine이 `(Backpressure, conveyor.id)` 토픽 구독
- mode는 Conveyor 생성 시 고정 주입 — 런타임 분기 없음
- **시프트 규칙 (역방향 순회)**: `i = length-1 → 1`로 순회하며 "내가 비면 왼쪽 한 칸 끌어당김". 정방향 순회면 `11110 → 11101`로 가운데 구멍이 남지만, 역방향이면 `11110 → 11101 → 11011 → 10111 → 01111`로 출구 쪽으로 자연스럽게 응축. 병목 시 입구 쪽에 pile-up (의도된 시각화)

### inputBuffer / 다중 입력 정책

`IMachine.acceptProduct(unique_ptr<Product>)` 가상 메서드 호출로 일원화. Phase 3에서 Machine 추상이 기본 구현(단일 `inputBuffer_`에 push)을 제공하고, Assembler/Collector는 종류별 typedBuffer에 분류 (override). Conveyor는 ProductType을 모름.

머신 처리 속도가 컨베이어 도착 속도보다 느릴 때 inputBuffer 누적 (의도된 동작). 단, SmartFactory에서는 상류가 `outputConveyor.canAccept()` 폴링하여 자체 정지 → inputBuffer 무한 누적은 일반 시나리오 한정.

### Factory 와이어링 책임 (Phase 6 예고)

Factory는 빌딩 블록 + 일괄 조립 양쪽 API를 노출:

```cpp
// 빌딩 블록 (UI 커스텀 / 테스트용으로도 호환)
Machine*  createMachine(MachineType, id, params)
Conveyor* createConveyor(id, length, OverflowMode)
void      wire(machineId, conveyorId)   // machine.outputConveyor 설정
void      wire(conveyorId, machineId)   // conveyor.downstream 설정 (setDownstream 호출)

// 프리셋 일괄 적용 (시나리오 드롭다운 경로)
void      applyConfig(ScenarioConfig)
  // 내부: createMachine 루프 → createConveyor 루프 → wire 루프 → 토픽 구독
```

> Phase 2 시점엔 Factory가 아직 없으므로 테스트가 직접 `new Conveyor(...) + setDownstream(&mock)`으로 와이어링. Factory는 Phase 6에서 구현.

## 의존성

- 상위: EventBroker, EventLog, Statistics (Phase 1)
- 하위: Phase 3 Machine이 IMachine을 상속. Conveyor는 Machine 헤더를 include하지 않음 (IMachine만 의존)

## 테스트

`tests/phase_2_conveyor.cpp` (테스트 내부에 `MockMachine: public IMachine` 정의해 downstream 주입):

- 빈 컨베이어에 push → 입구 슬롯 점유. 시프트 동작 — N틱 후 출구 도달
- 출구 슬롯 product + downstream 정상 → MockMachine.acceptProduct 호출 검증 (받은 product의 id 비교), 슬롯 reset
- 가득찬 상태에서 push (Drop 모드) → Drop 이벤트 publish (EventRecorder로 토픽 수신 검증), broker 구독자 Statistics가 lost++/wip--, EventLog가 "[Drop] C1" 자동 기록
- 가득찬 상태에서 push (Backpressure 모드) → Backpressure 이벤트 publish (Event.tick 검증), Statistics.lost 미증가 (Drop 이벤트 안 발행되므로)
- canAccept(): 빈 입구 → true, 점유 → false
- 응축 시프트: 초기 `[1,1,1,1,0]`, downstream=null → 1틱 후 `[0,1,1,1,1]`. 가운데 구멍 안 생김
- 정체 시 pile-up: 출구 막혀있고 입구만 계속 push → 점차 출구 쪽으로 슬롯 채워짐 (역방향 순회로 응축)
- setDownstream 와이어링: 생성 직후 downstream=null 상태 → 출구 슬롯 점유해도 방출 없음 (slots_ 유지). setDownstream(&mock) 후 update → 방출됨

## 산출 브랜치

`back/feat/conveyor`

## 후속

- Phase 3에서 `Machine` 추상이 `IMachine` 상속, `outputConveyor_: IConveyor*` 보유로 구현
- Factory 와이어링 API (`createMachine` / `createConveyor` / `wire` / `applyConfig`)는 Phase 6에서 구현
