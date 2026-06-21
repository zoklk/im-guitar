// im-guitar — Electric Guitar Factory Simulation
//
// 현재 상태: Phase 7 통합 완료
// Factory / Controller / View 객체를 연결하고 메인 루프에
// (cmd 수집 → dispatch → tickIfDue → snapshot → render) 흐름을 채운다.

#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>

// ── 백엔드 및 통합 뷰 헤더 ──
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/factory/Factory.h"
#include "model/factory/SimulationRunner.h"
#include "model/machine/IMachineLookup.h"
#include "model/memento/MementoStore.h"
#include "model/scenario/ScenarioLoader.h"
#include "model/repair_dispatcher/RepairDispatcher.h"
#include "model/stats/Statistics.h"
#include "controller/Controller.h"
#include "view/View.h"

// 초기화를 돕기 위한 임시 Lookup 클래스
class TempLookup : public IMachineLookup {
public:
    Machine* findMachine(const std::string&) override { return nullptr; }
};

// 루프에서 사용할 변수들을 구조체로 묶어서 관리
struct AppContext {
    EventBroker      broker;
    EventLog         eventLog;
    Statistics       stats;
    MementoStore     mementoStore;
    TempLookup       tempLookup;
    RepairDispatcher mgr;
    Factory          factory;
    SimulationRunner runner;
    ScenarioLoader   loader;
    Controller       ctrl;
    View             view;

    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    bool running = true;

    // 초기화 (참조 변수들이 꼬이지 않게 생성자 리스트 사용)
    AppContext() 
        : eventLog(broker), stats(broker), mgr(broker, tempLookup),
          factory(broker, eventLog, stats, mgr),
          runner(factory, broker, mementoStore),
          ctrl(factory, runner, mementoStore, loader) 
    {
        mgr.setLookup(factory);
    }
};

// 매 프레임마다 실행될 1회용 함수
void MainLoopStep(void* arg) {
    AppContext* app = (AppContext*)arg;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) app->running = false;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    double dt = ImGui::GetIO().DeltaTime;
    app->runner.tryAdvance(dt);
    const FactorySnap snap = app->factory.snapshot();
    const MachineCmd  cmd  = app->view.render(snap);
    app->ctrl.dispatch(cmd);

    ImGui::Render();
    int w, h;
    SDL_GetWindowSize(app->window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(app->window);
}

int main(int, char**)
{
    // ── SDL init ──────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::printf("SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    // 데스크톱 그래픽 세팅
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_Window* window = SDL_CreateWindow(
        "Electric Guitar Factory",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);  // vsync

    // ── ImGui init ────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    
    ImGui_ImplOpenGL3_Init("#version 130");

    // Context 하나만 동적 할당하여 생성
    AppContext* app = new AppContext();
    app->window = window;
    app->gl_context = gl_context;

<<<<<<< HEAD
    // [UI 테스트용] 더미 데이터 및 패널 객체 생성
    FactoryFloorPanel factoryFloor;
    FactorySnap dummySnap;
    
    std::vector<std::string> mIds = {
        "SPN_WOOD_BODY", "SPN_WOOD_NECK", "SPN_WOOD_HEAD", "MCH_BODY_CUT", "MCH_NECK_CUT", 
        "MCH_HEAD_CUT", "MCH_PAINT", "MCH_BODY_ASM", "SPN_PICKUP", "SPN_BRIDGE", 
        "MCH_ELEC", "MCH_PART_ASM", "MCH_PACK"
    };
    for (const auto& id : mIds) {
        MachineSnap m; m.id = id; m.health = 10;
        if (id == "MCH_PAINT") m.health = 0; 
        if (id == "MCH_BODY_CUT" || id == "MCH_BODY_ASM") m.currentProduct.push_back({});
        dummySnap.machines.push_back(m);
    }

    std::vector<std::string> cIds = {
        "CONV_WOOD_BODY", "CONV_WOOD_NECK", "CONV_WOOD_HEAD", "CONV_BODY_RAW",
        "CONV_BODY_PAINTED", "CONV_NECK", "CONV_HEAD", "CONV_PICKUP",
        "CONV_BRIDGE", "CONV_ASMBODY", "CONV_ELEC", "CONV_GUITAR"
    };
    for (int i = 0; i < cIds.size(); ++i) {
        ConveyorSnap c; c.id = cIds[i]; c.length = 5; c.slots.resize(5);
        c.slots[i % 5] = ProductSnap{}; 
        if (i % 2 == 0) c.slots[(i + 2) % 5] = ProductSnap{}; 
        dummySnap.conveyors.push_back(c);
    }

    TechnicianSnap t1; t1.id = "Jincheol"; t1.targetMachineId = "MCH_PAINT";
    TechnicianSnap t2; t2.id = "Jaeyong";
    dummySnap.technicians = {t1, t2};
    // ── UI 테스트용 ───────────────────────────────────────────

    // ── Main loop ─────────────────────────────────────────────
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ── Phase 7 통합 지점: 한 프레임의 흐름 ───────────────
        // factory.tickIfDue();                       // 600ms / speedMult 마다 1틱
        // const FactorySnap snap = factory.snapshot();
        // const MachineCmd  cmd  = view.render(snap);  // 위젯에서 cmd 작성
        // controller.dispatch(cmd);

        // ── UI 윈도우 5개 (필수요건 4절) ──────────────────────
        ImGui::Begin("Simulation Control");
        // TODO: Start / Pause / Reset 버튼, 속도 슬라이더(1~5), 시나리오 드롭다운, 틱 카운터
        ImGui::Text("placeholder");
        ImGui::End();

        ImGui::Begin("Factory Floor");
        // TODO: 머신 시각 맵, 상태별 색상(TextColored), Selectable 머신 항목,
        //       Conveyor 적재량 ProgressBar, Technician 위치 표시
        factoryFloor.render(dummySnap, nullptr);
        // ImGui::Text("placeholder");
        ImGui::End();

        ImGui::Begin("Inspector");
        // TODO: 선택된 머신의 state, health bar(ProgressBar), queue depth,
        //       output count, processingTime, Force Break / Instant Repair 버튼
        ImGui::Text("placeholder");
        ImGui::End();

        ImGui::Begin("Event Log");
        // TODO: BeginChild로 스크롤 가능한 로그 목록(화면 10개), Clear 버튼
        ImGui::Text("placeholder");
        ImGui::End();

        ImGui::Begin("Statistics");
        // TODO: finished / wip / breakdowns / lost
        ImGui::Text("placeholder");
        ImGui::End();

        // ── Render ────────────────────────────────────────────
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
=======
    // 데스크톱용 무한 루프
    while (app->running) {
        MainLoopStep(app);
>>>>>>> e45f6f6efe8e8e0187b65db4835850ce90aa6766
    }

    // ── Cleanup ───────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    delete app;
    return 0;
}