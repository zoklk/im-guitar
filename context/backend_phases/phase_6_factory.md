# Phase 6 — Factory & SimulationRunner

## 목표

객체 컨테이너 + 클럭. Phase 1~5 부품 조립.

## 적용 패턴

- **Factory (메서드)**: `createMachine/Conveyor/Technician` — 타입별 switch는 여기 1곳만 (시뮬레이션 루프엔 if/else 없음 원칙 유지)
- **Memento Originator**: `Factory.snapshot()` / `restore(snap)`

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

// 생성 (Factory 패턴)
createMachine(MachineDef) → Machine*           // 내부 switch(type)
createConveyor(ConveyorDef) → Conveyor*
createTechnician(TechnicianDef) → Technician*
applyConfig(ScenarioConfig)                    // 위 create들 일괄 호출 + 와이어링

// 조회 (벡터 선형 검색)
findMachine(string id) → Machine*              // TechnicianManager용. machines_ 선형 스캔
findConveyor(string id) → Conveyor*

// 도메인 메서드 (외부 cmd → Controller가 호출)
reset()
setScenario(ScenarioType)        // ScenarioLoader 호출은 Controller 책임
forceBreak(string machineId)     // machine.forceBreak()
instantRepair(string machineId)  // machine.repair() 직접 호출 (Technician 우회)
clearLog()                       // eventLog_.clear()

// 시뮬레이션
tick(): base 포인터 루프로 모든 객체 update
snapshot(): 현재 상태 → FactorySnap
restore(FactorySnap): 메멘토 복원
```

**조회는 벡터 + 선형검색** — 머신 13개 / 컨베이어 12개 수준이라 unordered_map 도입 가치 없음.

### `applyConfig` 와이어링 순서

1. `createConveyor` 일괄 호출 (downstream 포인터는 나중에 설정)
2. `createMachine` 일괄 호출 (inputConveyor / outputConveyor 참조 와이어링 포함)
3. `createTechnician` 일괄 호출
4. **Backpressure 토픽 구독 등록** (SmartFactory 한정): 각 Machine M에 대해 `M.outputConveyor.mode == Backpressure`면 `broker.subscribe(EventType::Backpressure, M.outputConveyor.id, M)`
5. **Fault cascade 토픽 구독 등록** (SmartFactory 한정):
   - 각 Conveyor가 `downstreamMachineId`를 들고 있으므로 (Phase 0 ConveyorSnap), Machine → outputConveyor → downstreamMachine 체인으로 DAG 구성 가능
   - 각 Machine M에 대해 BFS/DFS로 하류 transitive closure `D(M)` 수집
   - `X ∈ D(M)`마다:
     - `broker.subscribe(EventType::Fault,  X.id, M)`
     - `broker.subscribe(EventType::Resume, X.id, M)`
   - 구독 등록 1회로 끝 — Reset 시 broker도 재생성/구독 초기화되므로 누수 없음
6. Machine별 RNG 참조 / ProductIdGen 참조 / Statistics 참조 등 DI 마무리

> **Drop 시나리오는 4·5 단계 skip**. Machine 인스턴스는 동일 클래스라도 시나리오에 따라 cascade 동작 유무가 갈림 — 구독 등록 여부로만 분기, Machine 클래스 내부 분기 없음.

**tick() 순서**:

1. machines (Spawner 포함) update — base 포인터 루프
2. conveyors update
3. technicians update
4. technicianManager update (Fault 큐 ↔ idle Technician 매칭)

한 틱 내 흐름 일관성을 위해 명시 고정. concrete 타입 if/else 없음.

> **순서 결정 근거**: machines를 먼저 update하면 같은 틱에 conveyor 출구가 downstream 머신의 inputBuffer로 들어가는 흐름이 한 틱 손해 없이 이어진다 (machine A가 conveyor 출구 슬롯을 비우기 전에 conveyor가 shift하면 출구가 두 번 비는 시점 발생). machine → conveyor → technician 순으로 고정해야 product 이동이 1틱 내 인접 객체까지만 전파되어 처리량 추정이 단순해진다.

**start/pause/setSpeed는 Factory에 없음** — SimulationRunner 담당.

### `SimulationRunner`

```
factory_, broker_, mementoStore_     // 참조 주입
tickInterval_: double                 // 600ms / speedMultiplier (밀리초)
accumulator_: double
running_: bool

start / pause / reset
setSpeed(int 1~5): tickInterval_ = 600.0 / n
tryAdvance(realDt):
  if (!running) return
  accumulator += realDt
  while (accumulator >= tickInterval):
    factory_.tick()
    broker_.flush()
    mementoStore_.push(factory_.snapshot())
    accumulator -= tickInterval
```

### RNG 정책 + 직렬화

- Factory가 `std::mt19937` 1개 보유
- Reset / setScenario 시 `std::random_device`로 재시드
- 머신의 health drop 체크에 사용 → Machine 생성 시 `rng_&` 참조 주입 (개별 RNG 안 만듦)
- **메멘토 직렬화**: `FactorySnap.rngState: string`에 `std::stringstream ss; ss << rng_;` 결과 저장. restore 시 `ss >> rng_;`로 복원. mt19937은 표준이 streaming operator를 보장하므로 portable
- rewind 시 RNG도 같은 시점으로 되돌아가 동일 sequence 재현 (deterministic replay 가능)

### restore(FactorySnap) 정책

- 객체 재생성 안 함. 기존 unique_ptr 컨테이너 유지하고 **필드 일괄 덮어쓰기** (id 매칭으로 snap → 실제 객체)
  - Machine: health, processingTick, currentProduct (snap의 ProductSnap에서 실제 unique_ptr<Product> 재구성), inputBuffer, currentState_ (derive 규칙 역적용)
  - Conveyor: slots (ProductSnap → unique_ptr<Product>)
  - Technician: targetMachine_, repairProgress, currentState_
  - statistics, eventLog, broker.eventQueue (pendingEvents에서 복원), rng_, tick_
- snap에 없는 객체는 무시 (시나리오 변경된 경우는 reset+applyConfig가 별도)

명시: 데이터 멤버 자체를 snap한 뒤 그대로 덮어쓰는 구조라 restore = 필드 대입 + 상태 derive. 별도 복잡한 graph 재구성 없음.

### setScenario 흐름 (Controller 책임)

1. `factory.reset()`
2. `scenarioLoader.load(type)` → `ScenarioConfig`
3. `factory.applyConfig(config)`

Factory.setScenario는 ScenarioLoader를 모름 — Controller가 두 호출 묶음.

### forceBreak / instantRepair 동작

- **forceBreak**: `findMachine(id)` → `m.forceBreak()` 호출 → health = 0 설정. 다음 update 틱에서 ProcessingState/IdleState가 health 체크 후 자연스럽게 BrokenState 전이. 즉시 전이는 IdleState에서도 동작해야 하므로 IdleState.update에서도 health == 0 체크 추가 필요
- **instantRepair**: `findMachine(id)` → `m.repair()` 직접 호출. Technician/TechnicianManager 우회. 큐에 이미 있다면 매니저가 다음 update에서 idle 상태 발견 후 큐에서 자동 제외

## 의존성

- Phase 1~5 전체

## 테스트

`tests/phase_6_factory.cpp` (소형 시나리오 — spawner 1 + conveyor 1 + cutter 1):
- applyConfig 후 객체 개수 검증
- findMachine으로 id 조회 가능
- tick() 호출 후 spawner output, conveyor 슬롯 진행 검증
- forceBreak → 다음 tick에서 health 0 + Fault 발행
- instantRepair → health 즉시 복원 + Resume 발행

`tests/phase_6_cascade.cpp` (SmartFactory 시나리오 mini DAG — spawner A + spawner B → collector → assembler):
- applyConfig 후 spawner A / B / collector가 assembler 하류 closure에 포함되어 (Fault, assembler.id) / (Resume, assembler.id) 토픽 구독됨 (broker 내부 상태 확인 or mock handler)
- assembler forceBreak → flush 후 spawner A·B·collector의 `pendingDownstreamFaults_ == 1`, canStart false
- assembler instantRepair → flush 후 `pendingDownstreamFaults_ == 0`, canStart 복귀
- collector + assembler 동시 Fault → spawner A·B 카운터 2 → assembler만 Resume → 카운터 1, 여전히 정지 → collector도 Resume → 카운터 0, 재개 (refcount 검증)
- Drop 시나리오에서는 같은 forceBreak이어도 상류 카운터 변동 없음 (구독 등록이 skip되었으므로)

`tests/phase_6_runner.cpp`:
- start/pause/setSpeed 동작
- tryAdvance(realDt): accumulator 누적, tickInterval 도달 시 factory.tick 호출 횟수 검증
- speedMultiplier 변경 시 interval 변경

`tests/phase_6_memento_rng.cpp`:
- rng state 직렬화/역직렬화 round-trip — 같은 seed 후 sequence 일치
- restore 후 다음 tick의 RNG 결과 재현 가능

## 산출 브랜치

`back/feat/factory` + `back/feat/runner` (선택 분할)

## 후속

- TechnicianManager 큐 정리 (instantRepair 후 큐 잔존 머신 제거) 로직 — 매니저 update 진입부에서 큐 상단이 idle이면 pop
