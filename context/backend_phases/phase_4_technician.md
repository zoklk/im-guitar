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
- **TechnicianIdleState**: update no-op. `targetMachine_ == nullptr` 보장. EngineerManager가 `assign()` 호출 시 외부에서 전이
- **TechnicianWorkingState**:
  - `onEnter`: `repairProgress_ = 0`
  - `update`: `repairProgress_++`. repairTime 도달 시 → `targetMachine_->repair()` → `targetMachine_ = nullptr` → `TechnicianIdleState` 전이

> Resume 이벤트 발행은 `Machine.repair()` 내부에서 처리 (sourceId=`machine.id`). Technician은 별도 발행 안 함 — instantRepair 경로(Phase 6)와 일관성 유지 + Fault cascade 구독자가 `machine.id` 토픽을 기대하기 때문. Phase 3 명세 참조.

### `Technician` (SimulationObject 자식)

```
targetMachine_: Machine*
repairProgress_: int
repairTime_: int (default 3)
currentState_: ITechnicianState*

update(tick): currentState_->update(*this, tick)
assign(Machine*): targetMachine_ = m; transitionTo(TechnicianWorkingState::instance())
isIdle(): currentState_ == &TechnicianIdleState::instance()
```

EngineerManager가 `assign()` 호출 → TechnicianWorkingState 진입 → 3틱 후 자동 복귀.

### 이벤트 발행

Technician은 자체 이벤트 발행 없음. 수리 완료 시 `machine.repair()` 호출 결과로 Machine 측에서 `Resume` (sourceId=machine.id)을 발행 — Phase 3 표 참조.

**Resume 이벤트 용도** (SmartFactory 한정):
- EventLog 텍스트 마커
- Fault cascade 구독 머신의 `pendingDownstreamFaults_` 감소 트리거 (Phase 3)
- 자체 Machine의 Backpressure 정지 해제는 별개 — `outputConveyor.canAccept()` 폴링이 담당, Resume 이벤트에 의존 X

## 소유 방향

- **Technician → Machine 단방향** (`targetMachine_` 보유)
- Machine은 누가 수리하는지 모름. snap의 `MachineSnap.assignedTechId`는 Factory.snapshot()이 Technician 목록을 훑어 derive

## 의존성

- Phase 1 (EventBroker)
- Phase 2 (SimulationObject)
- Phase 3 (Machine — `repair()` 호출, forward declaration 가능)

## 테스트

`tests/phase_4_technician.cpp` (mock Machine):

- TechnicianIdleState 시작: repairProgress 0, targetMachine null. update no-op
- assign(mock) → TechnicianWorkingState 진입. onEnter에서 repairProgress 0 셋
- 1~2틱 진행: repairProgress 증가, machine.repair 호출 안 됨
- repairTime 도달 (3틱): machine.repair 호출 검증 (mock Machine 측에서 Resume publish 책임 — 본 테스트 범위 밖) + targetMachine null + TechnicianIdleState 복귀
- isIdle() 정확성: TechnicianWorkingState 중 false, 복귀 후 true

## 산출 브랜치

`back/feat/technician`

## 후속

- 없음 (단순 구조)
