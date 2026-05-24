# Phase 5 — Orchestrator (Manager / Memento / ScenarioLoader)

## 목표

Factory에서 분리된 조정자 3종 + 시나리오 정의. 셋 다 서로 독립이라 병렬 가능.

## 적용 패턴

- **Observer**: `TechnicianManager`가 Fault 구독자
- **Memento**: `MementoStore` (Caretaker), `FactorySnap` (Memento), `Factory` (Originator — Phase 6)
- **Configuration Loader**: `ScenarioLoader`가 JSON → 시나리오 설정 객체로 변환

## 구성요소

### `TechnicianManager` (IEventHandler 구현)

```
technicians_: vector<Technician*>             // 참조만, 소유는 Factory
repairQueue_: vector<QueueEntry>              // 정렬 + FIFO 동률 처리

struct QueueEntry { Machine* machine; int priority; int faultTick; int seq; }

handle(Event):
  - type == Fault: queue.push({machine_from_sourceId, prio_table[machine.type], event.tick, next_seq++})
update(tick):
  - 큐 정렬: priority 오름차순 → faultTick 오름차순 → seq 오름차순 (FIFO)
  - 각 idle Technician에 대해 큐 앞에서 pop → tech.assign(machine)
  - 큐 상단 머신이 이미 idle(수리 완료)이면 pop (instantRepair 우회 대응)
```

구독 등록: 생성 시 `broker.subscribe(EventType::Fault, this)`.

**우선순위 정책**:
- Packager 의존성 그래프 역방향 거리. 같은 MachineType의 여러 인스턴스가 다른 거리를 가지면 **최단 거리**로 단일화 (정적 테이블 단순화 위함). priority 값이 낮을수록 먼저 수리
- 정적 매핑 테이블 (`unordered_map<MachineType, int>`):

  | MachineType | priority |
  |---|---|
  | Packager | 0 |
  | PartAssembler | 1 |
  | BodyAssembler | 2 |
  | ElecPartCollector | 2 |
  | HeadCutter | 3 |
  | NeckCutter | 3 |
  | Painter | 3 |
  | BridgeSpawner | 3 |
  | PickupSpawner | 3 |
  | BodyCutter | 4 |
  | WoodSpawner | 4 |

- **동률 처리**: 우선순위 동일 시 Fault 발생 틱 → 큐 진입 sequence (FIFO). 결정론적, 메멘토 호환

**Machine 조회**: sourceId(string)에서 Machine*로 매핑하려면 Factory에 lookup 메서드 필요 (`Factory::findMachine(id)`). TechnicianManager는 Factory 참조도 보유.

### `MementoStore`

```
history_: deque<FactorySnap>
firstTick_: int       // history_[0]에 해당하는 tick (Reset 후 0이 아닐 수 있음)

push(FactorySnap)
rewind(int targetTick) → FactorySnap    // 이후 snap 폐기, 해당 snap 반환
clear()
```

- SimulationRunner가 매 틱 push 호출 (Phase 6)
- Rewind cmd: Controller가 호출 → 반환 snap을 `factory.restore(snap)`로 적용

### `ScenarioLoader`

```
load(ScenarioType) → ScenarioConfig
```

`ScenarioConfig` 구조:
```
struct MachineDef     { MachineType type; string id; int processingTime; double breakdownProb; int requiredCount; string outputConveyorId; }
struct ConveyorDef    { string id; int length; string downstreamId; OverflowMode overflowMode; }
struct TechnicianDef  { string id; int repairTime; }
struct ScenarioConfig {
  ScenarioType type;
  vector<MachineDef> machines;
  vector<ConveyorDef> conveyors;
  vector<TechnicianDef> technicians;
}
```

Controller가 `setScenario` cmd 처리 시 ScenarioLoader.load → Factory.applyConfig.

### 시나리오 JSON 풀 정의

`scenarios/*.json` 5개 (Normal / Breakdowns / Bottleneck / Overflow / SmartFactory). 모두 동일 13개 머신 + 12개 컨베이어 + 2명 Technician 구조. 차이는 파라미터만:

| 시나리오 | breakdownProb | Painter pt | WoodSpawner pt | overflowMode |
|---|---|---|---|---|
| Normal | 0 | 6 | 2 | drop |
| Breakdowns | 0.02 | 6 | 2 | drop |
| Bottleneck | 0.02 | 12 | 2 | drop |
| Overflow | 0.02 | 6 | 1 | drop |
| SmartFactory | 0.02 | 6 | 1 | backpressure |

```json
{
  "name": "Bottleneck",
  "machines": [
    { "type": "WoodSpawner",       "id": "SPN_WOOD_HEAD",  "processingTime": 2,  "breakdownProb": 0.02, "requiredCount": 0, "outputConveyorId": "CONV_WOOD_HEAD" },
    { "type": "WoodSpawner",       "id": "SPN_WOOD_NECK",  "processingTime": 2,  "breakdownProb": 0.02, "requiredCount": 0, "outputConveyorId": "CONV_WOOD_NECK" },
    { "type": "WoodSpawner",       "id": "SPN_WOOD_BODY",  "processingTime": 2,  "breakdownProb": 0.02, "requiredCount": 0, "outputConveyorId": "CONV_WOOD_BODY" },
    { "type": "BridgeSpawner",     "id": "SPN_BRIDGE",     "processingTime": 3,  "breakdownProb": 0.02, "requiredCount": 0, "outputConveyorId": "CONV_BRIDGE" },
    { "type": "PickupSpawner",     "id": "SPN_PICKUP",     "processingTime": 3,  "breakdownProb": 0.02, "requiredCount": 0, "outputConveyorId": "CONV_PICKUP" },
    { "type": "HeadCutter",        "id": "MCH_HEAD_CUT",   "processingTime": 4,  "breakdownProb": 0.02, "requiredCount": 1, "outputConveyorId": "CONV_HEAD" },
    { "type": "NeckCutter",        "id": "MCH_NECK_CUT",   "processingTime": 4,  "breakdownProb": 0.02, "requiredCount": 1, "outputConveyorId": "CONV_NECK" },
    { "type": "BodyCutter",        "id": "MCH_BODY_CUT",   "processingTime": 4,  "breakdownProb": 0.02, "requiredCount": 1, "outputConveyorId": "CONV_BODY_RAW" },
    { "type": "Painter",           "id": "MCH_PAINT",      "processingTime": 12, "breakdownProb": 0.02, "requiredCount": 1, "outputConveyorId": "CONV_BODY_PAINTED" },
    { "type": "ElecPartCollector", "id": "MCH_ELEC",       "processingTime": 4,  "breakdownProb": 0.02, "requiredCount": 2, "outputConveyorId": "CONV_ELEC" },
    { "type": "BodyAssembler",     "id": "MCH_BODY_ASM",   "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 3, "outputConveyorId": "CONV_ASMBODY" },
    { "type": "PartAssembler",     "id": "MCH_PART_ASM",   "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 2, "outputConveyorId": "CONV_GUITAR" },
    { "type": "Packager",          "id": "MCH_PACK",       "processingTime": 3,  "breakdownProb": 0.02, "requiredCount": 1, "outputConveyorId": "" }
  ],
  "conveyors": [
    { "id": "CONV_WOOD_HEAD",     "length": 5, "downstreamId": "MCH_HEAD_CUT",  "overflowMode": "drop" },
    { "id": "CONV_WOOD_NECK",     "length": 5, "downstreamId": "MCH_NECK_CUT",  "overflowMode": "drop" },
    { "id": "CONV_WOOD_BODY",     "length": 5, "downstreamId": "MCH_BODY_CUT",  "overflowMode": "drop" },
    { "id": "CONV_BRIDGE",        "length": 5, "downstreamId": "MCH_ELEC",      "overflowMode": "drop" },
    { "id": "CONV_PICKUP",        "length": 5, "downstreamId": "MCH_ELEC",      "overflowMode": "drop" },
    { "id": "CONV_HEAD",          "length": 5, "downstreamId": "MCH_BODY_ASM",  "overflowMode": "drop" },
    { "id": "CONV_NECK",          "length": 5, "downstreamId": "MCH_BODY_ASM",  "overflowMode": "drop" },
    { "id": "CONV_BODY_RAW",      "length": 5, "downstreamId": "MCH_PAINT",     "overflowMode": "drop" },
    { "id": "CONV_BODY_PAINTED",  "length": 5, "downstreamId": "MCH_BODY_ASM",  "overflowMode": "drop" },
    { "id": "CONV_ELEC",          "length": 5, "downstreamId": "MCH_PART_ASM",  "overflowMode": "drop" },
    { "id": "CONV_ASMBODY",       "length": 5, "downstreamId": "MCH_PART_ASM",  "overflowMode": "drop" },
    { "id": "CONV_GUITAR",        "length": 5, "downstreamId": "MCH_PACK",      "overflowMode": "drop" }
  ],
  "technicians": [
    { "id": "TECH_1", "repairTime": 3 },
    { "id": "TECH_2", "repairTime": 3 }
  ]
}
```

> 5종 JSON은 동일 토폴로지, 위 표의 파라미터만 차이. SmartFactory만 모든 conveyor의 overflowMode를 `backpressure`로 통일.

### 틱 내 이벤트 처리

- publish는 큐 적재만, flush는 틱 종료 시. 즉 같은 틱에 Fault 발행 → TechnicianManager 처리는 **다음 틱**의 update에서 (1틱 지연 감수)
- 명세상 명확하므로 TechnicianManager.update는 Factory.tick() 순서의 마지막에 배치되어 broker.flush() 결과를 한 틱 늦게 받음 ([phase_6_factory.md] 참조)

### Event payload 타입 확정 (Phase 3 단계에서 선반영)

`void*` 폐기. `Event`에 `std::optional<int> productId`, `std::optional<ProductType> productType` 추가. 발행 시점별 채움 규칙:

| Event | productId | productType | publisher | 비고 |
|---|---|---|---|---|
| **Spawned** | new product | new product | Spawner | WIP 회계: wip += sourceCount (=1) |
| **Packaged** | consumed FinishedGuitar | FinishedGuitar | Packager | WIP 회계: wip -= 5, finished++ |
| **Drop** | 손실된 product | 손실된 product | Machine (push 실패 시) | WIP 회계: wip/lost -= sourceCount |
| Started | 대표 input 1개 | 대표 input 1개 | 일반 Machine ProcessingState.onEnter | EventLog 라이프사이클 (WIP 영향 없음) |
| Completed | output | output | 일반 Machine ProcessingState push 성공 시 | EventLog 라이프사이클 (WIP 영향 없음) |
| Fault | nullopt | nullopt | Machine BrokenState.onEnter | sourceId=machine.id |
| Resume | nullopt | nullopt | Machine.repair() 본체 | sourceId=machine.id (Fault cascade 트리거) |
| Backpressure | nullopt | nullopt | (phase 6 결정) | EventLog 가시성 보조 |

이유: `void*`는 (1) 타입 안전성 0, (2) 수명 관리 불가 (publish 후 flush 사이 dangling), (3) 메멘토 직렬화 불가. 위 표의 정보는 (a) Statistics가 sourceCount 룩업으로 WIP/lost 회계, (b) EventLog가 사람이 읽을 수 있는 메시지 조합에 사용. variant 도입은 불필요 — optional 두 개로 충분.

## 의존성

- TechnicianManager: Phase 3 (Machine), Phase 4 (Technician), Phase 1 (EventBroker), Phase 6 (Factory.findMachine)
- MementoStore: Phase 0 (FactorySnap)
- ScenarioLoader: nlohmann/json (Phase 0)

## 테스트

`tests/phase_5_manager.cpp`:
- Fault 이벤트 수신 → repairQueue 진입
- 우선순위 정렬: Packager + HeadCutter 동시 fault → Packager 먼저
- FIFO 동률: HeadCutter / ElecPartCollector (priority 동일 4) 동시 fault → faultTick 같으면 seq 작은 쪽 먼저
- idle Technician에 assign 호출 검증
- instantRepair 후 큐 잔존 처리: queue 상단 machine이 idle이면 자동 pop

`tests/phase_5_memento.cpp`:
- push N개 → history size N
- rewind(K) 호출 → K 이후 snap 폐기, history[K] 반환
- clear() → empty

`tests/phase_5_loader.cpp`:
- 5종 JSON 파싱 → MachineDef/ConveyorDef/TechnicianDef 개수 검증
- overflowMode 문자열 → enum 매핑
- 잘못된 type 문자열 → 예외 / 오류 처리

## 산출 브랜치

`back/feat/orchestrator` (또는 셋 분할 — `back/feat/engineer-manager`, `back/feat/memento`, `back/feat/scenario-loader`)

## 후속

- 메멘토 RNG 상태 직렬화 → Phase 6에서 RNG 정책과 함께 확정
