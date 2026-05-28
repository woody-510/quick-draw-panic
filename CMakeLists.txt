# ==============================================================================
# CMakeLists.txt — 早撃ちパニック（Quick Draw Panic）ビルド設定
# ==============================================================================
cmake_minimum_required(VERSION 3.14)
project(sataro C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Raylibの自動ダウンロードと設定 ---
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD       ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# --- 実行ファイルの定義 ---
add_executable(${PROJECT_NAME} main.c logic.c render.c)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ==============================================================================
# WebAssembly（Emscripten）用の追加設定
# ==============================================================================
if(EMSCRIPTEN)
    # 【重要】ここに音声許可(HEAPF32)と全画面シェル(minshell)を正しく配置
    set(WEB_LINK_FLAGS
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,HEAPF32"
        "--preload-file ${CMAKE_SOURCE_DIR}/image@image"
        "--preload-file ${CMAKE_SOURCE_DIR}/audio@audio"
        "--shell-file ${raylib_SOURCE_DIR}/src/minshell.html"
    )

    string(REPLACE ";" " " WEB_LINK_FLAGS_STR "${WEB_LINK_FLAGS}")

    # ここでフラグを適用してビルドを確定させる
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "${WEB_LINK_FLAGS_STR}"
        SUFFIX ".html"
    )
endif()

# ==============================================================================
# ネイティブビルド（PC用 .exe）時の設定
# ==============================================================================
if(NOT EMSCRIPTEN)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/image DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/audio)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/audio DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()