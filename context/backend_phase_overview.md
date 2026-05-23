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
- Conveyor 오버플로우는 시나리오별 정책 — 일반 4종: `Drop` (드롭 + lost 카운트), SmartFactory: `Backpressure` (직전 Machine에 publish)
- **Product 소유권**: `std::unique_ptr<Product>` 체인. Spawner `make_unique` → Conveyor 슬롯 / Machine 버퍼는 `unique_ptr<Product>` 보관 → `std::move`로 전달. Packager `process()` / Conveyor drop에서 unique_ptr 소멸로 자동 해제. Machine은 처리 중 `currentProduct_`로 잠시 보유만.
- **EventBroker 토픽 모델**: `subscribe(EventType, IEventHandler*)` 외에 `subscribe(EventType, sourceId, IEventHandler*)` 토픽 구독 추가. publish 시 (type, *) + (type, sourceId) 양쪽 디스패치. Backpressure 캐스케이드 / per-machine 로그 등 source-aware 구독 가능. `subscribeAll`은 EventLog 전용 와일드카드 유지.
- **상류 정지/재개 (conveyor 포화)**: Backpressure 모드 한정. Machine은 자신의 `outputConveyor.id`에 대한 `(Backpressure, conveyor.id)` 토픽을 구독, 수신 시 일시정지 상태로 진입. 실제 재개는 매 틱 `outputConveyor.canAccept()` 폴링으로 자연 복귀 (이벤트 페어링 불필요). Drop 모드는 상류 정지 없음.
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

- [ ] `SimulationObject` 추상 — `update(tick)`, `getId()`, EventBroker 참조 보유
- [ ] `Conveyor` — 슬롯 배열, 매 틱 shift, 출구 슬롯이 downstream `inputBuffer`로 push
- [ ] `Conveyor` 오버플로우 분기 — `OverflowMode`(Drop/Backpressure) 멤버 + `dropAndLog(Product*)` / `publishBackpressure()` 메서드 분리. 시나리오 JSON에서 conveyor당 모드 지정

## Phase 3 — Machine + State 패턴

브랜치: `back/feat/machines`. 상세: [phase_3_machine.md](phases/phase_3_machine.md)

- [ ] `Machine` 추상 — `inputBuffer`, `outputConveyor`, `health`, `processingTick`, `IMachineState*` (싱글톤 참조)
- [ ] `IMachineState` + `IdleState` / `ProcessingState` / `BrokenState` 싱글톤
- [ ] `Spawner` 추상 (Machine 하위, `requiredCount=0`) + WoodSpawner / BridgeSpawner / PickupSpawner
- [ ] `Cutter` 추상 + 3종, `Painter`, `Assembler` 추상 + 2종, `ElecPartCollector`, `Packager`

## Phase 4 — Technician + State 패턴

브랜치: `back/feat/technician`. 상세: [phase_4_worker.md](phases/phase_4_worker.md)

- [ ] `Technician` (SimulationObject 직접 상속) — `targetMachine_`, `repairProgress_`, `ITechnicianState*`
- [ ] `ITechnicianState` + `IdleState` / `WorkingState` 싱글톤
- [ ] 수리 완료 시 `machine.repair()` 호출 + `Resume` publish

## Phase 5 — Orchestrator (Manager / Memento / ScenarioLoader)

브랜치: `back/feat/orchestrator`. 상세: [phase_5_orchestrator.md](phases/phase_5_orchestrator.md)

- [ ] `EngineerManager` — `Fault` 구독, 수리 대기 큐(packager 의존성 기준 우선순위), idle Technician 배정
- [ ] `MementoStore` — `std::deque<FactorySnap>`, 매 틱 push, `rewind(tick)` 복원
- [ ] `ScenarioLoader` — JSON 파싱 → create cmd 목록 반환
- [ ] `scenarios/*.json` 4개 (Normal / Breakdowns / Bottleneck / Overflow)

## Phase 6 — Factory & SimulationRunner

브랜치: `back/feat/factory`. 상세: [phase_6_factory.md](phases/phase_6_factory.md)

- [ ] `Factory` — `unique_ptr` 컨테이너, `createMachine/createConveyor/createTechnician`, 외부 cmd 메서드 (`start/pause/reset/setSpeed/setScenario/forceBreak/instantRepair/rewind/clearLog`), `snapshot()`
- [ ] `Factory.applyConfig` — Backpressure 토픽 구독 + (SmartFactory 시) Fault cascade DAG closure 계산 후 (Fault/Resume, x.id) 토픽 일괄 구독
- [ ] `Factory.tick()` — base 포인터 루프로 모든 객체 `update()`, broker `flush()` 호출은 Runner가 담당
- [ ] `SimulationRunner` — `tickInterval`, accumulator, running, `tryAdvance(realDt)` 한 번에 `factory.tick()` + `broker.flush()` + `mementoStore.push()` 호출
- [ ] RNG (`std::mt19937`) + `ProductIdGen` 보유, Reset 시 시드 재발급 + 카운터 0 리셋, 메멘토 직렬화/복원

## Phase 7 — Controller & 통합

브랜치: `back/feat/main-integration`. 상세: [phase_7_integration.md](phases/phase_7_integration.md)

- [ ] `Controller::dispatch(MachineCmd)` — action 분기 → Factory 도메인 메서드 / Runner 메서드 호출
- [ ] `main.cpp` — Factory + Runner + Controller + View 생성 및 와이어링, 매 프레임 `runner.tryAdvance(io.DeltaTime)`
- [ ] UML 다이어그램 마감
- [ ] GitHub Pages 배포

---

## 백/프론트 동기화 포인트

| 시점 | 프론트 가능 작업 |
|---|---|
| Phase 0 머지 후 | mock `FactorySnap`으로 UI 레이아웃·위젯 검증 |
| Phase 6 머지 후 | 진짜 Factory 데이터로 렌더링 검증 |
| Phase 7 머지 후 | 진짜 Controller 경로로 버튼 입력 검증 |
