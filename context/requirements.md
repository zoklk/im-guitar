# 프로젝트 필수 요건 (Desired State)

EC5209 OOP C++ Factory Simulation 공지에서 **반드시 충족해야 하는 조건**만 추출한 선언적 명세. 각 항목은 최종 결과물이 만족해야 하는 검증 가능한 상태다. 예시(example), 제안(suggestion), 선택(optional) 항목은 제외했다.

## 1. 설계 (OOP)

- [ ] 모든 시뮬레이션 객체는 단일 추상 base 클래스를 상속한다
- [ ] main loop는 객체를 base 포인터로 저장하며, concrete type에 대한 if/else 분기 없이 메서드를 호출한다
- [ ] 새로운 machine type을 추가할 때 simulation loop와 UI rendering loop를 수정하지 않는다 (수정이 필요하면 설계 미완)
- [ ] 모든 시뮬레이션 클래스의 모든 필드는 private 또는 protected다 (public data member 0개)
- [ ] UML(ER) 다이어그램이 위 설계를 반영하고 정당화한다

## 2. UI / Backend 분리

- [ ] UI와 backend가 분리되어 있다
- [ ] UI와 backend 양쪽을 동시에 참조하는 파일은 단 하나뿐이다 (예: main.cpp)
- [ ] backend 코드는 ImGui 헤더를 include하지 않는다
- [ ] `update(tick)`은 어떤 ImGui 함수도 호출하지 않는다

## 3. 시뮬레이션 기능

- [ ] 런타임에 ImGui 드롭다운으로 시나리오를 선택할 수 있다
- [ ] 최소 2종 시나리오(Normal flow, Random Breakdowns)를 지원한다
- [ ] 시뮬레이션 속도를 1배~5배로 조절할 수 있다
- [ ] Random Breakdowns 시나리오에서 머신 고장이 발생하고 technician이 자동 디스패치된다

## 4. UI 윈도우 (5개 필수)

- [ ] **Simulation Control**: Start / Pause / Reset 버튼, 속도 슬라이더(1~5배), 시나리오 드롭다운, 실시간 틱 카운터
- [ ] **Factory Floor**: 머신 시각 맵, 상태별 색상 구분
- [ ] **Inspector**: 선택된 머신의 state, health bar, queue depth, output count, process time, Force Break / Instant Repair 버튼
- [ ] **Event Log**: 스크롤 가능한 타임스탬프 이벤트 목록, Clear 버튼
- [ ] **Statistics**: finished goods, WIP, total breakdowns, lost products 누계

## 5. 필수 ImGui 위젯

- [ ] `ImGui::Button` (Start, Pause, Reset, Force Break, Instant Repair, Clear Log)
- [ ] `ImGui::SliderInt` (시뮬레이션 속도)
- [ ] `ImGui::Combo` 또는 `ImGui::ListBox` (시나리오 선택)
- [ ] `ImGui::ProgressBar` (머신 health, 머신 progress, 컨베이어 적재량)
- [ ] `ImGui::TextColored` (상태별 색상 라벨)
- [ ] `ImGui::BeginChild` / `EndChild` (스크롤 이벤트 로그)
- [ ] `ImGui::Selectable` (Factory Floor의 클릭 가능한 머신 항목)

## 6. 제출물

- [ ] 팀 GitHub 저장소 URL 제출
- [ ] `src/` — 모든 C++ 소스 파일과 헤더 포함
- [ ] 빌드 파일 포함 (`CMakeLists.txt` / `Tasks.json` / `Makefile` 중 하나)
- [ ] UML(ER) 다이어그램 포함 (손으로 그린 것도 허용)
- [ ] GitHub Pages 게시 (live 상태)

## 7. 일정

- [ ] **M1 (5/19~23)**: UML과 기본 코드 구조, UI 요소 준비 완료
- [ ] **최종 (6/6 23:59 KST)**: 전체 제출물 제출 완료

## 8. 협업 / 학술 무결성

- [ ] 모든 팀원이 의미 있는(meaningful) 커밋을 보유한다
- [ ] AI가 전적으로 작성한 코드를 제출하지 않는다
- [ ] 타 팀 저장소를 복사하지 않는다
- [ ] commit history를 왜곡하지 않는다
- [ ] 발표 주간에 팀원 누구나 코드의 어느 부분이든 설명할 수 있다
