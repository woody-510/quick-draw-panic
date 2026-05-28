# ==============================================================================
# CMakeLists.txt — 早撃ちパニック（Quick Draw Panic）ビルド設定
# ==============================================================================
# 【このファイルの役割】
# CMakeというビルドツールの設定ファイル。
# 「どのソースコードを、どのライブラリと一緒にコンパイルするか」を定義する。
# ネイティブ（Windows/Mac/Linux）とWebAssembly（Emscripten）の両方に対応。
#
# 【ビルド方法（ネイティブ）】
#   mkdir build
#   cd build
#   cmake ..
#   cmake --build .
#
# 【ビルド方法（Emscripten / WebAssembly）】
#   mkdir build-web
#   cd build-web
#   emcmake cmake ..
#   cmake --build .
#   （build-web フォルダ内で python -m http.server してブラウザで開く）
# ==============================================================================

# --- CMakeの最低バージョン ---
# FetchContent機能を使うため、3.14以上が必要
cmake_minimum_required(VERSION 3.14)

# --- プロジェクト名と使用言語 ---
# "sataro" がプロジェクト名。C言語を使用。
project(sataro C)

# --- C言語の標準バージョン ---
# C99を使用（//コメントやfor文内の変数宣言が使える）
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ==============================================================================
# Raylibの自動ダウンロードと設定
# ==============================================================================
# FetchContentを使うと、Raylibを自動でダウンロードしてビルドしてくれる。
# 手動でRaylibをインストールする必要がなくなる便利な仕組み。
include(FetchContent)

# --- Raylibのダウンロード元とバージョンを指定 ---
FetchContent_Declare(
    raylib                                              # ライブラリ名
    GIT_REPOSITORY https://github.com/raysan5/raylib.git  # GitHubのURL
    GIT_TAG        5.5                                  # バージョン（タグ）
    GIT_SHALLOW    TRUE                                 # 履歴を省略して高速化
)

# --- Raylibのビルドオプション ---
# サンプルやテストは不要なのでOFF
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS    OFF CACHE BOOL "" FORCE)

# 【重要】RaylibはデフォルトでJPEG画像を読み込めない！
# CUSTOMIZE_BUILD と SUPPORT_FILEFORMAT_JPG を ON にすることで
# JPEG読み込みを有効化する。これがないと LoadImage("1.jpeg") が失敗する。
set(CUSTOMIZE_BUILD       ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)

# --- Raylibをダウンロード＆ビルド可能にする ---
FetchContent_MakeAvailable(raylib)

# ==============================================================================
# 実行ファイルの定義
# ==============================================================================
# 3つのソースファイルをまとめて1つの実行ファイル "sataro" を作る
add_executable(${PROJECT_NAME}
    main.c          # エントリーポイント、メインループ、画面遷移
    logic.c         # ゲームロジック（的の生成、当たり判定、スコア）
    render.c        # 描画処理（疑似3D、UI、エフェクト）
)

# --- Raylibとリンク ---
# PRIVATE = このプロジェクト内だけで使う（外部に公開しない）
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# --- ヘッダーファイルの検索パス ---
# プロジェクトのルートディレクトリにあるヘッダーを参照できるようにする
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ==============================================================================
# WebAssembly（Emscripten）用の追加設定
# ==============================================================================
# emcmake cmake .. で実行するとEMSCRIPTENが定義される
if(EMSCRIPTEN)
    # --- Emscripten用のコンパイル・リンクフラグ ---
    # -sUSE_GLFW=3       : GLFWライブラリを使う（Raylibが必要とする）
    # -sASYNCIFY          : 非同期処理を有効化（メインループに必要）
    # -sFORCE_FILESYSTEM=1: ファイルシステムを有効化（画像読み込みに必要）
    # --preload-file      : 画像フォルダをWebAssemblyに埋め込む
    #                       "image@image" = 「ローカルのimageフォルダを、
    #                        仮想ファイルシステム上の/imageとしてマウント」
    set(WEB_LINK_FLAGS
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "--preload-file ${CMAKE_SOURCE_DIR}/image@image"
    )

    # フラグをスペース区切りの文字列に変換（CMakeの仕様で必要）
    string(REPLACE ";" " " WEB_LINK_FLAGS_STR "${WEB_LINK_FLAGS}")

    # リンクフラグを設定し、出力を .html にする
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "${WEB_LINK_FLAGS_STR}"
        SUFFIX ".html"
    )
endif()

# ==============================================================================
# 【追記】ネイティブビルド時のアセット自動コピー
# ==============================================================================
# .exeファイルが生成される build フォルダ内に、image と audio フォルダをコピーする
if(NOT EMSCRIPTEN)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/image DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    
    # audioフォルダが存在する場合のみコピー
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/audio)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/audio DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endif()

# --- C言語の標準バージョン ---
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ==============================================================================
# 【追記】Windows環境(MSVC)での文字化け・C4819警告の対策
# ==============================================================================
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ==============================================================================
# Raylibの自動ダウンロードと設定
# ...（これ以降は元のまま）...
set(WEB_LINK_FLAGS
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "--preload-file ${CMAKE_SOURCE_DIR}/image@image"
        "--shell-file ${raylib_SOURCE_DIR}/src/minshell.html"
    )
set(WEB_LINK_FLAGS
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32"
        "--preload-file ${CMAKE_SOURCE_DIR}/image@image"
        "--shell-file ${raylib_SOURCE_DIR}/src/minshell.html"
    )