# Phase 6 — Factory + SimulationRunner + Controller (backend 마무리)

## 목표

객체 컨테이너 + 클럭 + 외부 cmd dispatch. Phase 1~5 부품 조립 + Controller까지 묶어 backend 측 마무리.

> **작업 범위**: backend (`model/` + `controller/`). View 패널과 `main.cpp`의 backend↔view 와이어링은 별도 frontend phase에서 진행.

## 적용 패턴

- **Factory (메서드)**: `createMachine/Conveyor/Technician` — 타입별 switch는 여기 1곳만 (simulation loop엔 if/else 없음 원칙 유지)
- **Memento Originator**: `Factory.snapshot()` / `restore(snap)`
- **MVC Controller**: `dispatch(MachineCmd)` — 자체 상태 없음, 도메인 메서드 호출만

## 구성요소

### `Factory`

```
// 소유 컨테이너
machines_:     vector<unique_ptr<Machine>>
conveyors_:    vector<unique_ptr<Conveyor>>
technicians_:  vector<unique_ptr<Technician>>

// 협력자 (참조 주입)
broker_, eventLog_, statistics_, technicianManager_

// 시뮬 상태
tick_: int
scenario_: ScenarioType
rng_: mt19937
idGen_: ProductIdGen

// 생성 (Factory 패턴)
createMachine(MachineDef, OverflowMode) → Machine*   // 내부 switch(type) 1곳
createConveyor(ConveyorDef)              → Conveyor*
createTechnician(TechnicianDef)          → Technician*
applyConfig(ScenarioConfig)                          // 위 create들 + 와이어링

// 조회 (벡터 선형 검색) — IMachineLookup 구현
findMachine(string id)    → Machine*
findConveyor(string id)   → Conveyor*
findTechnician(string id) → Technician*

// 도메인 메서드 (외부 cmd → Controller가 호출)
reset()
setScenario(ScenarioType)        // ScenarioLoader 호출은 Controller 책임
forceBreak(string machineId)     // machine.forceBreak() (health=0)
instantRepair(string machineId)  // machine.repair() 직접 호출 (Technician 우회)
clearLog()                       // eventLog_.clear()

// 시뮬레이션
tick(): base 포인터 루프로 모든 객체 update + manager.update
snapshot(): 현재 상태 → FactorySnap
restore(FactorySnap): 메멘토 복원
```

**조회는 벡터 + 선형검색** — 머신 13개 / 컨베이어 12개 수준이라 unordered_map 도입 가치 없음.

**Factory가 `IMachineLookup`을 구현** — `findMachine`은 인터페이스 메서드. `TechnicianManager`가 setter 주입 (`techMgr.setLookup(factory)`)으로 받음. 생성 순서: `TechnicianManager(broker, nullLookup) → Factory(...) → techMgr.setLookup(factory)`.

### `applyConfig` 와이어링 순서

1. `reset()` 호출 (broker 토픽 구독 정리, 컨테이너 / 카운터 / RNG 시드 재발급 등)
2. `createConveyor` 일괄 호출 (downstream 포인터는 4단계에서 설정)
3. `createMachine` 일괄 호출 + outputConveyor 와이어링 (outputConveyor의 mode를 머신의 `outputOverflowMode_`로 전달)
4. `createTechnician` 일괄 + `technicianManager.registerTechnician`
5. 각 conveyor에 대해 `c.setDownstream(findMachine(c.downstreamId))`
6. **Priority map 계산 + 주입** (모든 시나리오 공통):
   - Sink 정의: `outputConveyorId == ""` 인 머신 (현 5종 시나리오는 모두 Packager 단일)
   - Machine → outputConveyor → downstreamMachine 체인의 역방향 adjacency 구성 후, 모든 sink를 시드로 단일 BFS → `unordered_map<string,int>` (machineId → 거리). 도달 불가 머신은 99
   - `technicianManager.setPriorityMap(map)` 호출
   - 인스턴스 단위 priority. phase 5의 priority 표는 표준 13-머신 토폴로지 결과 예시일 뿐 — source of truth는 본 단계의 계산 결과
7. **Backpressure 토픽 구독 등록** (SmartFactory 한정): 각 Machine M에 대해 `M.outputConveyor.mode == Backpressure`면 `broker.subscribe(EventType::Backpressure, M.outputConveyor.id, M)`
8. **Fault cascade 토픽 구독 등록** (SmartFactory 한정):
   - 각 Machine M에 대해 forward DAG로 하류 transitive closure `D(M)` 수집 (BFS)
   - `X ∈ D(M)`마다:
     - `broker.subscribe(EventType::Fault,  X.id, M)`
     - `broker.subscribe(EventType::Resume, X.id, M)`

> **Drop 시나리오는 7·8 단계 skip**. Machine 인스턴스는 동일 클래스라도 시나리오에 따라 cascade 동작 유무가 갈림 — 구독 등록 여부로만 분기, Machine 클래스 내부 분기 없음.

**`tick()` 순서**:

1. `++tick_`
2. machines (Spawner 포함) update — base 포인터 루프
3. conveyors update
4. technicians update
5. technicianManager update (Fault 큐 ↔ idle Technician 매칭)

`broker.flush()` 호출은 Runner가 담당.

> **순서 결정 근거**: machines를 먼저 update하면 같은 틱에 conveyor 출구가 downstream 머신의 inputBuffer로 들어가는 흐름이 한 틱 손해 없이 이어진다.

**start/pause/setSpeed는 Factory에 없음** — `SimulationRunner` 담당.

### `SimulationRunner`

```
factory_, broker_, mementoStore_     // 참조 주입
tickIntervalSec_: double             // 0.6 / speedMultiplier (초 단위, ImGuiIO::DeltaTime와 동일 단위)
accumulator_: double
running_: bool
speedMultiplier_: int (1~5, clamp)

start / pause / reset
setSpeed(int 1~5): tickIntervalSec_ = 0.6 / n  (clamp 1~5)
tryAdvance(realDt):
  if (!running) return
  accumulator += realDt
  while (accumulator >= tickIntervalSec):
    factory_.tick()
    broker_.flush()
    mementoStore_.push(factory_.snapshot())
    accumulator -= tickIntervalSec
```

### RNG 정책 + 직렬화

- Factory가 `std::mt19937` 1개 보유
- `reset()` 시 `std::random_device`로 재시드
- 머신의 health drop 체크에 사용 → Machine 생성 시 `rng_&` 참조 주입 (개별 RNG 안 만듦)
- **메멘토 직렬화**: `FactorySnap.rngState: string`에 `std::stringstream ss; ss << rng_;` 결과 저장. restore 시 `ss >> rng_;`로 복원. mt19937은 표준이 streaming operator를 보장하므로 portable
- rewind 시 RNG도 같은 시점으로 되돌아가 동일 sequence 재현 (deterministic replay 가능)

### `restore(FactorySnap)` 정책

- 객체 재생성 안 함. 기존 `unique_ptr` 컨테이너 유지하고 **필드 일괄 덮어쓰기** (id 매칭으로 snap → 실제 객체)
  - **Machine**: `restoreFromSnap(snap)` 가상 메서드. health, processingTick, currentProduct (ProductSnap → unique_ptr<Product> 재구성), inputBuffer (`clearInputs()` + `acceptProduct(...)` 다형성 호출 → MultiInputMachine.typedBuffer로 자동 분류), `currentState_` derive (health==0 → Broken, currentProduct/progress 있으면 Processing, else Idle), pendingDownstreamFaults_=0 (구독이 재계산하도록)
  - **Conveyor**: `slots_` 직접 덮어쓰기 (friend class Factory). ProductSnap → unique_ptr<Product>
  - **Technician**: `targetMachine_` (id로 findMachine), `repairProgress_`, `currentState_` (target 유무로 derive)
  - **Statistics**: `setSnapshot(finished, wip, breakdowns, lost)` 일괄 set
  - **EventLog**: `setLogs(snap.logs)` (200 cap 유지)
  - **EventBroker**: `restoreQueue(snap.pendingEvents)` (queue 일괄 재구성, 구독자는 유지)
  - **TechnicianManager**: `restoreQueue(entries, maxSeq+1)` (pendingRepairs → QueueEntry)
  - **RNG**: stringstream으로 역직렬화
  - **ProductIdGen**: `setCounter(snap.productIdCounter)`
- snap에 없는 객체는 무시 (시나리오 변경된 경우는 reset+applyConfig가 별도)

### `Controller`

```
factory_, runner_, mementoStore_, scenarioLoader_     // 참조 주입

dispatch(MachineCmd cmd):
  switch (cmd.action):
    None           → noop
    Start          → runner_.start()
    Pause          → runner_.pause()
    Reset          → runner_.reset(); factory_.reset()
    SetSpeed       → runner_.setSpeed(cmd.speedMultiplier)
    SetScenario    → factory_.reset()
                     auto cfg = scenarioLoader_.load(cmd.scenario)
                     factory_.applyConfig(cfg)
                     mementoStore_.clear()
    ForceBreak     → factory_.forceBreak(cmd.targetMachineId)
    InstantRepair  → factory_.instantRepair(cmd.targetMachineId)
    Rewind         → auto snap = mementoStore_.rewind(cmd.rewindTargetTick)
                     factory_.restore(snap)
    ClearLog       → factory_.clearLog()
```

순수 dispatch. Model과 협력자를 알지만 둘의 데이터 흐름엔 개입 안 함.

### `forceBreak` / `instantRepair` 동작

- **forceBreak**: `findMachine(id)` → `m.forceBreak()` 호출 → `health_ = 0` 설정. 다음 update 틱에서 IdleState/ProcessingState가 health 체크 후 자연스럽게 BrokenState 전이. 즉시 전이는 IdleState에서도 동작해야 하므로 IdleState.update에서도 health == 0 체크 포함 (구현됨)
- **instantRepair**: `findMachine(id)` → `m.repair(tick_)` 직접 호출. Technician/TechnicianManager 우회. 큐에 이미 있다면 매니저가 다음 update에서 idle 상태 발견 후 큐에서 자동 제외 (stale head pop)

> **주의**: 현재 `MachineProcessingState::update`의 health<=0 체크는 `breakdownProb > 0` 가드 안에 있어, bp=0인 머신이 Processing 중에 forceBreak 받으면 다음 cycle 종료 후에 Broken 전이됨. bp>0이거나 Idle 중이면 즉시 전이. 동작상 큰 문제는 없으나 후속 phase에서 가드 위치 보정 가능.

### 순환 의존성 해결

- `TechnicianManager`는 `IMachineLookup&` 참조를 (내부적으로는 pointer로) 보유. Factory가 `IMachineLookup`을 구현
- 생성 순서: `TechnicianManager(broker, nullLookup) → Factory(broker, log, stats, mgr) → mgr.setLookup(factory)`
- main / 테스트 / harness 모두 동일한 패턴 (`NullLookup` placeholder + setter 주입)

## 의존성

- Phase 1~5 전체
- frontend phase가 main 와이어링과 패널 5종을 별도 진행 (이 phase 산출물 위에 빌드)

## 테스트

`tests/phase_6_factory.cpp` (소형 시나리오 — spawner 1 + cutter 1 + packager):
- `applyConfig` 후 객체 개수 검증
- `findMachine/findConveyor/findTechnician`으로 id 조회 가능
- `tick()` 호출 후 spawner output 진행
- `forceBreak` → 다음 tick에서 health 0 + Fault 발행 + BrokenState
- `instantRepair` → health 즉시 복원 + Resume 발행
- `reset()` → 컨테이너 / 카운터 클리어
- priority map BFS → PACK=0 / CUT=1 / SPN=2 검증

`tests/phase_6_cascade.cpp` (SmartFactory mini DAG — spawner A + B → collector → assembler):
- SmartFactory에서 ELEC forceBreak → spawner A·B의 `pendingDownstreamFaults_ == 1`, canStart false
- ELEC instantRepair → counter 0 복귀
- ELEC + PA 동시 Fault → counter 2 → PA만 Resume → 1 (refcount 검증)
- Drop 시나리오에서는 같은 forceBreak이어도 상류 counter 변동 없음 (구독 등록 skip)

`tests/phase_6_runner.cpp`:
- start/pause/setSpeed 동작
- tryAdvance(realDt): accumulator 누적, tickInterval 도달 시 factory.tick + memento.push 호출 횟수 검증
- speed clamp (0 → 1, 99 → 5)
- reset → running false + mementoStore 비움

`tests/phase_6_memento_rng.cpp`:
- mt19937 stream round-trip — 같은 seed 후 sequence 일치
- snapshot 캡처 (tick / 객체 수 / rngState 비어있지 않음)
- restore 후 tick / outputCount 복원
- restore 후 RNG sequence 재현 (deterministic replay)

`tests/phase_7_controller.cpp` (이름은 history 보존 — 본 phase에 통합됨):
- 각 CmdAction → 해당 도메인 메서드 호출 검증
- SetScenario → factory.reset + loader.load + factory.applyConfig + mementoStore.clear
- Rewind → mementoStore.rewind + factory.restore
- None은 noop

`tests/phase_7_e2e.cpp` (이름은 history 보존):
- Normal 시나리오 200틱 → finished > 0
- Breakdowns 1500틱 → breakdowns > 0 (통계적 여유)
- 30틱 진행 → snapshot → 30틱 더 → restore → 30틱 → 동일 finished count (deterministic 검증)

## 산출 브랜치

`back/feat/orchestrator` (Phase 5 + 6 통합) 또는 별도 `back/feat/factory-controller`

## 후속

- snapshot 캐시 도입 (frontend가 매 프레임 호출 시 성능 측정 후 결정)
- View 측에서 추가 cmd 액션이 필요해지면 `CmdAction` 확장
- `MachineProcessingState::update`의 health<=0 체크를 breakdownProb 가드 밖으로 이동
