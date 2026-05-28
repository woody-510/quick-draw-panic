/* =============================================================================
 * render.c — 早撃ちパニック（Quick Draw Panic）描画実装
 * =============================================================================
 * 【このファイルの役割】
 * ゲームのすべての画面を描画するコードを実装する。
 * 画面上に絵を描くことだけを担当し、ゲームのルール（ロジック）には触れない。
 *
 * 【設計方針】
 * ・Painter's Algorithm（画家のアルゴリズム）で奥から手前の順に描画し、
 *   手前の的が奥の的を自然に遮蔽する疑似3D表現を実現する。
 * ・DrawRectangleRounded() やシャドウ効果を多用し、視覚的にリッチな画面を作る。
 * ・すべての座標・サイズは game.h の #define 定数を使い、マジックナンバーを排除。
 *
 * 【画面の種類と担当関数】
 * タイトル → DrawTitleScreen()
 * プレイ中 → DrawPlayingScreen()
 * ポーズ   → DrawPauseScreen()
 * カウントダウン → DrawCountdownScreen()
 * リザルト → DrawResultScreen()
 * スコア一覧 → DrawScoresScreen()
 * ========================================================================== */

#include "game.h"       /* 定数、構造体、グローバル変数の参照 */
#include "render.h"     /* このファイルで実装する関数の宣言 */
#include "logic.h"      /* GetTargetSize, GetTargetRect を使うため */
#include <stdio.h>      /* snprintf — 文字列フォーマット用 */
#include <math.h>       /* sinf, fabsf — パルスアニメーション用 */

/* ==========================================================================
 * 内部ヘルパー関数（staticで外部に公開しない）
 * ==========================================================================
 * これらの関数はこのファイルの中だけで使われる。
 * static を付けることで他のファイルから呼び出せなくなり、
 * 名前の衝突を防ぎ、コードの役割を明確にする。
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * GetButtonRect — ボタンの矩形を計算するヘルパー
 * --------------------------------------------------------------------------
 * 【引数】
 *   index  : 上から何番目のボタンか（0始まり）
 *   startY : 最初のボタンのY座標（ピクセル）
 *
 * 【戻り値】
 *   ボタンの矩形 (x, y, width, height)
 *
 * 【計算の仕組み】
 *   ボタンは画面中央に水平配置する。
 *   x = (画面幅 - ボタン幅) / 2 で中央揃えになる。
 *   y は startY から (ボタン高さ + マージン) × index ずつ下にずらす。
 *
 * 【変更のヒント】
 *   BUTTON_WIDTH / BUTTON_HEIGHT / BUTTON_MARGIN は game.h で定義。
 *   ボタン間隔を広げたければ BUTTON_MARGIN を大きくする。
 * ------------------------------------------------------------------------ */
static Rectangle GetButtonRect(int index, int startY)
{
    Rectangle rect;
    /* ボタンを画面中央に配置するためのX座標計算 */
    rect.x      = (float)(SCREEN_WIDTH - BUTTON_WIDTH) / 2.0f;
    /* 各ボタンは上から順にボタン高さ＋マージン分だけ下にずれる */
    rect.y      = (float)startY + (float)index * (float)(BUTTON_HEIGHT + BUTTON_MARGIN);
    rect.width  = (float)BUTTON_WIDTH;
    rect.height = (float)BUTTON_HEIGHT;
    return rect;
}

/* --------------------------------------------------------------------------
 * IsButtonClicked — ボタンがクリックされたかを判定する
 * --------------------------------------------------------------------------
 * 【引数】
 *   rect : ボタンの矩形（GetButtonRect で取得したもの）
 *
 * 【戻り値】
 *   true  = マウス左ボタンが押された瞬間かつ、カーソルがボタン内にある
 *   false = 上記以外
 *
 * 【注意】
 *   IsMouseButtonPressed は「押された瞬間」のみ true を返す。
 *   押しっぱなしでは false になる。これにより連打が防止される。
 *   CheckCollisionPointRec で点と矩形の当たり判定を行う。
 * ------------------------------------------------------------------------ */
static bool IsButtonClicked(Rectangle rect)
{
    /* マウス左クリックの瞬間かつ、カーソル位置がボタン矩形の中にあるか */
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(GetMousePosition(), rect);
}

/* --------------------------------------------------------------------------
 * DrawButton — 汎用UIボタンの描画
 * --------------------------------------------------------------------------
 * 【引数】
 *   text      : ボタン上に表示するテキスト（英語のみ）
 *   rect      : ボタンの矩形（位置とサイズ）
 *   bgColor   : ボタンの背景色
 *   textColor : テキストの色
 *   hovered   : true=マウスがボタン上にある（ホバーエフェクトを適用）
 *
 * 【描画の仕組み】
 *   1. ホバー時は背景色を明るくする（RGBそれぞれ+30、最大255）
 *   2. ボタンの影を少し下にずらして描画（立体感を演出）
 *   3. 角丸の長方形を DrawRectangleRounded で描画
 *   4. テキストをボタン中央に配置
 *
 * 【フォントサイズ】
 *   26px を使用。ボタン高さ(65px)に対して約40%の比率で見やすい。
 *   フォントサイズを変更する場合は、テキストのY座標計算も調整が必要。
 * ------------------------------------------------------------------------ */
static void DrawButton(const char *text, Rectangle rect, Color bgColor,
                       Color textColor, bool hovered)
{
    /* ホバー時に色を明るくするための計算 */
    /* 各色成分に30を加算するが、255を超えないようにする */
    Color drawColor = bgColor;
    if (hovered) {
        /* 明るさを加算（unsigned charのオーバーフロー防止） */
        drawColor.r = (unsigned char)((bgColor.r + 30 > 255) ? 255 : bgColor.r + 30);
        drawColor.g = (unsigned char)((bgColor.g + 30 > 255) ? 255 : bgColor.g + 30);
        drawColor.b = (unsigned char)((bgColor.b + 30 > 255) ? 255 : bgColor.b + 30);
    }

    /* --- ボタンのドロップシャドウ（影） ---
     * 本体より3ピクセル下に、半透明の暗い角丸矩形を描く。
     * これにより「ボタンが浮いている」ように見える立体感が出る。
     * 角の丸みは 0.3f（矩形の短辺の30%を半径とする）。 */
    Rectangle shadowRect = { rect.x + 2.0f, rect.y + 3.0f, rect.width, rect.height };
    DrawRectangleRounded(shadowRect, 0.3f, 8, (Color){ 0, 0, 0, 80 });

    /* --- ボタン本体の描画 ---
     * 角丸の長方形。segmentsは8でなめらかに見える。
     * segmentsを増やすとさらに滑らかになるが描画負荷も増える。 */
    DrawRectangleRounded(rect, 0.3f, 8, drawColor);

    /* --- テキストの中央配置 ---
     * MeasureText でテキストの描画幅を測り、
     * ボタンの中央に来るように x = rect.x + (rect.width - textWidth) / 2 で計算。
     * y は rect.y + (rect.height - fontSize) / 2 でボタンの縦中央に配置。
     * フォントサイズ26は、BUTTON_HEIGHT=65 に対してバランスの良いサイズ。 */
    int fontSize = 26;
    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(rect.x + (rect.width - (float)textWidth) / 2.0f);
    int textY = (int)(rect.y + (rect.height - (float)fontSize) / 2.0f);

    /* テキストの影（1px下・右にずらした暗い文字で奥行き感を出す） */
    DrawText(text, textX + 1, textY + 1, fontSize, (Color){ 0, 0, 0, 100 });
    /* テキスト本体 */
    DrawText(text, textX, textY, fontSize, textColor);
}

/* --------------------------------------------------------------------------
 * DrawSingleTarget — 1個の的を描画する
 * --------------------------------------------------------------------------
 * 【引数】
 *   row  : 行番号（0=最手前, 5=最奥）
 *   lane : レーン番号（0=左, 1=中央, 2=右）
 *
 * 【描画の仕組み】
 *   ・GetTargetRect() で的の描画位置とサイズを取得する。
 *   ・的が非アクティブ（isActive==false）なら何も描画しない。
 *   ・正解の的（isCorrect）: テクスチャ画像を角丸枠内に描画。
 *   ・空欄の的（!isCorrect）: ステージ色の×マークを描画。
 *
 * 【正解の的の描画手順】
 *   1. 影（ドロップシャドウ）を少し下に描画 → 浮遊感
 *   2. 角丸の枠（背景より少し明るい色）→ 的の縁取り
 *   3. テクスチャ画像を DrawTexturePro で矩形にフィットさせる
 *
 * 【空欄の的の描画手順】
 *   1. 半透明のステージ色で角丸矩形を塗りつぶす
 *   2. 対角線2本で×マークを描画
 *   3. 線の太さは的のサイズに比例させる（大きい的=太い線）
 *
 * 【変更時の注意】
 *   的のサイズ計算は logic.c の GetTargetSize / GetTargetRect に依存。
 *   ここを変えても的の位置・サイズは変わらない。
 * ------------------------------------------------------------------------ */
static void DrawSingleTarget(int row, int lane)
{
    /* 的の描画矩形を取得（位置とサイズ） */
    Rectangle rect = GetTargetRect(row, lane);

    /* 的のデータを取得 */
    Target t = game.targets[row][lane];

    /* 非アクティブな的は描画しない */
    if (!t.isActive) return;

    /* --- ステージカラーの取得 ---
     * game.stage は 1始まり（1〜16）なので、配列のインデックスは stage-1 を使う。
     * 範囲外アクセス防止のため 0〜15 にクランプする。 */
    int stageIdx = game.stage - 1;
    if (stageIdx < 0) stageIdx = 0;
    if (stageIdx >= TOTAL_STAGES) stageIdx = TOTAL_STAGES - 1;

    Color bgColor    = STAGE_BG_COLORS[stageIdx];
    Color blankColor = STAGE_BLANK_COLORS[stageIdx];

    /* --- 角丸の丸み係数 ---
     * 的のサイズが小さい（奥の行）ほど丸みの割合は同じでも絶対値が小さくなり、
     * 自然に見える。0.15f は角がほんのり丸い程度。 */
    float roundness = 0.15f;
    int segments = 6;   /* 角丸の滑らかさ（セグメント数） */

    if (t.isCorrect) {
        /* ==================== 正解の的（テクスチャ画像を表示） ==================== */

        /* --- ドロップシャドウ ---
         * 的の下に薄い黒の矩形を描画し、的が浮いて見える効果を出す。
         * オフセットは3px下・2px右。的が小さいほどシャドウも薄くする。 */
        Rectangle shadowRect = {
            rect.x + 2.0f,         /* 右に2pxずらす */
            rect.y + 3.0f,         /* 下に3pxずらす */
            rect.width,
            rect.height
        };
        /* 奥の行ほど影を薄くする（rowが大きいほどアルファ値が小さい） */
        int shadowAlpha = 100 - row * 12;
        if (shadowAlpha < 30) shadowAlpha = 30;     /* 最低でも30の薄影は残す */
        DrawRectangleRounded(shadowRect, roundness, segments,
                             (Color){ 0, 0, 0, (unsigned char)shadowAlpha });

        /* --- 枠（ボーダー） ---
         * テクスチャの周囲に少し大きい角丸矩形を描いて「額縁」効果を出す。
         * 背景色を少し明るくした色を使い、的が背景から浮き出て見えるようにする。 */
        float borderSize = 3.0f;    /* 枠の太さ（ピクセル） */
        Rectangle borderRect = {
            rect.x - borderSize,
            rect.y - borderSize,
            rect.width + borderSize * 2.0f,
            rect.height + borderSize * 2.0f
        };
        /* 枠色: 背景色を基にやや白混ぜ */
        Color borderColor = {
            (unsigned char)((bgColor.r + 200) / 2),
            (unsigned char)((bgColor.g + 200) / 2),
            (unsigned char)((bgColor.b + 200) / 2),
            220
        };
        DrawRectangleRounded(borderRect, roundness, segments, borderColor);

        /* --- テクスチャ描画 ---
         * DrawTexturePro を使って、テクスチャを的の矩形にぴったりフィットさせる。
         *
         * source: テクスチャの読み取り範囲（テクスチャ全体を使う）
         * dest  : 画面上の描画先矩形（GetTargetRect で計算した位置とサイズ）
         * origin: 回転の中心点（回転しないので {0,0}）
         * rotation: 回転角度（0 = 回転なし）
         * tint  : 色合い（WHITE = そのまま表示） */
        Texture2D tex = textures[t.imageIndex];
        Rectangle source = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
        Rectangle dest   = { rect.x, rect.y, rect.width, rect.height };
        Vector2 origin   = { 0.0f, 0.0f };
        DrawTexturePro(tex, source, dest, origin, 0.0f, WHITE);
    }
    else {
        /* ==================== 空欄の的（×マークを表示） ==================== */

        /* --- 半透明の角丸矩形 ---
         * blankColor のアルファ値を 180 にして半透明にする。
         * 完全に不透明だと重苦しく見えるが、半透明にすると背景が透けて奥行き感が出る。 */
        Color fillColor = { blankColor.r, blankColor.g, blankColor.b, 180 };
        DrawRectangleRounded(rect, roundness, segments, fillColor);

        /* --- ×マーク（対角線2本） ---
         * 矩形の四隅を結ぶ2本の対角線で×マークを作る。
         *
         * 左上(x1,y1) ──── 右上(x2,y1)
         *        ╲        ╱
         *         ╲      ╱
         *          ╲    ╱
         *         ╱      ╲
         *        ╱        ╲
         * 左下(x1,y2) ──── 右下(x2,y2)
         *
         * 内側にマージンを取ることで、矩形の端ギリギリではなく
         * 少し内側に×マークが描かれ、見栄えが良くなる。 */
        float margin = rect.width * 0.15f;  /* 矩形サイズの15%を内側マージンに */
        float x1 = rect.x + margin;         /* 左端（内側） */
        float y1 = rect.y + margin;          /* 上端（内側） */
        float x2 = rect.x + rect.width - margin;   /* 右端（内側） */
        float y2 = rect.y + rect.height - margin;   /* 下端（内側） */

        /* ×マークの線の太さ: 的のサイズに比例させる
         * TARGET_SIZE_FRONT=120 のとき約3px、TARGET_SIZE_BACK=40のとき約1px。
         * 最低でも1pxは確保する。 */
        float lineThickness = rect.width * 0.025f;
        if (lineThickness < 1.0f) lineThickness = 1.0f;

        /* ×マーク用の色: blankColor をそのまま使うがアルファを少し上げて見やすく */
        Color xColor = { blankColor.r, blankColor.g, blankColor.b, 220 };

        /* 対角線1: 左上 → 右下 */
        DrawLineEx((Vector2){ x1, y1 }, (Vector2){ x2, y2 }, lineThickness, xColor);
        /* 対角線2: 右上 → 左下 */
        DrawLineEx((Vector2){ x2, y1 }, (Vector2){ x1, y2 }, lineThickness, xColor);
    }
}

/* --------------------------------------------------------------------------
 * DrawAllTargets — すべての的を描画する（Painter's Algorithm）
 * --------------------------------------------------------------------------
 * 【描画順序の重要性】
 *   2Dゲームでは後に描画したものが前に表示される（上書き）。
 *   これを利用し、奥の的（row=5）から先に描き、
 *   手前の的（row=0）を最後に描くことで、
 *   手前の的が奥の的を自然に遮蔽する疑似3D効果が得られる。
 *
 *   この技法を「Painter's Algorithm（画家のアルゴリズム）」と呼ぶ。
 *   画家がキャンバスに絵を描くとき、遠景から描いて近景で上書きするのと同じ。
 *
 * 【ループの向き】
 *   row を MAX_VISIBLE_ROWS-1（=5、最奥）から 0（最手前）に向かって減少。
 *   各行のレーンは左(0)から右(2)の順で描画（レーン間の重なりは無いため順不同）。
 * ------------------------------------------------------------------------ */
static void DrawAllTargets(void)
{
    /* 奥の行（row=5）から手前の行（row=0）に向かって描画 */
    for (int row = MAX_VISIBLE_ROWS - 1; row >= 0; row--) {
        /* 各行の3レーンを左から右に描画 */
        for (int lane = 0; lane < LANE_COUNT; lane++) {
            DrawSingleTarget(row, lane);
        }
    }
}

/* --------------------------------------------------------------------------
 * FormatTime — ミリ秒値を "MM:SS.mmm" 形式にフォーマットするヘルパー
 * --------------------------------------------------------------------------
 * 【引数】
 *   buf     : フォーマット結果を書き込むバッファ
 *   bufSize : バッファサイズ（バイト数）。snprintf の安全な最大書き込み量。
 *   ms      : 時間のミリ秒値（整数）
 *
 * 【フォーマット】
 *   分:秒.ミリ秒（3桁）で表記する。
 *   例: 75123ms → "01:15.123"
 *
 * 【計算】
 *   ms から 分・秒・ミリ秒 を分離する:
 *     分   = ms / 60000
 *     秒   = (ms % 60000) / 1000
 *     ミリ = ms % 1000
 * ------------------------------------------------------------------------ */
static void FormatTime(char *buf, int bufSize, int ms)
{
    int min  = ms / 60000;          /* 分の部分 */
    int sec  = (ms % 60000) / 1000; /* 秒の部分 */
    int msec = ms % 1000;           /* ミリ秒の部分 */
    /* %02d = 2桁ゼロ埋め、%03d = 3桁ゼロ埋め */
    snprintf(buf, (size_t)bufSize, "%02d:%02d.%03d", min, sec, msec);
}

/* ==========================================================================
 * 公開描画関数（render.h で宣言されている関数の実装）
 * ==========================================================================
 * 以下の関数は main.c の描画ループから呼ばれる。
 * 各関数は BeginDrawing()〜EndDrawing() の間で呼ばれることが前提。
 * ClearBackground() は呼び出し元が行うため、ここでは行わない。
 * ========================================================================== */

/* --------------------------------------------------------------------------
 * DrawTitleScreen — タイトル画面の描画
 * --------------------------------------------------------------------------
 * 【表示内容】
 *   1. 背景: 暗めのグラデーション風（上が暗く下がやや明るい）
 *   2. スライドショー: 的の画像が順番にフェードイン/アウト（半透明で表示）
 *   3. タイトルロゴ: "QUICK DRAW PANIC" のテキスト（影付き）
 *   4. モード選択ボタン: TIME ATTACK / SURVIVAL / MARATHON
 *   5. ランキングボタン: RANKINGS
 *
 * 【レイアウト】
 *   y=0〜100    : タイトル領域（ロゴ）
 *   y=100〜280  : サブタイトル
 *   y=300〜560  : モードボタン3つ（各85px間隔）
 *   y=620       : ランキングボタン
 * ------------------------------------------------------------------------ */
void DrawTitleScreen(void)
{
    /* === 1. 背景グラデーション ===
     * 純粋な黒背景だと味気ないので、上から下へ少しだけ明るくなる
     * グラデーション風にする。DrawRectangle を複数重ねて近似表現。
     * ※本格的なグラデーションはシェーダーが必要だが、ここでは簡易版で十分。 */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 18, 18, 28, 255 });
    /* 下半分をやや明るくして微妙なグラデーション効果 */
    DrawRectangle(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2,
                  (Color){ 25, 25, 40, 255 });

    /* === 2. スライドショー ===
     * 的の画像が大きく半透明で表示され、一定間隔でフェードしながら切り替わる。
     * game.slideAlpha はロジック側（UpdateTitle）で 0.0〜1.0 に制御される。
     * Fade() で WHITE にアルファを掛けると半透明描画になる。
     *
     * 画像の配置: 画面中央、200x200px の正方形。
     * 大きすぎるとボタンが見えなくなるが、小さすぎると存在感が薄い。
     * y=170 付近に配置するとタイトルとボタンの間にバランス良く収まる。 */
    {
        /* スライドショー画像のインデックスが有効範囲か確認 */
        int idx = game.slideIndex;
        if (idx < 0) idx = 0;
        if (idx >= TOTAL_STAGES) idx = 0;

        Texture2D tex = textures[idx];
        /* テクスチャが有効かチェック（id > 0 で判定） */
        if (tex.id > 0) {
            /* スライドショー画像のサイズと位置 */
            float slideSize = 180.0f;   /* 表示サイズ（正方形） */
            float slideX = ((float)SCREEN_WIDTH - slideSize) / 2.0f;  /* 中央揃え */
            float slideY = 175.0f;      /* タイトル下、ボタン上の中間位置 */

            /* テクスチャのソース矩形（テクスチャ全体）とデスト矩形（画面上の位置） */
            Rectangle src  = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
            Rectangle dest = { slideX, slideY, slideSize, slideSize };
            Vector2 origin = { 0.0f, 0.0f };

            /* Fade(WHITE, alpha) で半透明にして描画。
             * slideAlpha が 1.0 で完全不透明、0.0 で完全透明。
             * 0.35f を掛けてさらに薄くし、背景に溶け込むようにする。 */
            float displayAlpha = game.slideAlpha * 0.35f;
            DrawTexturePro(tex, src, dest, origin, 0.0f,
                           Fade(WHITE, displayAlpha));
        }
    }

    /* === 3. タイトルロゴ ===
     * "QUICK DRAW PANIC" のテキストを大きく表示。
     * 影テキストを先に描き、本体テキストを上に重ねることで立体感を出す。 */
    {
        const char *titleText = "QUICK DRAW PANIC";
        int titleSize = 42;     /* フォントサイズ。40以上で目立つ。 */
        int titleWidth = MeasureText(titleText, titleSize);
        int titleX = (SCREEN_WIDTH - titleWidth) / 2;   /* 画面中央 */
        int titleY = 80;        /* 画面上部に配置 */

        /* 影テキスト（2px右下にずらし、暗い色で描画） */
        DrawText(titleText, titleX + 2, titleY + 2, titleSize, (Color){ 0, 0, 0, 180 });
        /* 本体テキスト（明るい色で上に重ねる） */
        DrawText(titleText, titleX, titleY, titleSize, (Color){ 255, 220, 60, 255 });

        /* サブタイトル（英語表記） */
        const char *subText = "- Hayauchi Panic -";
        int subSize = 20;
        int subWidth = MeasureText(subText, subSize);
        int subX = (SCREEN_WIDTH - subWidth) / 2;
        int subY = titleY + titleSize + 10;     /* タイトルの直下 */
        DrawText(subText, subX + 1, subY + 1, subSize, (Color){ 0, 0, 0, 120 });
        DrawText(subText, subX, subY, subSize, (Color){ 200, 200, 220, 255 });
    }

    /* === 4. モード選択ボタン ===
     * 3つのモードボタンを縦に並べる。
     * 各ボタンにはモード固有の色を使い、視覚的に区別しやすくする。
     * マウスホバーで色が少し明るくなるフィードバック付き。 */
    {
        /* ボタン開始Y座標。タイトルとサブタイトルの下、画面の中央付近に配置。 */
        int btnStartY = 380;
        Vector2 mousePos = GetMousePosition();

        /* --- TIME ATTACK ボタン ---
         * 爽やかな青色。速度を競うモードなのでクールな印象。 */
        Rectangle btnTA = GetButtonRect(0, btnStartY);
        bool hoveredTA = CheckCollisionPointRec(mousePos, btnTA);
        DrawButton("TIME ATTACK", btnTA, (Color){ 50, 100, 200, 255 }, WHITE, hoveredTA);

        /* --- SURVIVAL ボタン ---
         * 力強い赤色。生き残りを賭けた緊張感のあるモード。 */
        Rectangle btnSV = GetButtonRect(1, btnStartY);
        bool hoveredSV = CheckCollisionPointRec(mousePos, btnSV);
        DrawButton("SURVIVAL", btnSV, (Color){ 200, 50, 50, 255 }, WHITE, hoveredSV);

        /* --- MARATHON ボタン ---
         * 持久力を象徴する緑色。長時間プレイのモード。 */
        Rectangle btnMR = GetButtonRect(2, btnStartY);
        bool hoveredMR = CheckCollisionPointRec(mousePos, btnMR);
        DrawButton("MARATHON", btnMR, (Color){ 40, 170, 60, 255 }, WHITE, hoveredMR);
    }

    /* === 5. ランキングボタン ===
     * モードボタンの下に少し離して配置。
     * ややグレー寄りの落ち着いた色で、メインボタンとの差別化を図る。 */
    {
        Rectangle btnRank = GetButtonRect(0, 650);
        Vector2 mousePos = GetMousePosition();
        bool hoveredRank = CheckCollisionPointRec(mousePos, btnRank);
        DrawButton("RANKINGS", btnRank, (Color){ 80, 80, 100, 255 }, WHITE, hoveredRank);
    }
}

/* --------------------------------------------------------------------------
 * DrawPlayingScreen — ゲームプレイ画面の描画
 * --------------------------------------------------------------------------
 * 【表示内容】
 *   1. 背景: 現在のステージ背景色で塗りつぶし
 *   2. 的: DrawAllTargets() で疑似3D表示
 *   3. HUD: ステージ名、タイマー/スコア、ゲージ（マラソン）、ポーズボタン
 *   4. エフェクト: ミス時の白フラッシュ
 *
 * 【HUDの配置】
 *   左上  : "Stage N - ThemeName"
 *   中央上: モード別表示（タイム / スコア）
 *   右上  : ポーズボタン（‖）
 *   ステージ名下: ゲージバー（マラソンのみ）
 * ------------------------------------------------------------------------ */
void DrawPlayingScreen(void)
{
    /* === 1. 背景色の塗りつぶし ===
     * ステージ番号（1〜16）に対応する背景色で画面全体を塗る。
     * game.stage は1始まりなので、配列インデックスは stage-1。 */
    int stageIdx = game.stage - 1;
    if (stageIdx < 0) stageIdx = 0;
    if (stageIdx >= TOTAL_STAGES) stageIdx = TOTAL_STAGES - 1;
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, STAGE_BG_COLORS[stageIdx]);

    /* === 2. 的の描画 ===
     * Painter's Algorithm で奥から手前に描画する。 */
    DrawAllTargets();

    /* === 3. HUD（ヘッドアップディスプレイ）の描画 ===
     * ゲーム中に常時表示する情報。読みやすさのために影テキストを併用。 */

    /* --- 3a. ステージ名（左上） ---
     * "Stage N - テーマ名" の形式で表示する。
     * 影テキストで背景色に関わらず読みやすくする。 */
    {
        char stageBuf[64];
        /* ステージ番号とテーマ名を結合 */
        snprintf(stageBuf, sizeof(stageBuf), "Stage %d - %s",
                 game.stage, STAGE_NAMES[stageIdx]);
        int stageSize = 20;
        /* 影（1px右下にオフセット） */
        DrawText(stageBuf, 11, 11, stageSize, (Color){ 0, 0, 0, 160 });
        /* 本体 */
        DrawText(stageBuf, 10, 10, stageSize, WHITE);
    }

    /* --- 3b. モード別の中央表示 --- */
    {
        char hudBuf[64];
        int hudSize = 22;

        if (game.mode == MODE_TIMEATTACK) {
            /* タイムアタック: 経過タイム "MM:SS.mm" と残り枚数を表示 */

            /* 経過時間をフォーマット */
            int elapsedMs = (int)(game.elapsedTime * 1000.0f);
            char timeBuf[32];
            FormatTime(timeBuf, sizeof(timeBuf), elapsedMs);

            /* 残り枚数 = 目標枚数 - 撃ち抜いた枚数 */
            int remaining = TIMEATTACK_GOAL - game.score;
            if (remaining < 0) remaining = 0;

            snprintf(hudBuf, sizeof(hudBuf), "%s  Left:%d", timeBuf, remaining);
        }
        else if (game.mode == MODE_SURVIVAL) {
            /* サバイバル: シンプルにスコア（撃ち抜いた枚数）を表示 */
            snprintf(hudBuf, sizeof(hudBuf), "Score: %d", game.score);
        }
        else {
            /* マラソン: スコアを表示（ゲージは別途描画） */
            snprintf(hudBuf, sizeof(hudBuf), "Score: %d", game.score);
        }

        /* HUDテキストを画面上部中央に配置 */
        int hudWidth = MeasureText(hudBuf, hudSize);
        int hudX = (SCREEN_WIDTH - hudWidth) / 2;
        int hudY = 40;
        /* 影 */
        DrawText(hudBuf, hudX + 1, hudY + 1, hudSize, (Color){ 0, 0, 0, 160 });
        /* 本体 */
        DrawText(hudBuf, hudX, hudY, hudSize, WHITE);
    }

    /* --- 3c. マラソンモードのゲージバー ---
     * ゲージが残っている間はゲームが続行する。
     * 0になるとゲームオーバー。
     *
     * ゲージの色:
     *   50%以上 → 緑（安全）
     *   25〜50% → 黄（注意）
     *   25%未満 → 赤（危険）＋パルスエフェクト
     *
     * 【パルスエフェクト】
     *   ゲージが25%を切ると、sinf() で明滅させて焦りを演出する。
     *   sinf(GetTime() * 8.0) の値が -1〜1 で振動し、
     *   fabsf() で 0〜1 にした後、アルファ値に変換する。 */
    if (game.mode == MODE_MARATHON) {
        /* ゲージバーの位置とサイズ */
        float gaugeX = 30.0f;       /* 左端マージン */
        float gaugeY = 75.0f;       /* ステージ名とHUDの下 */
        float gaugeW = 340.0f;      /* ゲージの最大幅（ほぼ画面幅） */
        float gaugeH = 18.0f;       /* ゲージの高さ */

        /* ゲージの背景（暗いグレーの枠） */
        DrawRectangleRounded(
            (Rectangle){ gaugeX - 1.0f, gaugeY - 1.0f, gaugeW + 2.0f, gaugeH + 2.0f },
            0.3f, 4, (Color){ 30, 30, 30, 200 }
        );

        /* ゲージの充填割合を計算（0.0〜1.0） */
        float ratio = 0.0f;
        if (game.gaugeMax > 0.0f) {
            ratio = game.gaugeValue / game.gaugeMax;
        }
        /* 0.0〜1.0 にクランプ（万が一の負の値や超過を防止） */
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        /* ゲージの色を残量に応じて決定 */
        Color gaugeColor;
        if (ratio > 0.5f) {
            /* 50%以上: 安全の緑 */
            gaugeColor = (Color){ 50, 205, 50, 255 };
        }
        else if (ratio > 0.25f) {
            /* 25〜50%: 注意の黄色 */
            gaugeColor = (Color){ 255, 200, 0, 255 };
        }
        else {
            /* 25%未満: 危険の赤 ＋ パルス（明滅）エフェクト */
            /* sinf() の値を 0.0〜1.0 に変換し、アルファ値を 150〜255 で振動させる */
            float pulse = fabsf(sinf((float)GetTime() * 8.0f));
            unsigned char alpha = (unsigned char)(150.0f + pulse * 105.0f);
            gaugeColor = (Color){ 220, 40, 40, alpha };
        }

        /* ゲージ前景（充填部分）を描画 */
        float fillWidth = gaugeW * ratio;
        if (fillWidth > 0.5f) {     /* ほぼゼロのときは描画しない（角丸が崩れるため） */
            DrawRectangleRounded(
                (Rectangle){ gaugeX, gaugeY, fillWidth, gaugeH },
                0.3f, 4, gaugeColor
            );
        }
    }

    /* --- 3d. ポーズボタン（右上の ‖ アイコン） ---
     * PAUSE_BTN_X, PAUSE_BTN_Y, PAUSE_BTN_SIZE は game.h で定義。
     * 2本の縦線で「‖」（一時停止）マークを描画する。 */
    {
        /* ボタンの背景（半透明の暗い角丸矩形） */
        Rectangle pauseRect = {
            (float)PAUSE_BTN_X,
            (float)PAUSE_BTN_Y,
            (float)PAUSE_BTN_SIZE,
            (float)PAUSE_BTN_SIZE
        };
        DrawRectangleRounded(pauseRect, 0.25f, 6, (Color){ 0, 0, 0, 120 });

        /* ポーズアイコン: 2本の縦バー
         * ボタン内に等間隔で2本の太い線を描く。
         * 各バーの幅は PAUSE_BTN_SIZE の 15%、高さは 55%。 */
        float barW = (float)PAUSE_BTN_SIZE * 0.15f;    /* バーの幅 */
        float barH = (float)PAUSE_BTN_SIZE * 0.55f;    /* バーの高さ */
        float barY = (float)PAUSE_BTN_Y + ((float)PAUSE_BTN_SIZE - barH) / 2.0f;
        /* 左バーのX座標: ボタン中央から少し左 */
        float barX1 = (float)PAUSE_BTN_X + (float)PAUSE_BTN_SIZE * 0.30f;
        /* 右バーのX座標: ボタン中央から少し右 */
        float barX2 = (float)PAUSE_BTN_X + (float)PAUSE_BTN_SIZE * 0.55f;

        DrawRectangleRounded(
            (Rectangle){ barX1, barY, barW, barH },
            0.2f, 4, WHITE
        );
        DrawRectangleRounded(
            (Rectangle){ barX2, barY, barW, barH },
            0.2f, 4, WHITE
        );
    }

    /* === 4. ミスフラッシュエフェクト ===
     * ミスした瞬間に画面全体を白く光らせるエフェクト。
     * game.flashTimer が MISS_FLASH_TIME から 0 に向かって減少する間、
     * 白い半透明の矩形を画面全体に重ねる。
     *
     * アルファ値は flashTimer に比例して計算:
     *   flashTimer = MISS_FLASH_TIME のとき → alpha = 200（かなり白い）
     *   flashTimer = 0 のとき → alpha = 0（完全に透明=消える）
     * この比例計算により、フラッシュが滑らかに消えていく。 */
    if (game.flashTimer > 0.0f) {
        /* flashTimer / MISS_FLASH_TIME で 0.0〜1.0 の割合を得て、200を掛ける */
        float alphaRatio = game.flashTimer / MISS_FLASH_TIME;
        if (alphaRatio > 1.0f) alphaRatio = 1.0f;  /* 安全クランプ */
        unsigned char flashAlpha = (unsigned char)(200.0f * alphaRatio);
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                      (Color){ 255, 255, 255, flashAlpha });
    }
}

/* --------------------------------------------------------------------------
 * DrawPauseScreen — ポーズ画面の描画
 * --------------------------------------------------------------------------
 * 【表示内容】
 *   1. 背景: ゲームプレイ画面をそのまま描画（停止中の状態がわかる）
 *   2. 半透明黒オーバーレイ（暗くして「一時停止中」を演出）
 *   3. "PAUSED" テキスト
 *   4. RESUME / RETRY / TITLE の3ボタン
 *
 * 【設計意図】
 *   ポーズ画面は「ゲーム画面の上にオーバーレイを被せる」方式。
 *   プレイヤーがゲームの状況を確認しつつ、選択肢を選べるようにする。
 *   オーバーレイのアルファ値180は、背景が薄っすら見えつつ
 *   テキストやボタンが十分に目立つバランス。
 * ------------------------------------------------------------------------ */
void DrawPauseScreen(void)
{
    /* === 1. 背景にゲーム画面を描画 ===
     * プレイ画面を一度描画してから、その上にオーバーレイを重ねる。
     * DrawPlayingScreen() はゲーム状態を変更しないので安全に呼べる。 */
    DrawPlayingScreen();

    /* === 2. 半透明黒オーバーレイ ===
     * alpha=180 で画面全体を暗くする。
     * 0=完全透明（意味なし）、255=完全黒（ゲーム画面が見えない）。
     * 180は「ゲーム画面がうっすら見える」良いバランス。 */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 0, 0, 0, 180 });

    /* === 3. "PAUSED" テキスト ===
     * 画面上部中央に大きく表示。影付きで視認性を確保。 */
    {
        const char *pauseText = "PAUSED";
        int pauseSize = 52;
        int pauseWidth = MeasureText(pauseText, pauseSize);
        int pauseX = (SCREEN_WIDTH - pauseWidth) / 2;
        int pauseY = 180;

        /* 影テキスト */
        DrawText(pauseText, pauseX + 2, pauseY + 2, pauseSize, (Color){ 0, 0, 0, 200 });
        /* 本体テキスト */
        DrawText(pauseText, pauseX, pauseY, pauseSize, WHITE);
    }

    /* === 4. ボタン3つ ===
     * RESUME: ゲームを再開（カウントダウンを経て再開）
     * RETRY : ゲームを最初からやり直す
     * TITLE : タイトル画面に戻る */
    {
        int btnStartY = 300;
        Vector2 mousePos = GetMousePosition();

        /* RESUME ボタン（再開 → 緑色で「安全」を表現） */
        Rectangle btnResume = GetButtonRect(0, btnStartY);
        bool hResume = CheckCollisionPointRec(mousePos, btnResume);
        DrawButton("RESUME", btnResume, (Color){ 40, 170, 60, 255 }, WHITE, hResume);

        /* RETRY ボタン（やり直し → 青色で「リフレッシュ」を表現） */
        Rectangle btnRetry = GetButtonRect(1, btnStartY);
        bool hRetry = CheckCollisionPointRec(mousePos, btnRetry);
        DrawButton("RETRY", btnRetry, (Color){ 50, 100, 200, 255 }, WHITE, hRetry);

        /* TITLE ボタン（戻る → グレーで「中断」を表現） */
        Rectangle btnTitle = GetButtonRect(2, btnStartY);
        bool hTitle = CheckCollisionPointRec(mousePos, btnTitle);
        DrawButton("TITLE", btnTitle, (Color){ 100, 100, 110, 255 }, WHITE, hTitle);
    }
}

/* --------------------------------------------------------------------------
 * DrawCountdownScreen — カウントダウン画面の描画
 * --------------------------------------------------------------------------
 * 【表示内容】
 *   1. 背景: ゲームプレイ画面（停止状態）
 *   2. 半透明オーバーレイ（ポーズより薄い → プレイ画面を見やすく）
 *   3. カウントダウン数字（3, 2, 1）を画面中央に大きく表示
 *
 * 【パルスエフェクト】
 *   countdownTimer の値に応じて数字のサイズを微妙に変動させ、
 *   「ポン！」というリズム感を視覚的に表現する。
 *   タイマーが COUNTDOWN_INTERVAL に近い（数字が出た直後）ときに大きく、
 *   0に近い（次の数字に切り替わる直前）ときに通常サイズに戻る。
 * ------------------------------------------------------------------------ */
void DrawCountdownScreen(void)
{
    /* === 1. ゲーム画面を背景に描画 === */
    DrawPlayingScreen();

    /* === 2. 半透明オーバーレイ ===
     * alpha=120 でポーズ画面（180）より薄い。
     * カウントダウン中はプレイ画面を確認できるようにする意図。 */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 0, 0, 0, 120 });

    /* === 3. カウントダウン数字 ===
     * game.countdownNumber は 3→2→1 と遷移する。
     * 数字を 120pt 以上の巨大フォントで画面中央に描画する。 */
    {
        /* 数字を文字列に変換 */
        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "%d", game.countdownNumber);

        /* パルスエフェクトの計算:
         * countdownTimer は COUNTDOWN_INTERVAL(1.0秒) → 0.0 に減少。
         * ratio = timer / interval → 1.0（出現直後）→ 0.0（消える直前）
         * パルス = ratio * 0.3 で最大30%の拡大効果を加える。 */
        float ratio = game.countdownTimer / COUNTDOWN_INTERVAL;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        /* 基本フォントサイズ 120 に パルス分（最大36px）を加算 */
        int baseSize = 120;
        int fontSize = baseSize + (int)(ratio * 36.0f);

        /* 数字を画面中央に配置 */
        int textWidth = MeasureText(numBuf, fontSize);
        int textX = (SCREEN_WIDTH - textWidth) / 2;
        int textY = (SCREEN_HEIGHT - fontSize) / 2 - 30;   /* やや上寄りに */

        /* 影テキスト（大きいので影オフセットも大きめ） */
        DrawText(numBuf, textX + 3, textY + 3, fontSize, (Color){ 0, 0, 0, 200 });
        /* 本体テキスト（純白で目立たせる） */
        DrawText(numBuf, textX, textY, fontSize, WHITE);
    }
}

/* --------------------------------------------------------------------------
 * DrawResultScreen — リザルト画面の描画
 * --------------------------------------------------------------------------
 * 【表示内容】
 *   1. 背景: 暗めの色（最後のステージ背景色を暗くしたもの）
 *   2. モード名（画面上部）
 *   3. スコア（画面中央に大きく表示）
 *      - タイムアタック: "MM:SS.mmm" 形式。タイムアウト時は "TIME UP!"
 *      - サバイバル: "Score: XX" 形式
 *      - マラソン: "MM:SS.mmm" 形式（生存時間）
 *   4. NEW RECORD!! 表示（新記録時、金色で明滅）
 *   5. タイムアウト表示（赤文字）
 *   6. "Tap to continue"（画面下部、フェードイン/アウト）
 * ------------------------------------------------------------------------ */
void DrawResultScreen(void)
{
    /* === 1. 背景 ===
     * 最後にプレイしていたステージの背景色を暗めにして使う。
     * RGBを各50%に落とすことで落ち着いた雰囲気を出す。 */
    int stageIdx = game.stage - 1;
    if (stageIdx < 0) stageIdx = 0;
    if (stageIdx >= TOTAL_STAGES) stageIdx = TOTAL_STAGES - 1;

    Color baseBg = STAGE_BG_COLORS[stageIdx];
    Color darkBg = {
        (unsigned char)(baseBg.r / 2),
        (unsigned char)(baseBg.g / 2),
        (unsigned char)(baseBg.b / 2),
        255
    };
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, darkBg);
    /* さらに上から半透明の黒を重ねて統一的な暗さにする */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 0, 0, 0, 100 });

    /* === 2. モード名（画面上部） ===
     * どのモードの結果かわかるように、モード名を表示する。 */
    {
        const char *modeName = "";
        if (game.mode == MODE_TIMEATTACK) modeName = "TIME ATTACK";
        else if (game.mode == MODE_SURVIVAL) modeName = "SURVIVAL";
        else modeName = "MARATHON";

        int modeSize = 30;
        int modeWidth = MeasureText(modeName, modeSize);
        int modeX = (SCREEN_WIDTH - modeWidth) / 2;
        int modeY = 120;
        DrawText(modeName, modeX + 1, modeY + 1, modeSize, (Color){ 0, 0, 0, 150 });
        DrawText(modeName, modeX, modeY, modeSize, (Color){ 180, 180, 220, 255 });
    }

    /* === 3. スコア表示（画面中央） === */
    {
        char scoreBuf[64];
        int scoreSize = 48;

        if (game.mode == MODE_TIMEATTACK) {
            if (game.isTimedOut) {
                /* タイムアウト: 時間内に目標達成できなかった場合 */
                snprintf(scoreBuf, sizeof(scoreBuf), "TIME UP!");
            }
            else {
                /* 正常クリア: 経過時間を MM:SS.mmm 形式で表示 */
                int ms = (int)(game.elapsedTime * 1000.0f);
                FormatTime(scoreBuf, sizeof(scoreBuf), ms);
            }
        }
        else if (game.mode == MODE_SURVIVAL) {
            /* サバイバル: 撃ち抜いた枚数 */
            snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", game.score);
        }
        else {
            /* マラソン: 生存時間を MM:SS.mmm 形式で表示 */
            int ms = (int)(game.elapsedTime * 1000.0f);
            FormatTime(scoreBuf, sizeof(scoreBuf), ms);
        }

        /* スコアテキストを画面中央に配置 */
        int scoreWidth = MeasureText(scoreBuf, scoreSize);
        int scoreX = (SCREEN_WIDTH - scoreWidth) / 2;
        int scoreY = 280;

        /* 影テキスト */
        DrawText(scoreBuf, scoreX + 2, scoreY + 2, scoreSize, (Color){ 0, 0, 0, 200 });
        /* 本体テキスト（明るい白で大きく目立たせる） */
        DrawText(scoreBuf, scoreX, scoreY, scoreSize, WHITE);
    }

    /* === 4. NEW RECORD!! 表示（新記録時） ===
     * sinf() を使って金色テキストを明滅させ、達成感を演出する。
     * GetTime() はプログラム起動からの経過秒数を返すdouble値。
     * これに 6.0 を掛けて速い振動を作り、0.5〜1.0 のアルファ値に変換。 */
    if (game.isNewRecord) {
        const char *recordText = "NEW RECORD!!";
        int recordSize = 36;
        int recordWidth = MeasureText(recordText, recordSize);
        int recordX = (SCREEN_WIDTH - recordWidth) / 2;
        int recordY = 380;

        /* パルスアルファ: sinf の出力(-1〜1)を 0〜1 に変換し、128〜255にマッピング */
        float pulse = sinf((float)GetTime() * 6.0f);
        float normalizedPulse = (pulse + 1.0f) / 2.0f;     /* 0.0〜1.0 */
        unsigned char alpha = (unsigned char)(128.0f + normalizedPulse * 127.0f);

        /* 金色（ゴールド）のテキストで新記録を祝う */
        Color goldColor = { 255, 215, 0, alpha };

        DrawText(recordText, recordX + 2, recordY + 2, recordSize, (Color){ 0, 0, 0, alpha });
        DrawText(recordText, recordX, recordY, recordSize, goldColor);
    }

    /* === 5. タイムアウト表示 ===
     * タイムアタックで制限時間を超えた場合の特別メッセージ。
     * 記録が無効であることを赤文字で明示する。 */
    if (game.isTimedOut) {
        const char *toText = "TIME UP - NO RECORD";
        int toSize = 22;
        int toWidth = MeasureText(toText, toSize);
        int toX = (SCREEN_WIDTH - toWidth) / 2;
        int toY = 380;
        DrawText(toText, toX, toY, toSize, (Color){ 255, 60, 60, 255 });
    }

    /* === 6. "Tap to continue"（画面下部） ===
     * sinf() で穏やかにフェードイン/アウトする案内テキスト。
     * プレイヤーに「タップして次へ進む」ことを促す。 */
    {
        const char *tapText = "Tap to continue";
        int tapSize = 22;
        int tapWidth = MeasureText(tapText, tapSize);
        int tapX = (SCREEN_WIDTH - tapWidth) / 2;
        int tapY = 700;

        /* 穏やかなフェード: 周期2.0f で緩やかに振動 */
        float fadeVal = (sinf((float)GetTime() * 2.0f) + 1.0f) / 2.0f;  /* 0.0〜1.0 */
        unsigned char tapAlpha = (unsigned char)(80.0f + fadeVal * 175.0f);

        DrawText(tapText, tapX, tapY, tapSize, (Color){ 255, 255, 255, tapAlpha });
    }
}

/* --------------------------------------------------------------------------
 * DrawScoresScreen — スコア一覧画面（ランキング）の描画
 * --------------------------------------------------------------------------
 * 【表示内容】
 *   1. 背景: 暗い紺色（リラックスした閲覧画面の雰囲気）
 *   2. タイトル "RANKINGS"
 *   3. 3つのセクション（TIME ATTACK / SURVIVAL / MARATHON）
 *      - 各セクションにモード名ヘッダーとTOP5リスト
 *      - タイムアタック/マラソンは時間形式、サバイバルは枚数形式
 *   4. 戻るボタン（画面下部）
 *
 * 【レイアウト設計】
 *   450x800 の画面に3セクション＋ヘッダー＋ボタンを収める必要がある。
 *   各セクションは約200px の高さを使い、
 *   セクション間は小さなマージンで区切る。
 *
 *   y=10〜50   : "RANKINGS" タイトル
 *   y=50〜240  : TIME ATTACK セクション
 *   y=240〜430 : SURVIVAL セクション
 *   y=430〜620 : MARATHON セクション
 *   y=650〜    : BACK ボタン
 * ------------------------------------------------------------------------ */
void DrawScoresScreen(void)
{
    /* === 1. 背景色 ===
     * 暗い紺色。ランキング画面は落ち着いた色合いが適切。 */
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 20, 20, 30, 255 });

    /* === 2. タイトル "RANKINGS" === */
    {
        const char *title = "RANKINGS";
        int titleSize = 36;
        int titleWidth = MeasureText(title, titleSize);
        int titleX = (SCREEN_WIDTH - titleWidth) / 2;
        int titleY = 15;
        DrawText(title, titleX + 1, titleY + 1, titleSize, (Color){ 0, 0, 0, 150 });
        DrawText(title, titleX, titleY, titleSize, (Color){ 255, 220, 60, 255 });
    }

    /* === 3. 3つのセクションを描画 ===
     * 各セクションの描画を1つのループで処理する。
     * モードインデックス: 0=タイムアタック, 1=サバイバル, 2=マラソン */
    {
        /* セクションのモード名 */
        const char *modeNames[3] = { "TIME ATTACK", "SURVIVAL", "MARATHON" };
        /* セクションヘッダーの色（モード別に色を変えて区別しやすく） */
        Color headerColors[3] = {
            { 100, 160, 255, 255 },     /* タイムアタック: 青系 */
            { 255, 100, 100, 255 },     /* サバイバル: 赤系 */
            { 100, 220, 100, 255 }      /* マラソン: 緑系 */
        };

        /* 各セクションの開始Y座標 */
        int sectionStartY = 65;
        int sectionHeight = 190;     /* 各セクションの高さ（ヘッダー＋TOP5分） */

        for (int m = 0; m < 3; m++) {
            int secY = sectionStartY + m * sectionHeight;

            /* --- セクションヘッダー ---
             * モード名を表示。背景に薄い矩形を敷いて区切りを明確にする。 */
            DrawRectangle(15, secY, SCREEN_WIDTH - 30, 28,
                          (Color){ 40, 40, 55, 255 });
            int headerSize = 22;
            int headerWidth = MeasureText(modeNames[m], headerSize);
            int headerX = (SCREEN_WIDTH - headerWidth) / 2;
            DrawText(modeNames[m], headerX, secY + 3, headerSize, headerColors[m]);

            /* --- TOP5 ランキングリスト ---
             * 各スコアを "1. 値" の形式で表示する。
             * スコアが0の場合は "---"（未登録）と表示。 */
            int entrySize = 18;
            int entryY = secY + 34;    /* ヘッダーの下からスタート */
            int entrySpacing = 28;     /* 各エントリの行間 */

            for (int rank = 0; rank < MAX_RANKINGS; rank++) {
                char entryBuf[64];
                int value = 0;

                /* モードに応じたスコア値を取得 */
                if (m == 0)      value = scores.timeattack[rank];
                else if (m == 1) value = scores.survival[rank];
                else             value = scores.marathon[rank];

                if (value == 0) {
                    /* 未登録のスロット */
                    snprintf(entryBuf, sizeof(entryBuf), "%d.  ---", rank + 1);
                }
                else if (m == 0) {
                    /* タイムアタック: ミリ秒値を時間形式に変換 */
                    char timeBuf[32];
                    FormatTime(timeBuf, sizeof(timeBuf), value);
                    snprintf(entryBuf, sizeof(entryBuf), "%d.  %s", rank + 1, timeBuf);
                }
                else if (m == 1) {
                    /* サバイバル: 撃ち抜き枚数をそのまま表示 */
                    snprintf(entryBuf, sizeof(entryBuf), "%d.  %d hits", rank + 1, value);
                }
                else {
                    /* マラソン: ミリ秒値を時間形式に変換 */
                    char timeBuf[32];
                    FormatTime(timeBuf, sizeof(timeBuf), value);
                    snprintf(entryBuf, sizeof(entryBuf), "%d.  %s", rank + 1, timeBuf);
                }

                /* ランクエントリを画面左寄りに描画 */
                int ey = entryY + rank * entrySpacing;

                /* 1位は金色、2位は銀色、3位は銅色、その他は白 */
                Color rankColor = WHITE;
                if (rank == 0)      rankColor = (Color){ 255, 215, 0, 255 };     /* 金 */
                else if (rank == 1) rankColor = (Color){ 200, 200, 210, 255 };   /* 銀 */
                else if (rank == 2) rankColor = (Color){ 205, 127, 50, 255 };    /* 銅 */

                DrawText(entryBuf, 60, ey, entrySize, rankColor);
            }
        }
    }

    /* === 4. 戻るボタン === */
    {
        Rectangle btnBack = GetButtonRect(0, 650);
        Vector2 mousePos = GetMousePosition();
        bool hoveredBack = CheckCollisionPointRec(mousePos, btnBack);
        DrawButton("BACK", btnBack, (Color){ 80, 80, 100, 255 }, WHITE, hoveredBack);
    }
}
