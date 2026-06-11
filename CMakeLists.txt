cmake_minimum_required(VERSION 3.15)
project(im_guitar CXX)

# Emscripten (WebAssembly) 빌드인 경우의 추가 설정
if(EMSCRIPTEN)
    # 컴파일 시 SDL2 사용
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2 -s MAX_WEBGL_VERSION=2 -s MIN_WEBGL_VERSION=2 -s ALLOW_MEMORY_GROWTH=1")
    
    # 링크 시에도 SDL2 사용 + 이미지 폴더 패키징
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_SDL=2 --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets")
    
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

include(FetchContent)

# ---------------------------------------------------------------------------
# Third-party — nlohmann/json
#   header-only JSON 라이브러리. Phase 5의 ScenarioLoader가 scenarios/*.json
#   파일을 ScenarioConfig 구조체로 파싱할 때 사용.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# Third-party — GoogleTest
#   Google의 C++ 단위 테스트 프레임워크. tests/ 하위의 phase 별 컴포넌트
#   테스트를 작성하고 ctest로 실행.
# ---------------------------------------------------------------------------
option(IMGUITAR_BUILD_TESTS "Build unit tests" ON)
if(IMGUITAR_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# ---------------------------------------------------------------------------
# ImGui — static library target
# ---------------------------------------------------------------------------
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")

add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

# ---------------------------------------------------------------------------
# SDL2 / OpenGL
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
    # 웹 환경에서는 내장된 SDL2를 사용하므로 find_package 없이 인터페이스 라이브러리만 만들어서 링크
    set(SDL2_LIBRARIES "")
    set(SDL2_INCLUDE_DIRS "")
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    add_library(SDL2::SDL2main INTERFACE IMPORTED)
else()
    # 데스크톱 빌드인 경우 시스템에 설치된 SDL2를 찾아 링크
    find_package(SDL2 REQUIRED)
endif()
find_package(OpenGL REQUIRED)

if(TARGET SDL2::SDL2)
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
else()
    target_include_directories(imgui PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(imgui PUBLIC ${SDL2_LIBRARIES})
endif()

target_link_libraries(imgui PUBLIC OpenGL::GL)

# ---------------------------------------------------------------------------
# model_lib — backend 도메인 코드 (model/*.cpp)
#   app과 unit_tests가 공통으로 링크. main.cpp / view / controller는 제외.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MODEL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/model/*.cpp"
)

if(MODEL_SOURCES)
    add_library(model_lib STATIC ${MODEL_SOURCES})
    target_include_directories(model_lib PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/src/common"
        "${CMAKE_SOURCE_DIR}/src/model"
        "${CMAKE_SOURCE_DIR}/src/model/event"
        "${CMAKE_SOURCE_DIR}/src/model/product"
        "${CMAKE_SOURCE_DIR}/src/model/stats"
        "${CMAKE_SOURCE_DIR}/src/model/conveyor"
        "${CMAKE_SOURCE_DIR}/src/model/machine"
        "${CMAKE_SOURCE_DIR}/src/model/machine/spawner"
        "${CMAKE_SOURCE_DIR}/src/model/machine/cutter"
        "${CMAKE_SOURCE_DIR}/src/model/machine/painter"
        "${CMAKE_SOURCE_DIR}/src/model/machine/multiple"
        "${CMAKE_SOURCE_DIR}/src/model/machine/multiple/collector"
        "${CMAKE_SOURCE_DIR}/src/model/machine/multiple/assembler"
        "${CMAKE_SOURCE_DIR}/src/model/machine/packager"
        "${CMAKE_SOURCE_DIR}/src/model/technician"
        "${CMAKE_SOURCE_DIR}/src/model/memento"
        "${CMAKE_SOURCE_DIR}/src/model/scenario"
        "${CMAKE_SOURCE_DIR}/src/model/repair_dispatcher"
        "${CMAKE_SOURCE_DIR}/src/model/factory"
    )
    target_link_libraries(model_lib PUBLIC nlohmann_json::nlohmann_json)
endif()

# ---------------------------------------------------------------------------
# scenarios/ — JSON 시나리오 파일을 build output 디렉토리로 복사.
#   ScenarioLoader가 baseDir 기본값 "scenarios"로 상대경로 로드. 실행 작업
#   디렉토리(app / ctest 모두 build/) 기준 build/scenarios/*.json을 참조.
# ---------------------------------------------------------------------------
file(GLOB SCENARIO_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/scenarios/*.json")
if(SCENARIO_FILES)
    file(COPY ${SCENARIO_FILES} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scenarios")
endif()

# ---------------------------------------------------------------------------
# Application — main.cpp + view + controller + model_lib
# ---------------------------------------------------------------------------
set(APP_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
file(GLOB_RECURSE VIEW_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/view/*.cpp"
)
file(GLOB_RECURSE CONTROLLER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/controller/*.cpp"
)
list(APPEND APP_SOURCES ${VIEW_SOURCES} ${CONTROLLER_SOURCES})

add_executable(app ${APP_SOURCES})

target_include_directories(app PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_SOURCE_DIR}/src/view"
    "${CMAKE_SOURCE_DIR}/src/controller"
)
target_link_libraries(app PRIVATE imgui nlohmann_json::nlohmann_json)
if(TARGET model_lib)
    target_link_libraries(app PRIVATE model_lib)
endif()

# Windows: GUI 서브시스템 + SDL2의 WinMain→main 어댑터(SDL2main) 명시 링크
if(WIN32)
    set_target_properties(app PROPERTIES WIN32_EXECUTABLE TRUE)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(app PRIVATE SDL2::SDL2main)
    elseif(MINGW)
        target_link_libraries(app PRIVATE mingw32 SDL2main)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests — phase 별 단위 테스트
# ---------------------------------------------------------------------------
if(IMGUITAR_BUILD_TESTS)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    )
    # phase_7_controller.cpp 가 Controller.cpp 를 필요로 함 (model_lib 외부)
    file(GLOB CONTROLLER_TEST_DEPS CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/controller/*.cpp"
    )
    if(TEST_SOURCES)
        add_executable(unit_tests ${TEST_SOURCES} ${CONTROLLER_TEST_DEPS})
        target_include_directories(unit_tests PRIVATE
            "${CMAKE_SOURCE_DIR}/src"
            "${CMAKE_SOURCE_DIR}/src/common"
            "${CMAKE_SOURCE_DIR}/src/controller"
        )
        target_link_libraries(unit_tests PRIVATE
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        if(TARGET model_lib)
            target_link_libraries(unit_tests PRIVATE model_lib)
        endif()
        # gtest_discover_tests를 사용하면 build 단계에서 exe를 실행해 테스트를 등록하는데,
        # Windows MinGW 환경에서는 build 작업 디렉토리가 DLL 경로와 어긋나 5초 timeout 안에
        # 첫 startup조차 마치지 못하는 경우가 있다 (0xc0000139 entry-point 오류).
        # add_test로 통째로 한 번 실행하는 방식으로 우회 — 개별 케이스는 gtest --list-tests
        # 또는 unit_tests.exe 직접 실행으로 확인.
        add_test(NAME unit_tests COMMAND unit_tests)
    endif()
endif()