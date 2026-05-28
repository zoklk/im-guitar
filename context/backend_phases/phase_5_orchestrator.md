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
technicians_:  vector<Technician*>             // 참조만, 소유는 Factory
repairQueue_:  vector<QueueEntry>              // 정렬 + FIFO 동률 처리
priorityMap_:  unordered_map<string,int>       // Factory.applyConfig가 setPriorityMap으로 by-value 주입
lookup_:       IMachineLookup*                 // nullable. 생성자 인자 또는 setLookup setter로 주입

struct QueueEntry { Machine* machine; int priority; int faultTick; int seq; }

setPriorityMap(map):  priorityMap_ = std::move(map)   // Factory.applyConfig에서 1회 호출 (by-value 보유)
setLookup(lookup):    lookup_      = &lookup          // 순환 의존성 해결용 setter (Phase 7 통합 참조)
priorityOf(machineId) const → int                     // priorityMap_의 entry 반환, 없으면 99 fallback

handle(Event):
  - type == Fault && lookup_ 유효: queue.push({lookup_->findMachine(sourceId), priorityOf(machine.id), event.tick, next_seq++})
update(tick):
  - 큐 정렬: priority 오름차순 → faultTick 오름차순 → seq 오름차순 (FIFO)
  - 각 idle Technician에 대해 큐 앞에서 pop → tech.assign(machine)
  - 큐 상단 머신이 이미 idle(수리 완료)이면 pop (instantRepair 우회 대응)
```

구독 등록: 생성 시 `broker.subscribe(EventType::Fault, this)`.

**우선순위 정책**:
- Sink(Packager) 의존성 그래프 역방향 거리. priority 값이 낮을수록 먼저 수리
- **계산 위치**: Phase 6 Factory.applyConfig가 토폴로지에서 역방향 BFS로 `unordered_map<string,int>` (machineId → priority)를 산출하여 `technicianManager.setPriorityMap(map)`로 주입. TechnicianManager는 정적 타입 테이블을 보유하지 않음
- **Sink 정의**: `outputConveyorId == ""` 인 머신 (현 5종 시나리오는 모두 Packager 단일 sink). 다중 sink면 최단 거리 채택, 도달 불가 머신은 폴백 99
- **인스턴스 단위 priority**: 같은 MachineType이라도 그래프 위치별로 다른 priority를 가질 수 있음 (예: 백업 Packager 추가 시 자연 분리). 과거 "MachineType별 최단 거리 단일화" 단순화는 폐기
- **동률 처리**: 우선순위 동일 시 Fault 발생 틱 → 큐 진입 sequence (FIFO). 결정론적, 메멘토 호환

표준 13-머신 토폴로지에서의 산출 결과 (참고 예시, source of truth 아님):

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

**Machine 조회**: sourceId(string)에서 Machine*로 매핑하려면 `IMachineLookup` 인터페이스 (`findMachine(id) → Machine*`)가 필요. Phase 6 Factory가 이를 구현. TechnicianManager는 `lookup_: IMachineLookup*`를 보유 (nullable) — Phase 7 main 와이어링에서 Factory 생성 전 임시 `NullLookup` placeholder로 생성자 통과 후, Factory 생성 직후 `setLookup(factory)` 호출로 진짜 lookup 주입. 순환 의존 (TechnicianManager ↔ Factory) 해결 패턴.

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
- 사이즈 캡 없음 (무제한 누적). 메모리 정책은 Phase 6/7에서 sliding window 검토.

### 메멘토 정확도: TechnicianManager 큐 직렬화

`FactorySnap.pendingRepairs: vector<RepairOrderSnap>` 추가 (Phase 0 FactorySnap 확장). 한 entry 구조:

```
struct RepairOrderSnap { string machineId; int priority; int faultTick; int seq; }
```

rewind 시 큐 잔량을 100% 복원하기 위함. TechnicianManager 측에 `clearQueue()` / `restoreQueue(entries, nextSeq)` 노출. Factory.snapshot()이 `mgr.getQueue()` → `RepairOrderSnap` 변환, `factory.restore()`가 역변환 후 `mgr.restoreQueue()` 호출 (Phase 6).

### `ScenarioLoader`

```
load(ScenarioType) → ScenarioConfig
```

`ScenarioConfig` 구조:
```
struct MachineDef     { MachineType type; string id; int processingTime; double breakdownProb; int requiredCount; int maxHealth = 10; string outputConveyorId; }
struct ConveyorDef    { string id; int length; string downstreamId; OverflowMode overflowMode; }
struct TechnicianDef  { string id; string name; int repairTime; }
struct ScenarioConfig {
  ScenarioType type;
  string name;
  vector<MachineDef> machines;
  vector<ConveyorDef> conveyors;
  vector<TechnicianDef> technicians;
}
```

- `maxHealth`: JSON 미지정 시 default 10. 시나리오마다 머신 내구도 차별화 여지 확보 (현재 5종 시나리오는 모두 10).
- `TechnicianDef.name`: UI 표시용. JSON 미지정 시 id로 fallback. 시나리오는 `jincheol` / `jaeyong` 사용.

Controller가 `setScenario` cmd 처리 시 ScenarioLoader.load → Factory.applyConfig.

**에러 처리**: 잘못된 enum 문자열 / 필수 키 누락 / 파일 없음 / JSON 파싱 실패는 모두 `std::runtime_error`. 메시지에 파일 경로 + 원인 wrap (디버깅 효율).

### 시나리오 JSON 풀 정의

`scenarios/*.json` 5개 (Normal / Breakdowns / Bottleneck / Overflow / SmartFactory). 모두 동일 13개 머신 + 12개 컨베이어 + 2명 Technician 구조. 차이는 파라미터만:

| 시나리오 | breakdownProb | 머신 pt 정책 | overflowMode |
|---|---|---|---|
| Normal | 0 | 13대 전부 6 | drop |
| Breakdowns | 0.02 | 13대 전부 6 | drop |
| Bottleneck | 0.02 | 13대 전부 6, Painter만 12 | drop |
| Overflow | 0.02 | Spawner 5종만 2, 나머지 8종 6 | drop |
| SmartFactory | 0.02 | (변경 없음, 기존 값 유지) | backpressure |

> **pt 통일 결정 이유**: spawner의 pt를 낮추면 처음에는 빠르게 채워지지만 곧 conveyor가 포화 → Normal에서도 drop이 발생함. Normal은 drop 없이 흐름 검증 시나리오이므로 pt를 통일. Overflow는 의도적 drop이 목적이라 spawner만 예외적으로 pt 작게.

```json
{
  "name": "Bottleneck",
  "machines": [
    { "type": "WoodSpawner",       "id": "SPN_WOOD_HEAD",  "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 0, "maxHealth": 10, "outputConveyorId": "CONV_WOOD_HEAD" },
    { "type": "WoodSpawner",       "id": "SPN_WOOD_NECK",  "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 0, "maxHealth": 10, "outputConveyorId": "CONV_WOOD_NECK" },
    { "type": "WoodSpawner",       "id": "SPN_WOOD_BODY",  "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 0, "maxHealth": 10, "outputConveyorId": "CONV_WOOD_BODY" },
    { "type": "BridgeSpawner",     "id": "SPN_BRIDGE",     "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 0, "maxHealth": 10, "outputConveyorId": "CONV_BRIDGE" },
    { "type": "PickupSpawner",     "id": "SPN_PICKUP",     "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 0, "maxHealth": 10, "outputConveyorId": "CONV_PICKUP" },
    { "type": "HeadCutter",        "id": "MCH_HEAD_CUT",   "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 1, "maxHealth": 10, "outputConveyorId": "CONV_HEAD" },
    { "type": "NeckCutter",        "id": "MCH_NECK_CUT",   "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 1, "maxHealth": 10, "outputConveyorId": "CONV_NECK" },
    { "type": "BodyCutter",        "id": "MCH_BODY_CUT",   "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 1, "maxHealth": 10, "outputConveyorId": "CONV_BODY_RAW" },
    { "type": "Painter",           "id": "MCH_PAINT",      "processingTime": 12, "breakdownProb": 0.02, "requiredCount": 1, "maxHealth": 10, "outputConveyorId": "CONV_BODY_PAINTED" },
    { "type": "ElecPartCollector", "id": "MCH_ELEC",       "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 2, "maxHealth": 10, "outputConveyorId": "CONV_ELEC" },
    { "type": "BodyAssembler",     "id": "MCH_BODY_ASM",   "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 3, "maxHealth": 10, "outputConveyorId": "CONV_ASMBODY" },
    { "type": "PartAssembler",     "id": "MCH_PART_ASM",   "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 2, "maxHealth": 10, "outputConveyorId": "CONV_GUITAR" },
    { "type": "Packager",          "id": "MCH_PACK",       "processingTime": 6,  "breakdownProb": 0.02, "requiredCount": 1, "maxHealth": 10, "outputConveyorId": "" }
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
    { "id": "TECH_1", "name": "jincheol", "repairTime": 3 },
    { "id": "TECH_2", "name": "jaeyong",  "repairTime": 3 }
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

- TechnicianManager: Phase 3 (Machine), Phase 4 (Technician), Phase 1 (EventBroker), `IMachineLookup` 인터페이스 (Phase 6 Factory가 구현)
- MementoStore: Phase 0 (FactorySnap)
- ScenarioLoader: nlohmann/json (Phase 0)

> Factory와의 의존성 역전: TechnicianManager는 Factory 헤더를 include하지 않음. `src/model/machine/IMachineLookup.h`에 `findMachine(id) → Machine*` 인터페이스만 정의 → Phase 6 Factory가 구현.

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
