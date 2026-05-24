# Phase 4 — Technician + State 패턴

## 목표

수리공 단일 클래스. Machine과 동일한 State 패턴 (싱글톤 + DI). 머신과 독립이라 Phase 3과 병렬 가능.

## 적용 패턴

- **State** (싱글톤 + DI): `ITechnicianState` + `TechnicianIdleState` / `TechnicianWorkingState`

> 클래스명 접두사 (`Technician*`): Machine의 `MachineIdleState`와 글로벌 네임스페이스에서 이름 충돌 회피.

## 구성요소

> Worker 추상은 두지 않음. 현재 수리공 1종이라 YAGNI — Technician이 SimulationObject 직접 상속. 다른 직군(청소부 / 검수원 등) 추가 계획 없음.

### `ITechnicianState`

```
virtual void update(Technician&, int tick) = 0
virtual void onEnter(Technician&)  {}
virtual void onExit(Technician&)   {}
virtual const char* name() const = 0
static <Concrete>& instance()
```

자식:
- **TechnicianIdleState**: update no-op. `targetMachine_ == nullptr` 보장. TechnicianManager가 `assign()` 호출 시 외부에서 전이
- **TechnicianWorkingState**:
  - `onEnter`: `repairProgress_ = 0`
  - `update`: `repairProgress_++`. repairTime 도달 시 → `targetMachine_->repair()` → `targetMachine_ = nullptr` → `TechnicianIdleState` 전이

> Resume 이벤트 발행은 `Machine.repair()` 내부에서 처리 (sourceId=`machine.id`). Technician은 별도 발행 안 함 — instantRepair 경로(Phase 6)와 일관성 유지 + Fault cascade 구독자가 `machine.id` 토픽을 기대하기 때문. Phase 3 명세 참조.

### `Technician` (SimulationObject 자식)

```
targetMachine_: Machine*
repairProgress_: int
repairTime_: int (ctor 인자, 시나리오 튜닝 가능)
currentState_: ITechnicianState*

update(tick): currentState_->update(*this, tick)
assign(Machine*, int tick):
  assert(currentState_ == &TechnicianIdleState::instance())   // Working 중 재배정은 호출자 버그
  assert(m != nullptr)
  targetMachine_ = m
  transitionTo(TechnicianWorkingState::instance(), tick)
transitionTo(ITechnicianState&, int tick): onExit → swap → onEnter
isIdle(): currentState_ == &TechnicianIdleState::instance()
```

TechnicianManager가 `assign()` 호출 → TechnicianWorkingState 진입 → repairTime틱 후 자동 복귀.

**`targetMachine_` 불변 유지 정책**: assert 없이 진입점 제한으로 보장
- 셋: `assign(m, tick)` 한 곳에서만 (nullptr → m)
- 리셋: `WorkingState.update` 완료 분기 한 곳에서만 (m → nullptr)
- 외부에서 직접 접근 불가 (private + assign 메서드만 노출)

`Idle.update`에 `targetMachine_ == nullptr` assert 두지 않음 — 도달 불가능한 경로의 검증은 가치 낮음. 대신 `assign` 진입부의 `currentState_ == Idle` assert가 호출자 버그(double-assign)를 잡아 더 의미 있다.

### 이벤트 발행

Technician은 자체 이벤트 발행 없음. 수리 완료 시 `machine.repair()` 호출 결과로 Machine 측에서 `Resume` (sourceId=machine.id)을 발행 — Phase 3 표 참조.

**Resume 이벤트 용도** (SmartFactory 한정):
- EventLog 텍스트 마커
- Fault cascade 구독 머신의 `pendingDownstreamFaults_` 감소 트리거 (Phase 3)
- 자체 Machine의 Backpressure 정지 해제는 별개 — `outputConveyor.canAccept()` 폴링이 담당, Resume 이벤트에 의존 X

## 소유 방향

- **Technician → Machine 단방향** (`targetMachine_` 보유)
- Machine은 누가 수리하는지 모름. snap의 `MachineSnap.assignedTechId`는 Factory.snapshot()이 Technician 목록을 훑어 derive
- `TechnicianSnap`에 state 필드는 두지 않음 — Machine과 동일하게 `targetMachineId.has_value()` 또는 `repairProgress > 0`으로 Working/Idle을 derive. UI가 직접 표시할 때만 한 줄로 변환

## 의존성

- Phase 1 (EventBroker)
- Phase 2 (SimulationObject)
- Phase 3 (Machine — `repair()` 호출, forward declaration 가능)

## 테스트

`tests/phase_4_technician.cpp` (`SpyMachine` 스텁 — Machine 자식, process는 no-op):

`Machine::repair`가 non-virtual이라 직접 hook 불가. forceBreak으로 health=0 진입시킨 뒤 repair 호출 시 health가 maxHealth로 복원되는 변화를 관찰해 간접 검증.

- StartsInIdleStateWithNoTarget: 초기 상태 (Idle, targetMachine null, repairProgress 0)
- IdleUpdateIsNoOp: Idle 상태에서 update 여러 번 호출해도 상태 불변
- AssignTransitionsToWorkingAndResetsProgress: assign → Working 진입, onEnter가 repairProgress 0 셋
- ProgressIncrementsWithoutTriggeringRepairBeforeRepairTime: 1~2틱 진행 시 progress 증가, health 여전히 0
- ReachesRepairTimeTriggersRepairAndReturnsToIdle: 3틱 도달 시 health 복원 + Idle 복귀 + targetMachine null
- RepairCompletionPublishesResumeAtCompletionTick: Machine.repair가 Resume 이벤트 발행 (sourceId=machine.id, tick=완료 시점)
- CanReassignToAnotherMachineAfterCompletion: 한 사이클 완료 후 다른 머신 재배정 가능 (progress 0 리셋 검증)

## 산출 브랜치

`back/feat/technician`

## 후속

- 없음 (단순 구조)
