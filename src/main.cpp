// im-guitar — Electric Guitar Factory Simulation
//
// 현재 상태: ImGui 부트스트랩 + 5개 윈도우 placeholder.
// Phase 5에서 Factory / Controller / View 객체를 연결하고 메인 루프에
// (cmd 수집 → dispatch → tickIfDue → snapshot → render) 흐름을 채운다.

#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>

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

    // ── Phase 5 통합 지점: 도메인 객체 생성 ───────────────────
    // Factory    factory;
    // Controller controller(factory);
    // View       view;
    // factory.start();

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

        // ── Phase 5 통합 지점: 한 프레임의 흐름 ───────────────
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
        ImGui::Text("placeholder");
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
