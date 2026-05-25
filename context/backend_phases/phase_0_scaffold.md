# Phase 0 — Scaffold 갱신

## 목표

구설계 기반의 공통 헤더를 design-update 기준으로 정리. 백/프론트 병렬 작업의 인터페이스 베이스 확정.

## 변경 사항

### `src/common/Types.h`

- **삭제**: `MachineState`, `TechnicianState` enum — State 패턴 객체로 대체, snap은 데이터에서 derive
- **삭제**: `CutterMachineType` — 라운드 로빈 폐지로 불필요
- **갱신**: `MachineType` 11종 (`WoodSpawner`, `BridgeSpawner`, `PickupSpawner`, `HeadCutter`, `NeckCutter`, `BodyCutter`, `Painter`, `ElecPartCollector`, `BodyAssembler`, `PartAssembler`, `Packager`). 같은 클래스의 라인 구분은 인스턴스 ID로
- **추가**: `EventType` 8종 — `Fault`, `Resume`, `Started`, `Completed`, `Spawned`, `Packaged`, `Drop`, `Backpressure` (회계용 3종 `Spawned`/`Packaged`/`Drop`은 Phase 3 WIP 회계 결정과 함께 확정)
- **추가**: `OverflowMode` (`Drop`, `Backpressure`) — Machine이 outputOverflowMode_로 보유, 시나리오별 동작 분기
- **갱신**: `ScenarioType` (`Normal`, `Breakdowns`, `Bottleneck`, `Overflow`, `SmartFactory`)
- **유지**: `ProductType` 9종, `CmdAction`

### `src/common/Event.h` (신규)

```
struct Event {
    EventType                  type;
    std::string                sourceId;
    int                        tick = 0;
    std::optional<int>         productId;
    std::optional<ProductType> productType;
};
class IEventHandler { virtual void handle(const Event&) = 0; };
```

- EventBroker가 구독자를 `IEventHandler*`로만 보관 → 구체 구독자 클래스에 broker 비의존 (Observer)
- payload는 `void*` 대신 **`optional<int>` + `optional<ProductType>` 2필드 조합**. 이유: void*는 type-safe 아니고 수명 관리 불가 + 메멘토 직렬화 안 됨. Phase 3 WIP 회계 결정과 함께 확정 (variant 도입은 불필요 — optional 2개로 충분)
- `sourceId`는 string. 토픽 구독 (Phase 1) 키로도 사용

### `src/common/MachineCmd.h`

기존 유지. 머신 동적 추가/제거 액션은 현재 불필요 (시나리오 변경 = 전체 리셋 후 재생성).

### `src/common/LogEntry.h`

기존 유지. `{ tick, sourceId, message }`.

### `src/common/FactorySnap.h`

모든 snap에서 state 필드 제거. derive 규칙:

- Machine: `health == 0` → Broken / `!currentProduct.empty()` → Processing / else → Idle
- Technician: `targetMachineId.has_value()` → Working / else → Idle

구조 변경:

- `MachineSnap`: `type` 추가, `inputBuffer` / `currentProduct` 추가 (vector<ProductSnap>), `outputOverflowMode` 추가, `suspended: bool` 추가 (Backpressure 모드에서 outputConveyor 포화 시 true), `assignedTechId: optional<string>` 추가 (snap 생성 시 Technician 목록 훑어 채우는 derive 필드)
- `ConveyorSnap`: 큐 모델 → **슬롯 배열** (`vector<optional<ProductSnap>>`), `downstreamId` 명시. (overflowMode는 Machine.outputOverflowMode에 들어가므로 ConveyorSnap에는 두지 않음)
- `SpawnerSnap` 삭제 — Spawner는 Machine이므로 `MachineSnap`에 흡수
- `TechnicianSnap`: `targetMachineId: optional<string>`, `repairProgress`, `repairTime`
- `FactorySnap`: `pendingEvents: vector<Event>` 추가 (메멘토 정확도), `logs: vector<LogEntry>` 유지, `rngState: string` 추가, `productIdCounter: int` 추가 (ProductIdGen 상태 — rewind 후 ID 단조성 보존용), `pendingRepairs: vector<RepairOrderSnap>` 추가 (TechnicianManager 큐 직렬화 — Phase 5 결정), `speedMultiplier` / `running` 추가 (Runner 상태 전시용, restore는 무시)

> 참고: snap은 raw 값만 들고, 소유권은 unique_ptr이라도 snap엔 복사된 `ProductSnap` 값으로만 들어감.

### `CMakeLists.txt`

- `nlohmann/json` `FetchContent` (URL 방식, 헤더 온리) 추가
- `googletest` `FetchContent` 추가 + `enable_testing()` + `tests/` 디렉토리 (placeholder만 우선)

## 적용 패턴

- **Observer**: `IEventHandler` + EventBroker 토대 마련 (실제 구현은 Phase 1)

## 테스트

Phase 0 코드는 데이터 정의 위주라 로직 테스트 거의 없음. 다음만 확인:

- `tests/phase_0_compile.cpp` — 모든 `common/*.h`를 include하고 각 struct 기본 생성 + 필드 대입이 가능한지 컴파일 단계에서 검증 (`static_assert(std::is_default_constructible_v<FactorySnap>)` 등)
- 빌드 자체가 통과하는 것이 1차 검증

## 검증

- `cmake --build build` 통과
- `main.cpp`는 placeholder 유지, 런타임 동작 변화 없음
- frontend가 mock `FactorySnap` 생성 가능

## 산출 브랜치

`back/chore/scaffold-update` (단독 머지) — Phase 1 시작 전 베이스 정립

## 후속 이관

- `MachineCmd` 동적 액션 → 보류
