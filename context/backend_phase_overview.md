# Phase Overview

백엔드 작업 페이즈별 작업 리스트. 브랜치 prefix: `back/feat/...`
각 페이즈의 상세 구현 방안은 [`phases/phase_N_*.md`](phases/) 참조.

## 설계 결정 요약

본 phases는 다음 결정을 반영함:

- Factory는 객체 소유 + 외부 cmd 수신 + snap 생성으로 축소
- 클럭은 `SimulationRunner`로 분리
- Spawner는 Machine 계층에 흡수 (입력 없는 머신)
- Machine / Technician 상태는 State 패턴 + 싱글톤 + DI (`Types.h`에서 state enum 삭제)
- `FactorySnap`은 raw data만 보유 (state 필드 없음 — derive로 표현)
- `EventBroker` 큐 상태를 snap에 포함 (메멘토 정확도)
- `EventLog`와 `MementoStore` 분리. EventLog는 `subscribeAll`로 모든 EventType 구독 (만능 구독자)
- 시나리오는 JSON 파일 (`scenarios/*.json`), `ScenarioLoader`가 파싱 후 Factory의 create 호출 시퀀스로 변환
- 객체 생성은 Factory 패턴 — `factory.createMachine(type, id, params...)` 시그니처
- 시나리오 5종 (`Normal` / `Breakdowns` / `Bottleneck` / `Overflow` / `SmartFactory`)
- Overflow 정책은 Machine 멤버 (`outputOverflowMode_`). Drop 시나리오: push 실패 시 Machine이 직접 `Drop` publish + product 폐기. SmartFactory(Backpressure): canStart 폴링으로 사전 차단되어 push 자체가 발생 안 함. Conveyor는 단순 슬롯 버퍼 (가득찬 슬롯에 push 도달 시 std::abort).
- **Product 소유권**: `std::unique_ptr<Product>` 체인. Spawner `make_unique` → Conveyor 슬롯 / Machine 버퍼는 `unique_ptr<Product>` 보관 → `std::move`로 전달. Packager `process()` / Machine의 Drop 분기에서 unique_ptr 소멸로 자동 해제. Machine은 처리 중 `currentProduct_`로 잠시 보유만.
- **EventBroker 토픽 모델**: `subscribe(EventType, IEventHandler*)` 외에 `subscribe(EventType, sourceId, IEventHandler*)` 토픽 구독 추가. publish 시 (type, *) + (type, sourceId) 양쪽 디스패치. Fault cascade / per-machine 로그 등 source-aware 구독 가능. `subscribeAll`은 EventLog 전용 와일드카드 유지.
- **상류 정지/재개 (conveyor 포화)**: Backpressure 모드 한정. Machine의 `canStart()`가 `outputConveyor.canAccept()` 폴링으로 사전 차단. 이벤트 구독/페어링 없이 polling만으로 cascade가 물리적으로 자연 전파 (하류 막힘 → 상류 conveyor 차오름 → 상류 머신 polling으로 정지). Drop 모드는 상류 정지 없음.
- **Fault cascade (하류 머신 고장)**: SmartFactory 한정. Factory.applyConfig가 DAG 하류 transitive closure를 계산해 각 Machine을 모든 하류 머신의 `(Fault, x.id)` / `(Resume, x.id)` 토픽에 구독 등록. `pendingDownstreamFaults_` refcount로 복수 동시 고장 처리. `Resume` 발행은 `Machine.repair()` 내부에서 sourceId=machine.id로 (Technician/instantRepair 경로 공통). Drop 시나리오는 구독 등록 자체를 skip — Machine 클래스 분기 없음, Factory 와이어링에서만 분기.
- **수리 우선순위 동률**: Packager 의존성 거리 기준 정적 테이블, 동일 priority 내에서는 Fault 큐 진입 FIFO.
- **테스트**: 각 phase는 GoogleTest 단위 테스트 동반 (Phase 0에서 CMakeLists에 `FetchContent_Declare(googletest)` + `enable_testing()` 추가). 테스트는 `tests/` 하위에 phase별 파일.

## 머신 인스턴스 (전체 13개)

| 카테고리 | 인스턴스 |
|---|---|
| Spawner (5) | WoodSpawner ×3 (Head/Neck/Body 라인용), BridgeSpawner, PickupSpawner |
| Cutter (3) | HeadCutter, NeckCutter, BodyCutter |
| Painter (1) | Painter |
| Collector (1) | ElecPartCollector |
| Assembler (2) | BodyAssembler, PartAssembler |
| Packager (1) | Packager |

파이프라인은 `spawner → conveyor → machine` 한 줄 단위로 구성 (라운드 로빈 분배 폐지).

---

## Phase 0 — Scaffold 갱신

기존 머지된 공통 헤더를 새 설계에 맞춰 갱신. 상세: [phase_0_scaffold.md](phases/phase_0_scaffold.md)

- [x] `Types.h` — state enum 삭제, `MachineType` 11종 정의, `EventType` 추가, `OverflowMode` 추가, `ScenarioType` 5종으로 갱신
- [x] `Event.h` 신규 — `Event` 구조체 + `IEventHandler` 인터페이스
- [x] `FactorySnap.h` — `ConveyorSnap` 슬롯 모델, state 필드 제거, `pendingEvents` / `rngState` 추가, `SpawnerSnap` 삭제(Machine에 흡수)
- [x] `CMakeLists.txt` — nlohmann/json + GoogleTest `FetchContent` 추가
- [x] 빌드 통과 확인

## Phase 1 — 이벤트 인프라 & 데이터

브랜치: `back/feat/event-data`. 상세: [phase_1_event_data.md](phases/phase_1_event_data.md)

- [x] `EventBroker` — subscribe / publish / flush / clearQueue (단순 큐 루프, global→type→topic 디스패치)
- [x] `Product` 추상 + 9종 + `ProductIdGen` (atomic, next/peek/setCounter)
- [x] `Statistics` — finished / wip / breakdowns / lost + reset
- [x] `EventLog` — 텍스트 로그 max 200, FIFO drop, EventBroker subscribeAll 자동 등록
- [x] CMake `model_lib` static library 추출 (app/unit_tests 공통 링크, 후속 phase의 src/model/*.cpp 자동 흡수)

## Phase 2 — SimulationObject & Conveyor

브랜치: `back/feat/conveyor`. 상세: [phase_2_conveyor.md](phases/phase_2_conveyor.md)

- [x] `SimulationObject` 추상 — `update(tick)`, `getId()`, EventBroker 참조 보유
- [x] `Conveyor` — 슬롯 배열, 매 틱 shift, 출구 슬롯이 downstream `inputBuffer`로 push
- [x] Conveyor는 overflow 책임 없음 (후속 refactor PR에서 Machine으로 이전, push가 가득찬 슬롯에 도달 시 std::abort)

## Phase 3 — Machine + State 패턴

브랜치: `back/feat/machines`. 상세: [phase_3_machine.md](phases/phase_3_machine.md)

- [x] `Machine` 추상 — `inputBuffer`, `outputConveyor`, `health`, `processingTick`, `outputOverflowMode`, RNG, ProductIdGen, `IMachineState*` 싱글톤 참조
- [x] `IMachineState` + `MachineIdleState` / `MachineProcessingState` / `MachineBrokenState` 싱글톤 (Technician과의 이름 충돌 회피용 접두사)
- [x] `tryPushOrDrop` helper (Drop 모드 push 분기를 한 곳에 모음)
- [x] `Spawner` 추상 (Machine 하위, `requiredCount=0`) + WoodSpawner / BridgeSpawner / PickupSpawner
- [x] `Cutter` 추상 + 3종 (HeadCutter/NeckCutter/BodyCutter)
- [x] `Painter` (단독)
- [x] `MultiInputMachine` 공통 추상 (typedBuffer 보유) + `ElecPartCollector` + `BodyAssembler` + `PartAssembler`
- [x] `Packager` (sink, outputConveyor 없음)
- [x] 디렉토리 분리: `machine/{spawner,cutter,painter,multiple/{collector,assembler},packager}/`
- [x] 통합 시나리오 테스트 (13머신 파이프라인 end-to-end, Statistics 회계 검증)

## Phase 4 — Technician + State 패턴

브랜치: `back/feat/technician`. 상세: [phase_4_technician.md](phases/phase_4_technician.md)

- [x] `Technician` (SimulationObject 직접 상속) — `targetMachine_`, `repairProgress_`, `ITechnicianState*`
- [x] `ITechnicianState` + `TechnicianIdleState` / `TechnicianWorkingState` 싱글톤
- [x] 수리 완료 시 `machine.repair()` 호출 (Resume publish는 Machine.repair 내부에서)
- [x] `EngineerManager` → `TechnicianManager` 명칭 통일 (Phase 5/6/7 문서 일괄 변경)

## Phase 5 — Orchestrator (Manager / Memento / ScenarioLoader)

브랜치: `back/feat/orchestrator`. 상세: [phase_5_orchestrator.md](phases/phase_5_orchestrator.md)

- [x] `RepairDispatcher` — `Fault` 구독, 수리 대기 큐(packager 의존성 기준 우선순위), idle Technician 배정
- [x] `MementoStore` — `std::deque<FactorySnap>`, 매 틱 push, `rewind(tick)` 복원
- [x] `ScenarioLoader` — JSON 파싱 → create cmd 목록 반환
- [x] `scenarios/*.json` 4개 (Normal / Breakdowns / Bottleneck / Overflow)

## Phase 6 — Factory + SimulationRunner + Controller (backend 마무리)

브랜치: `back/feat/orchestrator` (Phase 5 + 6 통합) 또는 `back/feat/factory-controller`. 상세: [phase_6_factory.md](phases/phase_6_factory.md)

> **작업 범위**: backend (`model/` + `controller/`)만. View 패널과 `main.cpp`의 backend↔view 와이어링은 별도 frontend phase에서 진행.

- [x] `Factory` — `unique_ptr` 컨테이너, `createMachine/Conveyor/Technician` (타입별 switch 1곳), 외부 cmd 메서드 (`reset/setScenario/forceBreak/instantRepair/clearLog`), `IMachineLookup` 구현, `findMachine/findConveyor/findTechnician`
- [x] `Factory.applyConfig` — 와이어링 + priority map BFS 주입 + (SmartFactory 시) Backpressure / Fault cascade 토픽 구독
- [x] `Factory.tick()` — base 포인터 루프 (machines → conveyors → technicians → manager), `broker.flush()` 호출은 Runner가 담당
- [x] `Factory.snapshot()` / `restore(snap)` — RNG 직렬화 포함, id 매칭 + 필드 일괄 덮어쓰기 + state derive
- [x] `SimulationRunner` — `tickIntervalSec_` (0.6s/speed), accumulator, `tryAdvance(realDt)` 한 번에 `factory.tick + broker.flush + mementoStore.push`
- [x] RNG (`std::mt19937`) + `ProductIdGen` Factory 보유, reset 시 시드 재발급 + 카운터 0
- [x] `Controller::dispatch(MachineCmd)` — 11종 CmdAction → Factory / Runner / Loader / MementoStore 도메인 메서드 위임. 자체 상태 없음
- [x] `RepairDispatcher`에 `setLookup(IMachineLookup&)` setter 추가 → Factory와의 순환 의존 해결
- [x] 정적 priorityTable 제거 → Factory.applyConfig의 BFS 결과를 `setPriorityMap`으로 주입

## Phase 7 — refactor

- [x] `TechnicianManager` → `RepairDispatcher` 명칭 변경 (역할 명확화: 사람 관리가 아닌 수리 작업 디스패처 / SimObj 아님). 디렉토리 `src/model/repair_dispatcher/`, 테스트 / phase 문서 일괄 변경

---

## 백/프론트 동기화 포인트

| 시점 | 프론트 가능 작업 |
|---|---|
| Phase 0 머지 후 | mock `FactorySnap`으로 UI 레이아웃·위젯 검증 |
| Phase 6 머지 후 | 진짜 Factory + Controller 위에 View 패널 5종 + `main.cpp` 와이어링 작업. `runner.tryAdvance(io.DeltaTime)` 한 줄로 시뮬레이션 진행 |
