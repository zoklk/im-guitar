# Electric Guitar Factory Simulation 기획서

## 1. 개요

### 도메인

일렉기타 제조 공장. 원목과 전자부품을 입력받아 절단 → 도장 → 결합 → 전자부품 장착 → 포장 순으로 진행한다.

### 아키텍처

순수 MVC 구조를 따른다.

- **Model**: 시뮬레이션 상태와 로직. Factory, Machine, Product, Conveyor, Statistics, EventLog. MachineCmd 구조체를 모르며, 의미 단위 도메인 메서드(`start`, `pause`, `reset`, `setSpeed`, `setScenario`, `forceBreak`, `instantRepair`, `rewind` 등)만 노출한다
- **View**: ImGui 기반 UI 윈도우. 버튼 입력을 MachineCmd로 작성하고, FactorySnap을 읽어 렌더링한다
- **Controller**: View가 작성한 MachineCmd를 파싱(`dispatch`)해 Model의 적절한 도메인 메서드를 호출한다. View와 Model 사이의 얇은 계층

계층별 소통 수단:

- View ↔ Controller: MachineCmd
- Controller ↔ Model: 도메인 메서드 호출
- Model → View: FactorySnap

cmd 파싱과 디스패치는 Controller의 책임이며 Model로 넘어가지 않는다. Machine 클래스는 ImGui 헤더를 include하지 않는다.

## 2. 시나리오

4종. 누적 구조로 난이도가 단계적으로 증가한다.

| 시나리오 | 고장 확률 | 추가 효과 |
|---|---|---|
| Normal | 0% | 없음 |
| Random Breakdowns | 2% | 고장 발생 |
| Bottleneck | 2% | Painter 처리시간 12틱 |
| Overflow | 2% | WoodSpawner 주기 1틱으로 가속 |

- 고장 확률은 매 틱 각 머신의 체력을 1 감소시킬 확률이다.
- `Scenario::apply(Factory&)`가 시나리오 타입에 따라 머신 파라미터를 일괄 주입한다.
- 시나리오 변경 시 Reset과 동일하게 전체 초기화한다.

## 3. 클래스 구조

```
Factory (컨테이너 / 조정자)

[상속 트리 1: 능동 객체]
SimulationObject (추상)
├── Machine (추상)
│   ├── Cutter (추상) → HeadCutter, NeckCutter, BodyCutter
│   ├── Painter
│   ├── Assembler (추상) → BodyAssembler, PartAssembler
│   ├── ElecPartCollector
│   └── Packager
├── Spawner (추상)
│   ├── WoodSpawner
│   └── ElecPartSpawner
├── Worker (추상)
│   └── Technician
└── Conveyor

[상속 트리 2: 수동 객체]
Product (추상)
├── RawWood
├── GuitarPart (추상) → HeadPart, NeckPart, BodyPart (isPainted 플래그)
├── ElecPart (추상) → Bridge, Pickup
├── ElecPartSet
├── AssembledBody
└── FinishedGuitar

[보조 클래스]
EventLog, Statistics, Scenario
Controller (cmd 파싱 → Model 도메인 메서드 호출)
MachineCmd (View→Controller), FactorySnap (Model→View)
```

## 4. 클래스 책임

- **Factory**: 모든 객체 소유, 매 틱 update 호출, BROKEN 머신을 IDLE Technician에 디스패치, 파이프라인 셋업, FactorySnap 생성 및 메멘토용 누적. 의미 단위 도메인 메서드(start/pause/reset/setSpeed/setScenario/forceBreak/instantRepair/rewind)를 노출하며 MachineCmd 구조체에는 의존하지 않는다
- **Controller**: View가 작성한 MachineCmd를 파싱(`dispatch`)해 Factory의 도메인 메서드를 호출하는 얇은 계층
- **SimulationObject**: 공통 인터페이스 (`update(tick)`, `getInfo()`, `getId()`). EventLog 참조 보유. 디스패치용 가상함수 (`needsRepair()`, `assignWorker(...)`)
- **Machine**: 상태(IDLE/WORKING/BROKEN), 체력(0~10), 진행도, 처리시간, 고장확률, 입출력 큐 참조. 자식이 `process()` 구현. BROKEN 전이 시 자기 로그
- **Cutter 자식들**: RawWood를 HeadPart/NeckPart/BodyPart로 가공
- **Painter**: BodyPart에 도장 (isPainted = true)
- **Assembler**: 다중 입력 큐 동기화 공통 로직
- **BodyAssembler**: head + neck + paintedBody 결합 → AssembledBody
- **PartAssembler**: AssembledBody + ElecPartSet 결합 → FinishedGuitar
- **ElecPartCollector**: 종류별 입력 큐(Bridge, Pickup)에서 하나씩 모아 ElecPartSet 생성
- **Packager**: 완제품 포장 후 출고
- **WoodSpawner**: 일정 주기로 RawWood를 3개 Cutter에 라운드 로빈 push
- **ElecPartSpawner**: 일정 주기로 Bridge, Pickup을 각 종류 큐에 동시에 하나씩 push
- **Technician**: 상태(WAITING/MOVING/REPAIRING). dispatch(Machine\*)로 작업 받음. 일정 틱 후 머신 수리. 자기 활동 로그. 총 2명
- **Conveyor**: 내부 큐와 capacity. 가득 차면 push 실패 → 호출자가 lost 카운트
- **Product 자식들**: id, 종류 정보만 보유하는 데이터 객체. BodyPart만 isPainted 상태 보유
- **EventLog**: LogEntry 벡터, max size 200, add/clear/get
- **Statistics**: finished, wip, breakdowns, lost 카운터
- **Scenario**: 시나리오 타입과 `apply(Factory&)` 메서드로 머신 파라미터 일괄 주입

## 5. 시뮬레이션 파라미터

| 항목 | 값 |
|---|---|
| 컨베이어 capacity | 5 (전 구간 동일) |
| 머신 기본 처리시간 | 6틱 |
| Painter 처리시간 (Bottleneck) | 12틱 |
| WoodSpawner 주기 | 2틱 (Overflow 시 1틱) |
| ElecPartSpawner 주기 | 6틱 (Bridge·Pickup 동시 push) |
| 머신 체력 | 0~10 정수 |
| 고장 확률 | 시나리오별 (2절 표) |
| Technician 수 | 2명 |
| 수리 소요 시간 | 3틱 |
| 틱 주기 | 600ms당 1틱 (1배속) |
| 속도 배수 | 1~5배 (5배 시 120ms당 1틱) |
| EventLog max size | 200 |
| EventLog 화면 표시 | 10개 (스크롤 가능) |
| 머신 인스턴스 | 각 종류 1대 |

## 6. 핵심 정책

- **체력**: 0~10 정수. 매 틱 시나리오별 확률로 1 감소. 0이 되면 BROKEN
- **고장 시 동작**: 작업 중이었다면 진행도와 현재 처리 중인 제품을 유지한 채 멈춤. 수리 완료 후 멈춘 지점부터 재개
- **수리**: Technician 2명, 각 수리 소요 3틱. 완료 시 체력 10으로 복원, 상태 IDLE로 복귀
- **고장 디스패치**: Factory가 매 틱 BROKEN 머신과 IDLE Technician을 매칭. Technician은 머신 목록을 모름
- **오버플로우**: 컨베이어가 가득 차면 새 제품 드롭, lost 카운트 증가
- **Spawner**: 일정 주기로 push. 큐가 모두 가득 차면 스킵
- **속도 배수**: 1~5배. 경과 시간 누적이 (600ms / 배수)를 넘을 때마다 1틱 진행
- **ID 체계**: Machine ID는 고정 문자열. Product ID는 전역 카운터로 단일 증가
- **Force Break**: 일반 고장과 동일 로직(진행도·제품 유지). 체력을 0으로 설정해 BROKEN 전이
- **Instant Repair**: Technician 디스패치를 거치지 않고 즉시 체력 10 복구, IDLE 복귀
- **Reset / 시나리오 변경**: 틱 0 초기화, 머신 상태 초기화, EventLog 초기화, 메멘토 스냅 초기화

## 7. 이벤트 로깅 정책

EventLog는 Factory가 소유한다. 각 SimulationObject는 생성 시 EventLog 참조를 주입받아 직접 기록한다. **Factory 자체는 로그를 찍지 않는다.**

**객체별 로그 항목**

- Spawner: 원자재/전자부품 생성
- Cutter/Painter/Assembler/Packager: 작업 시작, 작업 완료
- ElecPartCollector: 부품 세트 완성
- Machine: BROKEN 전이 시
- Technician: 수리 시작, 수리 완료
- Conveyor: 오버플로우(드롭) 발생만 기록. 정상 push/pop은 생략

시나리오 변경 시 EventLog 전체 초기화 (별도 로그 없음). max size 200 초과 시 가장 오래된 로그부터 드롭.

## 8. 파이프라인 토폴로지

```
[WoodSpawner]
  └→ HeadCutter ──────────────────┐
  └→ NeckCutter ──────────────────┼→ BodyAssembler ─┐
  └→ BodyCutter → Painter ────────┘                  │
                                                       ↓
                                                  PartAssembler → Packager → FinishedGuitar
                                                       ↑
[ElecPartSpawner]                                     │
  └→ Bridge, Pickup → ElecPartCollector ─────────────┘
```

머신 사이는 모두 Conveyor로 연결. WoodSpawner는 3개 Cutter에 라운드 로빈 분배, ElecPartSpawner는 ElecPartCollector의 종류별 입력 큐(Bridge용, Pickup용)에 동시 분배.

## 9. 데이터 흐름

```
View (ImGui 위젯) → 버튼 입력으로 MachineCmd 작성
Controller.dispatch(cmd) → cmd.action 파싱 → Factory의 도메인 메서드 호출
Factory → 모든 객체 update + BROKEN 머신 디스패치
객체들 → 자기 활동을 EventLog에 직접 기록
Factory → FactorySnap 생성 (메멘토용으로 누적 보관)
View → FactorySnap 읽어 렌더링
```

cmd 파싱은 Controller가 담당하고, Factory는 MachineCmd 구조체를 알지 못한다.

### MachineCmd (View → Controller)

동작 기반 경량 구조. 객체의 상태 정보를 담지 않는다.

```cpp
enum class CmdAction {
    None, Start, Pause, Reset,
    SetSpeed, SetScenario,
    ForceBreak, InstantRepair, Rewind,
    ClearLog
};

struct MachineCmd {
    CmdAction    action          = CmdAction::None;
    std::string  targetMachineId;          // ForceBreak / InstantRepair
    int          speedMultiplier = 1;      // SetSpeed (1~5)
    ScenarioType scenario;                 // SetScenario
    int          rewindTargetTick = 0;     // Rewind
};
```

Controller의 `dispatch`는 `action`을 분기해 Factory의 도메인 메서드(`setSpeed`, `setScenario`, `start`, `pause`, `reset`, `forceBreak`, `instantRepair`, `rewind`, `clearLog`)를 호출한다.

### FactorySnap (Model → View)

모든 객체의 상태 정보를 담는다. 메멘토 복원이 가능한 수준으로 두텁게 정의하며, UI는 이 중 렌더링에 필요한 값만 선택해 사용한다. 진행 카운터(progress, spawnCounter, repairProgress)를 포함해야 정확한 되감기가 보장된다.

```cpp
struct ProductSnap {
    int         id;
    ProductType type;
    bool        isPainted = false;   // BodyPart만 유효
};

struct MachineSnap {
    std::string  id;
    MachineState state;              // IDLE / WORKING / BROKEN
    int          health;             // 0~10
    int          progress;           // 현재 진행 틱
    int          processingTime;
    double       breakdownProb;
    int          outputCount;
    std::optional<ProductSnap> currentProduct;   // 처리 중인 제품
};

struct ConveyorSnap {
    std::string              id;
    int                      capacity;
    std::vector<ProductSnap> items;
};

struct SpawnerSnap {
    std::string id;
    int         period;
    int         spawnCounter;        // 다음 생성까지
    int         roundRobinIndex;
};

struct TechnicianSnap {
    std::string      state;          // WAITING / MOVING / REPAIRING
    std::string      id;
    std::string      targetMachineId;   // 없으면 빈 문자열
    int              repairProgress;
};

struct StatisticsSnap {
    int finished;       // 출고 누적
    int wip;            // 현재 공정 중 제품 수
    int breakdowns;     // 고장 누적
    int lost;           // 오버플로우 소실 누적
};

struct FactorySnap {
    int          tick;
    ScenarioType scenario;
    int          speedMultiplier;
    bool         running;
    std::vector<MachineSnap>    machines;
    std::vector<ConveyorSnap>   conveyors;
    std::vector<SpawnerSnap>    spawners;
    std::vector<TechnicianSnap> technicians;
    StatisticsSnap              stats;
    std::vector<LogEntry>       logs;
};
```

## 10. 메멘토 (되감기)

- 매 틱 생성되는 FactorySnap을 vector에 누적한다.
- Rewind 명령 시 목표 틱의 Snap으로 Model을 복원(`restore`)하고, 그 이후의 Snap은 전부 폐기한다.
- 복원 후 그 시점부터 시뮬레이션을 재개한다. redo는 지원하지 않는다.
- Reset 시 누적 Snap도 초기화한다.

## 11. UI 윈도우

- **Simulation Control**: Start/Pause/Reset, 속도 슬라이더, 시나리오 드롭다운, 틱 카운터
- **Factory Floor**: 머신 시각화, 상태별 색상, 컨베이어 적재량 ProgressBar, Selectable 클릭으로 머신 선택, Technician 위치 표시
- **Inspector**: 선택된 머신의 state, 체력 바, 큐 깊이, 출력 카운트, 처리시간, Force Break / Instant Repair 버튼
- **Event Log**: 스크롤 가능한 타임스탬프 목록(화면 10개), Clear 버튼
- **Statistics**: finished, wip, breakdowns, lost

## 12. 빌드

- C++17 기준, CMake 사용
- 의존성: ImGui 및 렌더 백엔드

## 13. 잔여 확정 항목

- C++ 표준 버전 최종 합의 (C++17 / C++20)
- Technician 대기 공간 좌표 등 UI 레이아웃 세부
