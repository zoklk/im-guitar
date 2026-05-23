cmake_minimum_required(VERSION 3.15)
project(im_guitar CXX)

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
find_package(SDL2 REQUIRED)
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
    )
    target_link_libraries(model_lib PUBLIC nlohmann_json::nlohmann_json)
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
    if(TEST_SOURCES)
        add_executable(unit_tests ${TEST_SOURCES})
        target_include_directories(unit_tests PRIVATE
            "${CMAKE_SOURCE_DIR}/src"
            "${CMAKE_SOURCE_DIR}/src/common"
        )
        target_link_libraries(unit_tests PRIVATE
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        if(TARGET model_lib)
            target_link_libraries(unit_tests PRIVATE model_lib)
        endif()
        include(GoogleTest)
        gtest_discover_tests(unit_tests)
    endif()
endif()
