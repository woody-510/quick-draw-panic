/* =============================================================================
 * main.c — 早撃ちパニック（Quick Draw Panic）エントリーポイント
 * =============================================================================
 * 【このファイルの役割】
 * プログラムの「玄関」。ゲームが起動すると、一番最初にこのファイルの
 * main() 関数が呼ばれる。主に以下の3つを担当する：
 *
 *   1. 初期化：ウィンドウを開く、画像を読み込む、BGMを準備する
 *   2. メインループ：毎フレーム「更新→描画」を繰り返す
 *   3. 後片付け：ゲーム終了時にメモリやデバイスを解放する
 *
 * 【グローバル変数の「実体」について】
 * game.h では extern（宣言のみ）だった変数に、ここで実際のメモリを割り当てる。
 * extern宣言 = 「こういう変数があるよ」という看板
 * ここでの定義 = 「看板の場所に実際に建物を建てる」
 *
 * 【Emscripten対応について】
 * ネイティブビルド（Windows/Mac/Linux）では while ループでメインループを回すが、
 * WebAssembly（ブラウザ）ではブラウザの描画サイクルに合わせる必要がある。
 * emscripten_set_main_loop() を使って、ブラウザが毎フレーム呼んでくれるように
 * コールバック関数を登録する。
 * ========================================================================== */

/* -------------------------------------------------------------------------
 * インクルードセクション
 * -------------------------------------------------------------------------
 * game.h  : 定数、構造体、グローバル変数のextern宣言
 * logic.h : ゲームロジック関数（更新処理・スコア管理など）
 * render.h: 描画関数（各画面の描画）
 * stdio.h : printf() でデバッグ出力するため
 * string.h: memset() を使う場合に備えて（安全策）
 * ------------------------------------------------------------------------- */
#include "game.h"
#include "logic.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

/* --- Emscriptenプラットフォーム判定 ---
 * Emscriptenでビルドすると PLATFORM_WEB マクロが定義される。
 * このマクロが定義されている場合のみ、emscriptenヘッダーをインクルードする。
 * ネイティブビルドではこのブロックは丸ごとスキップされる。 */
#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

/* ==========================================================================
 * グローバル変数の「実体」定義
 * ==========================================================================
 * game.h では extern キーワード付きで「宣言」だけしていた変数に、
 * ここで実際のメモリを確保する。プログラム全体でこの1箇所だけで定義する。
 *
 * {0} は「全メンバーをゼロで初期化する」イディオム（C99以降）。
 * ・int → 0
 * ・float → 0.0f
 * ・bool → false
 * ・ポインタ → NULL
 * これにより、未初期化変数によるバグを防ぐ。
 * ========================================================================== */

/* ゲーム全体の状態。スコア、ステージ、的の配列、タイマー等すべてを管理する。
 * logic.c が値を更新し、render.c が値を読んで描画する。 */
GameState game = {0};

/* スコアデータ。各モードのTOP5ランキングを保持する。
 * ファイルまたはlocalStorageに永続化される。 */
ScoreData scores = {0};

/* 的の画像テクスチャ配列（16枚分）。
 * textures[0] = ステージ1（1.jpeg）の画像
 * textures[15] = ステージ16（16.jpeg）の画像
 * GPU上のテクスチャハンドルを保持するため、プログラム終了時に
 * UnloadTexture() で明示的に解放する必要がある。 */
Texture2D textures[TOTAL_STAGES] = {0};

/* BGMのストリーム配列（5曲分）。
 * bgm[0] = タイトル画面BGM
 * bgm[1] = タイムアタックBGM
 * bgm[2] = サバイバルBGM
 * bgm[3] = マラソンBGM
 * bgm[4] = リザルト画面BGM */
Music bgm[BGM_COUNT] = {0};

/* 各BGMの読み込み状態フラグ。
 * bgmLoaded[i] == true  → bgm[i] は正常に読み込まれ、再生可能
 * bgmLoaded[i] == false → ファイルが見つからなかった等で読み込み失敗
 * 再生前に必ずこのフラグをチェックし、falseなら再生をスキップする。 */
bool bgmLoaded[BGM_COUNT] = {0};

/* --- 拡大表示（スケーリング）用の仮想画面 ---
 * ウィンドウサイズが変わってもゲーム内部のレイアウト（450x800）を崩さないために、
 * 一度この RenderTexture2D に描画し、それを画面サイズに合わせて引き伸ばして表示する。 */
static RenderTexture2D renderTarget = {0};

/* ==========================================================================
 * LoadAndCropSquare — 画像を読み込み、正方形にクロップしてGPUへアップロード
 * ==========================================================================
 * 【この関数の目的】
 * JPEGファイルから画像を読み込み、以下の加工を行ってテクスチャにする：
 *   1. 正方形にクロップ（中央切り抜き）
 *   2. TEXTURE_LOAD_SIZE × TEXTURE_LOAD_SIZE にリサイズ
 *   3. GPUにアップロードしてTexture2Dとして返す
 *
 * 【なぜ正方形にクロップするのか？】
 * 的はゲーム画面上で正方形として描画される。元画像が長方形だと
 * 歪んで見えるため、先に正方形に切り抜いておく。
 * クロップは中央基準で行い、画像の一番「おいしい」部分を残す。
 *
 * 【なぜリサイズするのか？】
 * 元画像が巨大（例: 4000×3000px）だとGPUメモリを大量に消費する。
 * ゲーム上では最大120px程度でしか表示しないため、256×256にリサイズしても
 * 見た目は十分きれいで、メモリ節約になる。
 *
 * 【引数】
 * filename: 読み込むJPEGファイルのパス（例: "image/1.jpeg"）
 *
 * 【戻り値】
 * Texture2D: GPUにアップロード済みのテクスチャ。
 *            読み込み失敗時はtex.id == 0 の空テクスチャを返す。
 *
 * 【注意点】
 * ・Image（CPUメモリ）とTexture2D（GPUメモリ）は別物。
 *   Image → 加工（クロップ、リサイズ）→ Texture2Dへ変換 → Image解放
 *   という流れを守る。Image を解放し忘れるとメモリリークになる。
 * ========================================================================== */
static Texture2D LoadAndCropSquare(const char *filename)
{
    /* --- ステップ1: ファイルからCPUメモリに画像を読み込む ---
     * LoadImage() はRaylibの関数。対応フォーマット: PNG, JPEG, BMP等。
     * 戻り値の img.data が NULL なら読み込み失敗。 */
    Image img = LoadImage(filename);

    /* --- ステップ2: 読み込み失敗チェック ---
     * ファイルが存在しない、パスが間違っている、フォーマットが非対応などの
     * 場合、img.data は NULL になる。
     * 空の Texture2D（全メンバー0）を返して呼び出し元に失敗を知らせる。 */
    if (img.data == NULL) {
        printf("[エラー] 画像ファイルの読み込みに失敗しました: %s\n", filename);
        Texture2D emptyTex = {0};   /* 全フィールドを0初期化した空テクスチャ */
        return emptyTex;
    }

    /* --- ステップ3: 正方形クロップサイズの計算 ---
     * 短い方の辺を基準にして正方形を切り出す。
     *
     * 例: 画像が 800×600 の場合
     *   cropSize = min(800, 600) = 600
     *   offsetX = (800 - 600) / 2 = 100  → 左右100pxずつカット
     *   offsetY = (600 - 600) / 2 = 0    → 上下カットなし
     *
     * 例: 画像が 500×1000 の場合
     *   cropSize = min(500, 1000) = 500
     *   offsetX = (500 - 500) / 2 = 0    → 左右カットなし
     *   offsetY = (1000 - 500) / 2 = 250 → 上下250pxずつカット */
    int cropSize = (img.width < img.height) ? img.width : img.height;
    int offsetX = (img.width - cropSize) / 2;   /* 左からのオフセット */
    int offsetY = (img.height - cropSize) / 2;  /* 上からのオフセット */

    /* --- ステップ4: 正方形にクロップ ---
     * Rectangle は { x, y, width, height } の構造体。
     * ImageCrop() はCPUメモリ上の画像データを直接書き換える（破壊的操作）。
     * &img のようにポインタを渡すのは、関数内で画像データを変更するため。 */
    Rectangle cropRect = {
        (float)offsetX,     /* クロップ開始のX座標 */
        (float)offsetY,     /* クロップ開始のY座標 */
        (float)cropSize,    /* クロップする幅 */
        (float)cropSize     /* クロップする高さ（正方形なので幅と同じ） */
    };
    ImageCrop(&img, cropRect);

    /* --- ステップ5: テクスチャ用サイズにリサイズ ---
     * TEXTURE_LOAD_SIZE (256) × TEXTURE_LOAD_SIZE (256) にリサイズする。
     * これにより、巨大な元画像でもGPUメモリの消費を一定に抑えられる。
     * 256は2のべき乗（2^8）であり、GPUが効率的に扱えるサイズ。
     * ImageResize() も破壊的操作。バイリニア補間でリサイズされる。 */
    ImageResize(&img, TEXTURE_LOAD_SIZE, TEXTURE_LOAD_SIZE);

    /* --- ステップ6: GPU にアップロード ---
     * LoadTextureFromImage() は CPUメモリの Image データを
     * GPUメモリ（VRAM）にコピーして Texture2D として使えるようにする。
     * この時点で img（CPU側）と tex（GPU側）は独立したデータになる。 */
    Texture2D tex = LoadTextureFromImage(img);

    /* --- ステップ7: CPUメモリの画像データを解放 ---
     * GPU にアップロード済みなので、CPU側の Image データはもう不要。
     * これを忘れると、16枚分の画像がCPUメモリに残り続ける（メモリリーク）。 */
    UnloadImage(img);

    /* --- 完成したテクスチャを返す --- */
    return tex;
}

/* ==========================================================================
 * LoadAllTextures — 全16枚の的画像をまとめて読み込む
 * ==========================================================================
 * 【この関数の目的】
 * ステージ1〜16に対応する16枚の画像ファイル（image/1.jpeg 〜 image/16.jpeg）を
 * 順番に読み込み、正方形にクロップしてテクスチャ配列に格納する。
 *
 * 【ファイル名の命名規則】
 * "image/N.jpeg" （N = 1, 2, 3, ..., 16）
 * ・配列のインデックスは 0〜15 だが、ファイル名は 1〜16 なので i+1 を使う。
 * ・snprintf() を使って安全にファイル名文字列を組み立てる。
 *   sprintf() は バッファオーバーフローの危険があるため、必ず snprintf() を使う。
 *
 * 【snprintf の使い方】
 * snprintf(出力バッファ, バッファサイズ, フォーマット, ...);
 * ・バッファサイズを超える書き込みは自動的にカットされる
 * ・末尾の '\0'（ヌル終端）も保証される
 * ========================================================================== */
static void LoadAllTextures(void)
{
    printf("[情報] テクスチャの読み込みを開始します（全%d枚）...\n", TOTAL_STAGES);

    for (int i = 0; i < TOTAL_STAGES; i++) {
        /* ファイル名を組み立てるバッファ。
         * "image/16.jpeg" は最大14文字 + '\0' = 15バイト。
         * 余裕を持って64バイト確保する。 */
        char filename[64];

        /* snprintf でファイル名を安全に組み立てる。
         * i はインデックス（0始まり）、ファイル名は1始まりなので i+1 を使う。 */
        snprintf(filename, sizeof(filename), "image/%d.jpeg", i + 1);

        /* LoadAndCropSquare で正方形クロップ + リサイズ + GPU転送 */
        textures[i] = LoadAndCropSquare(filename);

        /* 読み込み結果をログ出力。
         * tex.id が 0 以外なら成功、0 なら失敗。 */
        if (textures[i].id != 0) {
            printf("[情報] テクスチャ読み込み成功: %s （%dx%d）\n",
                   filename, textures[i].width, textures[i].height);
        } else {
            printf("[警告] テクスチャ読み込み失敗: %s\n", filename);
        }
    }

    printf("[情報] テクスチャの読み込みが完了しました。\n");
}

/* ==========================================================================
 * UnloadAllTextures — 全テクスチャをGPUメモリから解放する
 * ==========================================================================
 * 【この関数の目的】
 * プログラム終了時に、GPUメモリ（VRAM）に確保した全テクスチャを解放する。
 * これを忘れても、OSが終了時に自動でクリーンアップしてくれることが多いが、
 * 行儀の良いプログラムは自分で後片付けをする。
 *
 * 【注意】
 * Emscripten（WebAssembly）環境では、ブラウザのタブを閉じた時に
 * メモリが自動解放されるため、この関数が呼ばれないこともある。
 * しかしネイティブビルドでは必ず呼ばれるべき。
 * ========================================================================== */
static void UnloadAllTextures(void)
{
    printf("[情報] テクスチャを解放します...\n");

    for (int i = 0; i < TOTAL_STAGES; i++) {
        /* UnloadTexture() はGPUメモリ上のテクスチャデータを解放する。
         * tex.id == 0 の場合（読み込み失敗だったテクスチャ）でも
         * Raylibは安全にスキップしてくれるので、チェック不要。 */
        UnloadTexture(textures[i]);
    }

    printf("[情報] テクスチャの解放が完了しました。\n");
}

/* ==========================================================================
 * InitBGM — BGMファイルの読み込みを試みる
 * ==========================================================================
 * 【この関数の目的】
 * 5曲分のBGMファイル（MP3）を読み込む。ファイルが存在する場合のみ読み込み、
 * 存在しない場合は bgmLoaded[i] = false にしてスキップする。
 *
 * 【なぜ FileExists() でチェックするのか？】
 * ゲーム開発の初期段階では、BGMファイルがまだ用意できていないことがある。
 * 存在しないファイルを LoadMusicStream() に渡すとエラーが出るため、
 * 事前に FileExists() で確認してから読み込む「防御的プログラミング」を行う。
 * これにより、BGMファイルなしでもゲームが正常に動作する。
 *
 * 【BGMスロットの構成】
 * bgm[0] = タイトル画面 BGM       (BGM_TITLE_PATH)
 * bgm[1] = タイムアタックモード BGM (BGM_TIMEATTACK_PATH)
 * bgm[2] = サバイバルモード BGM     (BGM_SURVIVAL_PATH)
 * bgm[3] = マラソンモード BGM       (BGM_MARATHON_PATH)
 * bgm[4] = リザルト画面 BGM        (BGM_RESULT_PATH)
 *
 * 【BGMファイルを追加する手順】
 * 1. audio/ ディレクトリにMP3ファイルを配置する
 * 2. game.h の BGM_*_PATH 定数をそのファイル名に書き換える
 * 3. 再ビルドする → 自動的にBGMが鳴るようになる
 * ========================================================================== */
static void InitBGM(void)
{
    printf("[情報] BGMの読み込みを開始します（全%d曲）...\n", BGM_COUNT);

    /* 各BGMスロットに対応するファイルパスの配列。
     * game.h で定義された定数を使って、ループ処理できるようにまとめる。
     * const char * const = ポインタ自体もポインタの指す先も変更不可 */
    const char * const bgmPaths[BGM_COUNT] = {
        BGM_TITLE_PATH,         /* スロット0: タイトル画面 */
        BGM_TIMEATTACK_PATH,    /* スロット1: タイムアタック */
        BGM_SURVIVAL_PATH,      /* スロット2: サバイバル */
        BGM_MARATHON_PATH,      /* スロット3: マラソン */
        BGM_RESULT_PATH         /* スロット4: リザルト画面 */
    };

    /* 各BGMスロットについて読み込みを試みる */
    for (int i = 0; i < BGM_COUNT; i++) {
        /* FileExists() はRaylibの関数。ファイルの存在チェックを行う。
         * trueならファイルが存在する、falseなら存在しない。 */
        if (FileExists(bgmPaths[i])) {
            /* --- ファイルが見つかった場合 ---
             * LoadMusicStream() でBGMをストリーミング読み込みする。
             * ストリーミング = ファイル全体をメモリに載せず、再生に必要な分だけ
             * 少しずつ読み込む方式。長いBGMでもメモリ消費が少ない。 */
            bgm[i] = LoadMusicStream(bgmPaths[i]);
            bgmLoaded[i] = true;
            printf("[情報] BGM読み込み成功: スロット%d = %s\n", i, bgmPaths[i]);
        } else {
            /* --- ファイルが見つからなかった場合 ---
             * bgmLoaded[i] を false にして、再生時にスキップされるようにする。
             * これはエラーではなく、想定内の状況。 */
            bgmLoaded[i] = false;
            printf("[情報] BGMファイルが見つかりません（スキップ）: スロット%d = %s\n",
                   i, bgmPaths[i]);
        }
    }

    printf("[情報] BGMの読み込みが完了しました。\n");
}

/* ==========================================================================
 * PlayBGMForScreen — 画面状態に応じたBGMを再生する
 * ==========================================================================
 * 【この関数の目的】
 * 画面が切り替わったときに、適切なBGMを再生する。
 * まず全てのBGMを停止してから、新しい画面に合ったBGMを再生する。
 *
 * 【BGMの対応表】
 * 画面状態            → BGMスロット
 * SCREEN_TITLE        → bgm[0] (タイトル)
 * SCREEN_PLAYING      → bgm[1] (タイムアタック) / bgm[2] (サバイバル) / bgm[3] (マラソン)
 * SCREEN_RESULT       → bgm[4] (リザルト)
 * SCREEN_PAUSE        → BGMは変更しない（音量を下げるだけ）
 * SCREEN_COUNTDOWN    → BGMは変更しない
 * SCREEN_SCORES       → BGMは変更しない（タイトルBGMが流れ続ける）
 *
 * 【引数】
 * screen: 切り替え先の画面状態（ScreenState型）
 *
 * 【なぜ全BGMを止めてから再生するのか？】
 * 複数のBGMが同時に鳴ると不快なため。画面遷移のたびに「全停止→新BGM再生」
 * という流れでクリーンに切り替える。
 * ========================================================================== */
static void PlayBGMForScreen(ScreenState screen)
{
    /* --- 全BGMの停止 ---
     * StopMusicStream() は再生中でなくても安全に呼べる。
     * bgmLoaded[i] が false の場合は無効なストリームなのでスキップする。 */
    for (int i = 0; i < BGM_COUNT; i++) {
        if (bgmLoaded[i]) {
            StopMusicStream(bgm[i]);
        }
    }

    /* --- 画面状態に応じたBGMの選択と再生 --- */
    int bgmIndex = -1;  /* 再生するBGMのスロット番号。-1 = 再生しない */

    switch (screen) {
        case SCREEN_TITLE:
            /* タイトル画面: スロット0（タイトルBGM）を再生 */
            bgmIndex = 0;
            break;

        case SCREEN_PLAYING:
            /* プレイ画面: ゲームモードに応じたBGMを選択
             * MODE_TIMEATTACK (0) → スロット1
             * MODE_SURVIVAL   (1) → スロット2
             * MODE_MARATHON   (2) → スロット3 */
            switch (game.mode) {
                case MODE_TIMEATTACK:
                    bgmIndex = 1;   /* タイムアタック用BGM */
                    break;
                case MODE_SURVIVAL:
                    bgmIndex = 2;   /* サバイバル用BGM */
                    break;
                case MODE_MARATHON:
                    bgmIndex = 3;   /* マラソン用BGM */
                    break;
            }
            break;

        case SCREEN_RESULT:
            /* リザルト画面: スロット4（リザルトBGM）を再生 */
            bgmIndex = 4;
            break;

        case SCREEN_PAUSE:
        case SCREEN_COUNTDOWN:
        case SCREEN_SCORES:
            /* これらの画面ではBGMを切り替えない。
             * ポーズ中は音量を下げる処理を別途行う場合がある。
             * スコア画面はタイトルBGMがそのまま流れ続ける。 */
            break;
    }

    /* --- 選択されたBGMを再生 ---
     * bgmIndex が有効なスロット番号であり、
     * かつそのスロットのBGMが正常に読み込まれている場合のみ再生する。 */
    if (bgmIndex >= 0 && bgmIndex < BGM_COUNT && bgmLoaded[bgmIndex]) {
        /* PlayMusicStream() でBGMの再生を開始する。
         * ストリーミング再生なので、毎フレーム UpdateMusicStream() を
         * 呼ばないと途中で止まる。 */
        PlayMusicStream(bgm[bgmIndex]);

        /* SetMusicVolume() で音量を設定する。
         * BGM_VOLUME_NORMAL (0.8) = 通常の再生音量。
         * 0.0 = 無音, 1.0 = 最大音量 */
        SetMusicVolume(bgm[bgmIndex], BGM_VOLUME_NORMAL);

        printf("[情報] BGM再生開始: スロット%d\n", bgmIndex);
    }
}

/* ==========================================================================
 * UpdateBGMStreams — 全BGMストリームのバッファを更新する
 * ==========================================================================
 * 【この関数の目的】
 * 毎フレーム呼び出して、再生中のBGMストリームにデータを供給し続ける。
 *
 * 【なぜ毎フレーム呼ぶ必要があるのか？】
 * ストリーミング再生は、音楽ファイルを小さなチャンク（塊）に分けて
 * 少しずつデコード・再生する仕組み。UpdateMusicStream() を呼ばないと
 * バッファ内のデータを使い切った時点で音が途切れてしまう。
 *
 * 【パフォーマンスへの影響】
 * 5曲分のUpdateMusicStream() は非常に軽量な処理。
 * 再生していないストリームに対して呼んでも問題ない（Raylibが内部で判定する）。
 * ========================================================================== */
static void UpdateBGMStreams(void)
{
    for (int i = 0; i < BGM_COUNT; i++) {
        /* 読み込みに成功したBGMのみ更新する。
         * bgmLoaded[i] == false のスロットは無効なストリームなのでスキップ。 */
        if (bgmLoaded[i]) {
            UpdateMusicStream(bgm[i]);
        }
    }
}

/* ==========================================================================
 * UpdateDrawFrame — メインループの1フレーム分の処理
 * ==========================================================================
 * 【この関数の目的】
 * ゲームの「心臓部」。毎フレーム（1秒に60回）呼ばれ、以下を行う：
 *   1. 経過時間（dt）の取得
 *   2. BGMストリームの更新
 *   3. 現在の画面状態に応じた更新処理（ロジック）
 *   4. 現在の画面状態に応じた描画処理（レンダリング）
 *
 * 【ネイティブビルドとWebビルドの違い】
 * ・ネイティブ: while ループの中から毎フレーム呼ばれる
 * ・Web: emscripten_set_main_loop() に登録したコールバックとして
 *        ブラウザのrequestAnimationFrame()のタイミングで呼ばれる
 *
 * 【更新と描画を分ける理由】
 * 更新（Update）= ゲームのルールに基づいてデータを変更する（見えない処理）
 * 描画（Draw）= データに基づいて画面に絵を描く（見える処理）
 * これを分けることで、コードが整理され、バグも見つけやすくなる。
 *
 * 【注意】
 * この関数は static ではない。なぜなら、Emscriptenビルドでは
 * emscripten_set_main_loop() に関数ポインタとして渡す必要があるため。
 * ただし、ネイティブビルドでは main() からしか呼ばないので問題ない。
 * ========================================================================== */
void UpdateDrawFrame(void)
{
    float dt = GetFrameTime();
    UpdateBGMStreams();

    /* =========================================================================
     * スケーリング（仮想解像度）のためのマウス座標補正
     * =========================================================================
     * ウィンドウサイズが変更されても、ゲーム内部では 450x800 として
     * 処理を続けるため、マウス座標をそれに合わせて逆変換する。 */
    float scaleX = (float)GetScreenWidth() / SCREEN_WIDTH;
    float scaleY = (float)GetScreenHeight() / SCREEN_HEIGHT;
    float finalScale = (scaleX < scaleY) ? scaleX : scaleY; /* アスペクト比を保つため小さい方を採用 */

    /* 描画時のレターボックス/ピラーボックス（黒帯）の幅を計算 */
    int offsetX = (int)((GetScreenWidth() - (SCREEN_WIDTH * finalScale)) * 0.5f);
    int offsetY = (int)((GetScreenHeight() - (SCREEN_HEIGHT * finalScale)) * 0.5f);

    /* Raylib の SetMouseOffset / SetMouseScale を使うことで、
     * GetMousePosition() の戻り値が自動的に 450x800 の仮想座標に変換される */
    SetMouseOffset(-offsetX, -offsetY);
    SetMouseScale(1.0f / finalScale, 1.0f / finalScale);

    /* --- 画面状態に応じた更新処理 --- */
    switch (game.screen) {
        case SCREEN_TITLE:     UpdateTitle(dt); break;
        case SCREEN_PLAYING:   UpdatePlaying(dt); break;
        case SCREEN_PAUSE:     UpdatePause(dt); break;
        case SCREEN_COUNTDOWN: UpdateCountdown(dt); break;
        case SCREEN_RESULT:    UpdateResult(dt); break;
        case SCREEN_SCORES:    UpdateScoresScreen(dt); break;
    }

    /* =========================================================================
     * 仮想画面（RenderTexture2D）への描画フェーズ
     * ========================================================================= */
    BeginTextureMode(renderTarget);

    Color bgColor;
    if (game.screen == SCREEN_PLAYING || game.screen == SCREEN_PAUSE || game.screen == SCREEN_COUNTDOWN) {
        bgColor = STAGE_BG_COLORS[game.stage - 1];
    } else {
        bgColor = (Color){20, 20, 30, 255};
    }
    ClearBackground(bgColor);

    switch (game.screen) {
        case SCREEN_TITLE:     DrawTitleScreen(); break;
        case SCREEN_PLAYING:   DrawPlayingScreen(); break;
        case SCREEN_PAUSE:     DrawPauseScreen(); break;
        case SCREEN_COUNTDOWN: DrawCountdownScreen(); break;
        case SCREEN_RESULT:    DrawResultScreen(); break;
        case SCREEN_SCORES:    DrawScoresScreen(); break;
    }

    EndTextureMode(); /* 仮想画面への描画終了 */

    /* =========================================================================
     * 実際の画面への拡大描画フェーズ
     * ========================================================================= */
    BeginDrawing();
    ClearBackground(BLACK); /* 余白（黒帯）を黒で塗りつぶす */

    /* RenderTexture2D は OpenGL の仕様により Y 軸が反転して保存されるため、
     * 描画元の矩形 (sourceRec) の高さをマイナスにして上下反転を打ち消す。 */
    Rectangle sourceRec = { 0.0f, 0.0f, (float)renderTarget.texture.width, -(float)renderTarget.texture.height };
    Rectangle destRec = { (float)offsetX, (float)offsetY, (float)SCREEN_WIDTH * finalScale, (float)SCREEN_HEIGHT * finalScale };
    
    DrawTexturePro(renderTarget.texture, sourceRec, destRec, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndDrawing();
}

/* ==========================================================================
 * main — プログラムのエントリーポイント
 * ==========================================================================
 * 【この関数の役割】
 * プログラム起動時に最初に呼ばれる関数。
 * 以下の3つのフェーズを順番に実行する：
 *
 *   フェーズ1: 初期化（ウィンドウ作成、リソース読み込み）
 *   フェーズ2: メインループ（毎フレームの更新と描画の繰り返し）
 *   フェーズ3: 後片付け（リソース解放、デバイスクローズ）
 *
 * 【Emscripten（WebAssembly）での特殊対応】
 * ブラウザ環境では、メインスレッドをブロック（while で止める）してはいけない。
 * ブラウザのイベントループに制御を返す必要がある。
 * emscripten_set_main_loop() を使うと、ブラウザが毎フレーム
 * UpdateDrawFrame() を呼んでくれるようになる。
 *
 * 【戻り値】
 * 0 = 正常終了。慣例として return 0 を書く。
 * ========================================================================== */
int main(void)
{
    /* =================================================================
     * フェーズ1: 初期化
     * ================================================================= */

    /* --- ウィンドウの初期化 ---
     * ユーザーがウィンドウの端をドラッグしてサイズ変更できるように
     * FLAG_WINDOW_RESIZABLE フラグをセットしてから初期化する。 */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Quick Draw Panic");

    /* 仮想画面 (RenderTexture2D) の初期化
     * ゲーム内の描画は全てこの 450x800 のテクスチャに対して行い、最後に拡大表示する。 */
    renderTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_BILINEAR); /* 拡大時に滑らかにする */

    /* --- オーディオデバイスの初期化 ---
     * BGMや効果音を再生するために必要。
     * InitWindow() の後、音声関連の関数を使う前に呼ぶ。
     * この関数が失敗しても（スピーカーがない等）プログラムは続行できるが、
     * 音が鳴らなくなる。 */
    InitAudioDevice();

    /* --- テクスチャの読み込み ---
     * 16枚の的の画像ファイルをGPUにロードする。
     * InitWindow() の後でなければ、GPUにテクスチャをアップロードできない。 */
    LoadAllTextures();

    /* --- BGMの読み込み（ダミー枠） ---
     * 音声ファイルが用意されている場合のみ読み込む。
     * ファイルがなくてもエラーにならず、BGMなしで動作する。 */
    InitBGM();

    /* --- スコアの読み込み ---
     * ファイルまたはlocalStorageから過去のランキングデータを読み込む。
     * ファイルが存在しない場合（初回起動時等）は、全スコアが0のまま。
     * LoadScores() は logic.c に実装されている。 */
    LoadScores();

    /* --- ゲーム状態の初期化 ---
     * game構造体は {0} で初期化済みだが、タイトル画面に必要な値を
     * 明示的に設定する。 */
    game.screen = SCREEN_TITLE;  /* 最初はタイトル画面を表示 */

    /* スライドショーの初期設定:
     * slideIndex     = 現在表示中の画像番号（0 = ステージ1の画像）
     * slideIndexNext = フェード先の画像番号（1 = ステージ2の画像）
     * slideTimer     = 切り替えまでの残り時間（SLIDE_INTERVAL = 3.0秒）
     * slideAlpha     = 現在画像の不透明度（1.0 = 完全不透明） */
    game.slideIndex     = 0;
    game.slideIndexNext = 1;
    game.slideTimer     = SLIDE_INTERVAL;
    game.slideAlpha     = 1.0f;

    /* =================================================================
     * フェーズ2: メインループ
     * ================================================================= */

#if defined(PLATFORM_WEB)
    /* === Emscripten（WebAssembly）ビルドの場合 ===
     *
     * emscripten_set_main_loop(関数ポインタ, FPS, ループシミュレーション);
     *
     * 第1引数: 毎フレーム呼ばれるコールバック関数
     * 第2引数: 目標FPS。0 = ブラウザのリフレッシュレートに同期
     *          (requestAnimationFrame を使用)
     * 第3引数: 1 = main() がここで「止まる」ように振る舞う
     *          (実際には制御をブラウザに返す)
     *
     * 【なぜ fps=0 にするのか？】
     * ブラウザのリフレッシュレートは環境によって異なる（60Hz, 120Hz等）。
     * 0を指定するとブラウザに最適なタイミングで描画してくれる。
     *
     * 【simulate_infinite_loop=1 の意味】
     * この呼び出しの後のコード（後片付け等）が実行されないようにする。
     * ブラウザ環境では「ゲーム終了」という概念がないため。 */
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    /* === ネイティブビルド（Windows/Mac/Linux）の場合 ===
     *
     * SetTargetFPS() でフレームレートの上限を設定する。
     * TARGET_FPS (60) を設定すると、EndDrawing() 内で適切な待ち時間が
     * 挿入され、CPUを100%使い切らないようになる。
     * これを設定しないと、ゲームが可能な限り高速に動作し、
     * CPUファンが唸り、ノートPCのバッテリーが急速に消耗する。 */
    SetTargetFPS(TARGET_FPS);

    /* --- メインゲームループ ---
     * WindowShouldClose() は以下の場合に true を返す：
     *   ・ウィンドウの「×」ボタンが押された
     *   ・ESCキーが押された（Raylibのデフォルト動作）
     *   ・CloseWindow() が呼ばれた
     *
     * 毎フレーム UpdateDrawFrame() を呼び、「更新→描画」を繰り返す。
     * 60FPSなら、このループは1秒間に約60回実行される。 */
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    /* =================================================================
     * フェーズ3: 後片付け（クリーンアップ）
     * =================================================================
     * ゲームが終了したら、確保したリソースを順番に解放する。
     * 解放の順番は、初期化の「逆順」にするのが安全な慣例。
     *  初期化: Window → Audio → Textures → BGM → ...
     *  解放:   ... → BGM → Textures → Audio → Window
     *
     * 【Emscriptenの場合】
     * emscripten_set_main_loop() の第3引数が1なので、
     * ここには到達しない（ブラウザがタブを閉じるまで動き続ける）。
     * ブラウザが閉じた時はメモリが自動的に解放される。 */

    /* --- テクスチャの解放 ---
     * GPUメモリ上の16枚分のテクスチャを解放する。 */
    UnloadAllTextures();

    /* --- BGMの解放 ---
     * 正常に読み込まれたBGMストリームのみ解放する。
     * bgmLoaded[i] == false のスロットは読み込み失敗だったため、
     * UnloadMusicStream() を呼ぶと不正メモリアクセスになる可能性がある。 */
    for (int i = 0; i < BGM_COUNT; i++) {
        if (bgmLoaded[i]) {
            UnloadMusicStream(bgm[i]);
            printf("[情報] BGM解放: スロット%d\n", i);
        }
    }

    /* --- 仮想画面の解放 --- */
    UnloadRenderTexture(renderTarget);

    /* --- オーディオデバイスのクローズ ---
     * BGMの解放が終わった後に呼ぶ。順番を逆にすると、
     * オーディオデバイスが閉じた後にBGMを解放しようとしてクラッシュする。 */
    CloseAudioDevice();

    /* --- ウィンドウのクローズ ---
     * OpenGLコンテキストとウィンドウを破棄する。
     * これが最後に呼ぶRaylib関数。この後はRaylib関数を使えない。 */
    CloseWindow();

    /* 正常終了を示す戻り値。0 = 成功。
     * メインの return 0 はC言語の慣例。 */
    return 0;
}
