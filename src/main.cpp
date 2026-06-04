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

int main(int, char**)
{
    // ── SDL init ──────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::printf("SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

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

    // ── Phase 7 통합 지점: 도메인 객체 생성 ───────────────────
    EventBroker      broker;
    EventLog         eventLog{broker};
    Statistics       stats{broker};
    MementoStore     mementoStore;

    TempLookup       tempLookup;
    RepairDispatcher mgr{broker, tempLookup};
    Factory          factory{broker, eventLog, stats, mgr};
    mgr.setLookup(factory); // Factory가 생성된 후 진짜 Lookup으로 연결

    SimulationRunner runner{factory, broker, mementoStore};
    ScenarioLoader   loader;
    Controller       ctrl{factory, runner, mementoStore, loader};
    View             view;

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
        double dt = ImGui::GetIO().DeltaTime;
        runner.tryAdvance(dt);                             // 1. 공장 틱(가동) 진행
        const FactorySnap snap = factory.snapshot();       // 2. 백엔드에서 현재 상태 스냅샷 가져오기
        const MachineCmd  cmd  = view.render(snap);        // 3. View가 스냅샷을 보고 화면을 그리고, 명령 받아옴
        ctrl.dispatch(cmd);                                // 4. 받아온 명령을 백엔드로 전달

        // ── Render ────────────────────────────────────────────
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // ── Cleanup ───────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}