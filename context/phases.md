# Phase 계획

백엔드 작업 페이즈별 작업 리스트. 브랜치 prefix: `back/feat/...`

## Phase 0 — Scaffold

공통 인터페이스를 머지해 백/프론트 병렬 작업이 가능하게 한다.

- [x] `CMakeLists.txt` 작성 (ImGui static lib target, SDL2/OpenGL3 링크, `src/` 자동 수집)
- [x] `src/{common,model,view,controller}/` 디렉토리 생성
- [x] `src/common/Types.h` — `CmdAction`, `MachineState`, `ScenarioType`, `ProductType` enum
- [x] `src/common/MachineCmd.h` — View → Controller 구조체
- [x] `src/common/LogEntry.h` — `{tick, sourceId, message}`
- [x] `src/common/FactorySnap.h` — ProductSnap / MachineSnap / ConveyorSnap / SpawnerSnap / TechnicianSnap / StatisticsSnap / FactorySnap
- [x] 기존 `src/main.cpp` 유지 상태로 빌드 통과 확인

## Phase 1 — Model 빌딩 블록 (M1 5/23 전까지)

의존성이 적은 leaf 클래스. 가능하면 병렬.

- [ ] `back/feat/sim-object` — `SimulationObject` 추상 (`update`, `getInfo`, `getId`, `needsRepair`, `assignWorker`), EventLog 참조 주입
- [ ] `back/feat/products` — `Product` 추상 + 9개 구체 (RawWood / HeadPart / NeckPart / BodyPart(isPainted) / Bridge / Pickup / ElecPartSet / AssembledBody / FinishedGuitar) + 전역 product ID 카운터
- [ ] `back/feat/event-stats` — `EventLog` (max 200, 가장 오래된 것부터 drop) + `Statistics`
- [ ] `back/feat/conveyor` — `Conveyor` (capacity, push/pop, 오버플로우 시 lost 시그널)

## Phase 2 — 능동 객체

- [ ] `back/feat/machines` — `Machine` 추상 (상태/체력/진행도, BROKEN 전이) + Cutter 추상 + HeadCutter / NeckCutter / BodyCutter + Painter + Assembler 추상 + BodyAssembler / PartAssembler + ElecPartCollector + Packager
- [ ] `back/feat/spawners` — `Spawner` 추상 + WoodSpawner (라운드 로빈) + ElecPartSpawner
- [ ] `back/feat/technician` — `Worker` 추상 + `Technician` (WAITING / MOVING / REPAIRING, 3틱 수리)
- [ ] `back/feat/scenario` — `Scenario` 타입 + `apply(Factory&)` 파라미터 주입

## Phase 3 — Factory 조정자

브랜치: `back/feat/factory`

- [ ] 객체 소유 (`unique_ptr` 컨테이너)
- [ ] 파이프라인 셋업 (Spawner ↔ Conveyor ↔ Machine 와이어링)
- [ ] `update(tick)` 디스패치 — base 포인터 루프, concrete type if/else 없음
- [ ] BROKEN 머신 ↔ IDLE Technician 매칭
- [ ] RNG 인스턴스 (`std::mt19937`) 보유, Reset 시 `std::random_device`로 시드
- [ ] 도메인 메서드 노출 — `start`, `pause`, `reset`, `setSpeed`, `setScenario`, `forceBreak`, `instantRepair`, `clearLog`
- [ ] `snapshot()` — 현재 상태로 `FactorySnap` 생성

## Phase 4 — 메멘토 & Controller

- [ ] `back/feat/memento` — `std::deque<FactorySnap>` + `firstTick_` 인덱스, 매 틱 push_back, `rewind(targetTick)` 복원, RNG 상태 직렬화/복원 (`operator<<` / `>>`)
- [ ] `back/feat/controller` — `Controller::dispatch(MachineCmd)` 분기 → Factory 도메인 메서드 호출

## Phase 5 — 통합 (6/1~5, UI 담당과 함께)

- [ ] `back/feat/main-integration` — `main.cpp` 갈아끼우기 (Factory + Controller + View 생성, 메인 루프)
- [ ] 디버깅 / 통합 테스트
- [ ] UML 다이어그램 마감
- [ ] GitHub Pages 배포

---

## 백/프론트 동기화 포인트

| 시점 | 프론트 가능 작업 |
|---|---|
| Phase 0 머지 후 | mock `FactorySnap`으로 UI 레이아웃·위젯 검증 |
| Phase 3 머지 후 | 진짜 Factory 데이터로 렌더링 검증 |
| Phase 4 머지 후 | 진짜 Controller 경로로 버튼 입력 검증 |
