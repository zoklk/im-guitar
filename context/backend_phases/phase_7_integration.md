# Phase 7 — Controller & 통합

## 목표

얇은 Controller로 부품 연결, `main.cpp` 갈아끼우기, 최종 산출물 마감.

## 적용 패턴

- **MVC**: Controller는 cmd dispatch 전용. 자체 상태 없음, 비즈니스 위임만

## 구성요소

### `Controller`

```
factory_, runner_, mementoStore_, scenarioLoader_     // 참조 주입

dispatch(MachineCmd cmd):
  switch (cmd.action):
    Start          → runner_.start()
    Pause          → runner_.pause()
    Reset          → runner_.reset(); factory_.reset()
    SetSpeed       → runner_.setSpeed(cmd.speedMultiplier)
    SetScenario    → factory_.reset()
                     auto cfg = scenarioLoader_.load(cmd.scenario)
                     factory_.applyConfig(cfg)
    ForceBreak     → factory_.forceBreak(cmd.targetMachineId)
    InstantRepair  → factory_.instantRepair(cmd.targetMachineId)
    Rewind         → auto snap = mementoStore_.rewind(cmd.rewindTargetTick)
                     factory_.restore(snap)
    ClearLog       → factory_.clearLog()
```

순수 dispatch. View와 Model을 알지만 둘의 데이터 흐름엔 개입 안 함.

### `main.cpp` 와이어링

```
1. SDL/ImGui init

2. 백엔드 부품 생성 (의존성 순서)
   EventBroker      broker
   EventLog         eventLog(broker)         // 생성자에서 subscribeAll
   Statistics       stats
   MementoStore     mementoStore
   TechnicianManager  techMgr(broker, factory)  // Fault subscribe
   Factory          factory(broker, eventLog, stats, techMgr)
   SimulationRunner runner(factory, broker, mementoStore)
   ScenarioLoader   loader
   Controller       ctrl(factory, runner, mementoStore, loader)
   View             view

3. 초기 시나리오 로드
   ctrl.dispatch({SetScenario, scenario=Normal})

4. 메인 루프
   while (!quit):
     - SDL poll / ImGui new frame
     - FactorySnap snap = factory.snapshot()
     - MachineCmd cmd = view.render(snap)        // View가 위젯 그리고 cmd 작성
     - if (cmd.action != None) ctrl.dispatch(cmd)
     - runner.tryAdvance(io.DeltaTime)
     - ImGui render
```

**순환 의존성**: 컴파일 단위로는 forward declaration으로 끊김. TechnicianManager → Factory는 setter 주입 (`techMgr.setFactory(&factory)`)으로 생성 순서 해결. 진짜 순환은 없음 — `Factory.technicianManager_` (참조), `TechnicianManager.factory_` (포인터/참조)는 lifetime 동일 (main 스코프 보장).

판단:
- snapshot 매 프레임 호출 부담 가능 — Runner가 매 틱 push한 최신 snap을 캐시 후 View가 그걸 읽도록 최적화 가능. 일단 단순 호출로 시작, 성능 이슈 시 캐시

### UML 마감

- 상속 트리 (Machine / Product / State 계층)
- 의존 관계 (Controller → Factory/Runner/Loader, Factory → 협력자, EventBroker ↔ 구독자)
- 시퀀스 다이어그램 1~2개:
  - 정상 틱 진행 (Spawner → Conveyor → Machine → Conveyor → ...)
  - Fault 발생 → TechnicianManager 큐 → Technician 배정 → Resume

### GitHub Pages

- README + UML 이미지 + 빌드/실행 가이드 정도면 요구사항의 "live 상태" 충족
- 별도 워크플로(`.github/workflows/pages.yml`)로 정적 배포

## 의존성

- Phase 1~6 전체

## 테스트

`tests/phase_7_controller.cpp` (mock Factory/Runner/Loader):
- 각 CmdAction → 해당 메서드 호출 검증 (Start → runner.start, SetScenario → factory.reset + loader.load + factory.applyConfig 순)
- Rewind → mementoStore.rewind + factory.restore
- None action은 noop

`tests/phase_7_e2e.cpp` (실제 객체로 짧은 시나리오):
- Normal 시나리오 로드 → 30틱 진행 → finished count > 0
- Breakdowns → breakdowns count > 0, EventLog에 fault 메시지 누적
- Rewind 후 동일 tick까지 재진행 → 동일 finished count (deterministic 검증)

## 산출 브랜치

`back/feat/main-integration` (UML / Pages는 별도 PR 가능)

## 후속

- snapshot 캐시 도입 (성능 측정 후 결정)
- View 측에서 추가 cmd 액션이 필요해지면 `CmdAction` 확장
