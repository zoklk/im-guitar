# Phase 1 — 이벤트 인프라 & 데이터

## 목표

상위 의존성 없는 leaf 컴포넌트 4종. 병렬 작업 가능.

## 적용 패턴

- **Observer**: `EventBroker` — 발행자/수신자 결합도 0
- **Topic-per-source 구독**: `(EventType, sourceId)` 키로 특정 발신원 한정 구독
- **만능 구독자**: `EventLog`는 `subscribeAll`로 모든 EventType 자동 수신

### push 디스패치 (vs pull 검토)

EventBroker는 **push 방식** — `flush()` 시점에 broker가 구독자의 `handle()`을 직접 호출:

- 결합도: broker는 `IEventHandler*`만 알고 구체 클래스는 모름 (인터페이스로 차단됨). push 자체가 결합도를 높이는 게 아님
- 동기성: 단일 스레드 sim에서 `flush()`는 tick 종료 시 일괄 호출되는 자연 동기점. 비동기 필요 없음
- pull 대안 (구독자가 broker.poll()로 본인 관심 이벤트 가져감) 불채택 — 구독자마다 커서 위치 관리 / 중복 방지 / 미수신 보장 로직이 필요해져 코드 늘어남. 분산 시스템 아닌 이상 push가 표준

## 구성요소

### `EventBroker`

```
subscribe(EventType, IEventHandler*)              // 타입 전체 구독
subscribe(EventType, sourceId, IEventHandler*)    // 토픽 (type, source) 한정 구독
subscribeAll(IEventHandler*)                       // 와일드카드 (EventLog 전용)
publish(Event)                                     // 큐에 적재, 즉시 호출 X
flush()                                            // 큐 비우며 매칭 구독자 handle() 호출
clearQueue()                                       // 미디스패치 잔량 폐기 (Reset cmd용)
queueSize() const                                  // 큐 잔량 (테스트/검증용)
```

내부 자료:
- `typeSubs_:   unordered_map<EventType, vector<IEventHandler*>>`
- `topicSubs_:  unordered_map<EventType, unordered_map<string, vector<IEventHandler*>>>` (type → sourceId → handlers)
- `globalSubs_: vector<IEventHandler*>`
- `eventQueue_: queue<Event>`

dispatch 순서 (한 이벤트 당): globalSubs → typeSubs[type] → topicSubs[type][sourceId]. 중복 등록 시 중복 호출 가능성 — 구독자 측에서 idempotent하게 작성.

publish는 즉시 디스패치 X, 매 틱 끝에 SimulationRunner가 `flush()` 호출해 일괄 소비.

**flush 루프 정책**: 단순 `while (!queue.empty())` 루프. handler가 flush 도중 publish하면 같은 flush 사이클에서 연쇄 처리됨. 단, 정상 설계에선 handler는 상태 mutation만 하고 publish는 다음 update()로 미룸 (Backpressure cascade도 다음 틱 Conveyor.update에서 자연 전파). 재진입 publish가 정말 필요한 케이스가 미래에 발생하면 그때 swap 패턴 도입.

### `Product` 계층

데이터 객체 위주, 책임 최소 (`id`, `getType()`). 소유권은 `std::unique_ptr<Product>` 체인.

```
Product (추상)
├── RawWood
├── HeadPart / NeckPart / BodyPart (isPainted)
├── Bridge / Pickup / ElecPartSet
├── AssembledBody
└── FinishedGuitar
```

전역 ID는 `ProductIdGen` (atomic counter) — Product에 static 두는 것보다 SRP 측면에서 분리. **Factory가 1개 소유, Spawner 생성 시 `idGen_&` 참조 주입** (`rng_` / `statistics_` / `broker_`와 동일한 DI 패턴). Spawner.process()에서 `id = idGen_.next()`. 글로벌 정적 싱글톤 (`ProductIdGen::next()`) 안 씀 — 테스트 격리 / 메멘토 직렬화 / 의존성 그래프 일관성 위해.

API: `next()` (전위 증가 후 반환), `peek() const` (현재값 조회), `setCounter(int)` (메멘토 복원용 강제 설정).

카운터 값은 `FactorySnap.productIdCounter`로 직렬화 → rewind 후 `setCounter()`로 복원해 ID 단조 증가 보존 (Phase 0의 FactorySnap에 필드 추가됨).

소유 흐름 명세:
- Spawner: `auto p = std::make_unique<RawWood>();` → `outputConveyor->push(std::move(p))`
- Conveyor.push: 입구 슬롯 `unique_ptr<Product>` 보유. 매 틱 시프트는 `slots_[i+1] = std::move(slots_[i])` 형태
- Conveyor → Machine: `downstream->acceptProduct(std::move(out_slot))`. Machine은 inputBuffer (또는 typedBuffer)에 `unique_ptr<Product>` 적재
- Machine.process(): inputs를 currentProduct에서 소비 후 새 `unique_ptr<Product>` 생성 → outputConveyor로 move. 입력 unique_ptr은 함수 종료 시 자동 소멸
- Packager.process(): FinishedGuitar unique_ptr이 함수 스코프에서 소멸 (출고 = drop)
- Conveyor drop: 슬롯 unique_ptr을 reset() → 자동 소멸

### `Statistics`

단순 카운터 4종: `finished` / `wip` / `breakdowns` / `lost`. getter + incrementer + `decWip()` + `reset()`. 호출자:
- `finished`: Packager가 출고 시 `incFinished()`
- `wip`: Spawner가 생성 시 `incWip()`, Packager가 출고 시 `decWip()`, Conveyor.dropAndLog 시 `decWip()` (drop된 제품도 공정에서 제거되므로)
- `breakdowns`: BrokenState.onEnter `incBreakdowns()`
- `lost`: Conveyor.dropAndLog `incLost()`
- `reset()`: Reset cmd 경로에서 호출, 4 카운터 모두 0으로

> wip는 "현재 공정 중인 제품 수" 정의. 따라서 drop 시에도 decWip를 같이 호출해야 일관. `dropAndLog`가 단일 호출 지점이므로 거기서 묶어서 처리

### `EventLog`

- `IEventHandler` 구현. 생성자에서 `broker.subscribeAll(this)` 호출
- 내부 `deque<LogEntry>`, `static constexpr size_t kMaxEntries = 200`, FIFO drop
- `handle(Event)`이 Event → `"[<type-name>] <sourceId>"` 텍스트 변환 후 push. EventType→string 헬퍼는 `EventLog.cpp` 익명 namespace 내부 (외부 비노출)
- `getLogs()` getter — `deque → vector` 복사 반환. Factory.snapshot()이 호출해 `FactorySnap.logs`에 복사
- `size()` getter — 테스트/검증용
- `clear()` — ClearLog cmd 처리용
- `appendDirect(LogEntry)` — Conveyor.dropAndLog 같은 EventBroker 안 거치는 케이스 대응

## 파일 배치

```
src/model/
├── event/    EventBroker.{h,cpp}  EventLog.{h,cpp}
├── product/  Product.h (추상 + 9종 derived 1파일)  ProductIdGen.h
└── stats/    Statistics.h
```

Product derived 9종은 데이터 거의 없어 한 헤더에 모음. Statistics / ProductIdGen은 inline 가능해서 헤더 only.

## 빌드 인프라

CMake에 `model_lib` static library 추가 (`src/model/*.cpp` GLOB_RECURSE). PUBLIC include 경로로 model/event/product/stats 노출. `app`과 `unit_tests` 양쪽이 model_lib에 링크. 후속 Phase 2~에서 추가되는 모든 `src/model/**/*.cpp`는 자동으로 model_lib에 흡수되므로 별도 CMake 와이어링 불필요.

## 의존성

- 상위 의존: 없음 (모두 leaf)
- 하위에서 의존: Phase 2~ 전부

## 테스트

`tests/phase_1_event_broker.cpp` / `tests/phase_1_product.cpp` / `tests/phase_1_log_stats.cpp`:

- **EventBroker**: subscribe/publish/flush 라운드트립. type 구독만 등록 후 다른 type 이벤트 publish → handle 호출 안 됨. 토픽 (type, sourceA) 구독 → sourceB 이벤트 publish → 호출 안 됨, sourceA 이벤트 publish → 호출됨. subscribeAll은 모든 type 수신. dispatch 순서 검증 (mock handler call order).
- **Product**: 9종 인스턴스화, `ProductIdGen` 인스턴스 1개에 `next()` 연속 호출 시 ID 단조 +1, 별도 인스턴스는 카운터 독립 (테스트 격리 검증), BodyPart의 isPainted 토글, getType() 반환값.
- **Statistics**: 4개 카운터 증감, getter 반환값.
- **EventLog**: max 200 enforcement (201개 push → 가장 오래된 1개 drop), broker subscribeAll 후 publish 시 handle 호출되어 log 누적, clear() 후 size 0, appendDirect 정상 작동.

## 산출 브랜치

`back/feat/event-data`

## 후속

- Event payload 타입 확정 → Phase 5에서 페이로드 목록 확정 후 결정
