#include "game.h"
#include "logic.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static void OnTargetHit(void);
static void OnTargetMiss(void);
extern void PlayWebBGM(int trackId);

void InitGame(GameMode mode)
{
    game.score = 0;
    game.elapsedTime = 0.0f;
    game.stage = 1;
    game.isGameOver = false;
    game.isNewRecord = false;
    game.isTimedOut = false;
    game.mode = mode;
    game.screen = SCREEN_PLAYING;

    switch (mode) {
        case MODE_TIMEATTACK: PlayWebBGM(1); break;
        case MODE_SURVIVAL:   PlayWebBGM(2); break;
        case MODE_MARATHON:   PlayWebBGM(3); break;
    }

    game.freezeTimer = 0.0f;
    game.flashTimer = 0.0f;
    game.gaugeMax = GetMarathonGaugeMax(1);
    game.gaugeValue = game.gaugeMax;
    game.gaugeDecaySpeed = MARATHON_INITIAL_DECAY;
    InitAllTargetRows();
    srand((unsigned int)time(NULL));
}

void GenerateTargetRow(int row)
{
    int correctLane = rand() % LANE_COUNT;
    int i;
    for (i = 0; i < LANE_COUNT; i++) {
        game.targets[row][i].lane = i;
        game.targets[row][i].row = row;
        game.targets[row][i].isActive = true;
        game.targets[row][i].isCorrect = false;
        game.targets[row][i].imageIndex = -1;
    }
    game.targets[row][correctLane].isCorrect = true;
    game.targets[row][correctLane].imageIndex = game.stage - 1;

    if (game.mode == MODE_MARATHON) {
        float dualRoll = (float)rand() / (float)RAND_MAX;
        if (dualRoll < MARATHON_DUAL_CHANCE) {
            int secondLane = (correctLane + 1 + rand() % 2) % LANE_COUNT;
            game.targets[row][secondLane].isCorrect = true;
            game.targets[row][secondLane].imageIndex = game.stage - 1;
        }
    }
}

void InitAllTargetRows(void)
{
    int row;
    for (row = 0; row < MAX_VISIBLE_ROWS; row++)
        GenerateTargetRow(row);
}

void SlideTargetsForward(void)
{
    int row, lane;
    for (row = 0; row < MAX_VISIBLE_ROWS - 1; row++) {
        for (lane = 0; lane < LANE_COUNT; lane++) {
            game.targets[row][lane] = game.targets[row + 1][lane];
            game.targets[row][lane].row = row;
        }
    }
    GenerateTargetRow(MAX_VISIBLE_ROWS - 1);
}

float GetTargetSize(int row)
{
    float t = (float)row / (float)(MAX_VISIBLE_ROWS - 1);
    return (float)TARGET_SIZE_FRONT + ((float)TARGET_SIZE_BACK - (float)TARGET_SIZE_FRONT) * t;
}

Rectangle GetTargetRect(int row, int lane)
{
    float size = GetTargetSize(row);
    float playAreaWidth = (float)(PLAY_AREA_RIGHT - PLAY_AREA_LEFT);
    float laneWidth = (playAreaWidth / (float)LANE_COUNT) * (size / (float)TARGET_SIZE_FRONT);
    float totalWidth = laneWidth * (float)LANE_COUNT;
    float startX = (float)PLAY_AREA_LEFT + (playAreaWidth - totalWidth) / 2.0f;
    float x = startX + (float)lane * laneWidth + (laneWidth - size) / 2.0f;

    float y = (float)PLAY_AREA_BOTTOM;
    int r;
    for (r = 0; r < row; r++)
        y -= GetTargetSize(r) * (1.0f - ROW_OVERLAP_RATIO);
    y -= size;

    return (Rectangle){ x, y, size, size };
}

TapResult HandleTargetTap(Vector2 pos)
{
    if (game.freezeTimer > 0.0f) return TAP_NONE;
    int lane;
    for (lane = 0; lane < LANE_COUNT; lane++) {
        Rectangle rect = GetTargetRect(0, lane);
        if (CheckCollisionPointRec(pos, rect)) {
            if (game.targets[0][lane].isCorrect) { OnTargetHit(); return TAP_HIT; }
            else { OnTargetMiss(); return TAP_MISS; }
        }
    }
    return TAP_NONE;
}

static void OnTargetHit(void)
{
    game.score++;
    game.stage = GetCurrentStage(game.score);
    SlideTargetsForward();

    if (game.mode == MODE_MARATHON) {
        game.gaugeMax = GetMarathonGaugeMax(game.stage);
        game.gaugeValue = game.gaugeMax;
    } else if (game.mode == MODE_TIMEATTACK) {
        if (game.score >= TIMEATTACK_GOAL) game.isGameOver = true;
    }
}

static void OnTargetMiss(void)
{
    game.freezeTimer = MISS_FREEZE_TIME;
    game.flashTimer = MISS_FLASH_TIME;
    if (game.mode == MODE_SURVIVAL) game.isGameOver = true;
}

int GetCurrentStage(int score)
{
    const int *thresholds = (game.mode == MODE_TIMEATTACK)
        ? TIMEATTACK_STAGE_THRESHOLDS : STAGE_THRESHOLDS;
    int i;
    for (i = 0; i < TOTAL_STAGES - 1; i++) {
        if (score < thresholds[i]) return i + 1;
    }
    return TOTAL_STAGES;
}

float GetMarathonGaugeMax(int stage)
{
    int value = 17 - stage;
    return (value > 1) ? (float)value : 1.0f;
}

void UpdatePlaying(float dt)
{
    if (game.freezeTimer > 0.0f) game.freezeTimer -= dt;
    if (game.flashTimer > 0.0f) game.flashTimer -= dt;
    game.elapsedTime += dt;

    if (game.mode == MODE_TIMEATTACK) {
        if (game.elapsedTime >= TIMEATTACK_LIMIT) {
            game.isGameOver = true;
            game.isTimedOut = true;
        }
    } else if (game.mode == MODE_MARATHON) {
        game.gaugeDecaySpeed = MARATHON_INITIAL_DECAY + game.elapsedTime * MARATHON_DECAY_ACCEL;
        game.gaugeValue -= game.gaugeDecaySpeed * dt;
        if (game.gaugeValue <= 0.0f) {
            game.gaugeValue = 0.0f;
            game.isGameOver = true;
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 pos = GetMousePosition();
        Rectangle pauseRect = { (float)PAUSE_BTN_X, (float)PAUSE_BTN_Y, (float)PAUSE_BTN_SIZE, (float)PAUSE_BTN_SIZE };
        if (CheckCollisionPointRec(pos, pauseRect)) {
            game.screen = SCREEN_PAUSE;
            return;
        }
        HandleTargetTap(pos);
    }

    int pressedLane = -1;
    if (IsKeyPressed(KEY_LEFT))       pressedLane = 0;
    else if (IsKeyPressed(KEY_UP))    pressedLane = 1;
    else if (IsKeyPressed(KEY_RIGHT)) pressedLane = 2;

    if (pressedLane != -1 && game.freezeTimer <= 0.0f) {
        if (game.targets[0][pressedLane].isCorrect) OnTargetHit();
        else OnTargetMiss();
    }

    if (game.isGameOver) {
        game.screen = SCREEN_RESULT;
        PlayWebBGM(4);

        if (game.mode == MODE_TIMEATTACK) {
            if (!game.isTimedOut) {
                int timeMs = (int)(game.elapsedTime * 1000.0f);
                game.isNewRecord = RegisterScore(MODE_TIMEATTACK, timeMs);
            }
        } else if (game.mode == MODE_SURVIVAL) {
            game.isNewRecord = RegisterScore(MODE_SURVIVAL, game.score);
        } else if (game.mode == MODE_MARATHON) {
            int timeMs = (int)(game.elapsedTime * 1000.0f);
            game.isNewRecord = RegisterScore(MODE_MARATHON, timeMs);
        }
        SaveScores();
    }
}

void UpdateTitle(float dt)
{
    game.slideTimer -= dt;
    if (game.slideTimer <= 0.0f) {
        game.slideIndex = (game.slideIndex + 1) % TOTAL_STAGES;
        game.slideIndexNext = (game.slideIndex + 1) % TOTAL_STAGES;
        game.slideTimer = SLIDE_INTERVAL;
        game.slideAlpha = 0.0f;
    }
    if (game.slideAlpha < 1.0f) {
        game.slideAlpha += SLIDE_FADE_SPEED * dt;
        if (game.slideAlpha > 1.0f) game.slideAlpha = 1.0f;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 pos = GetMousePosition();
        float bx = (float)(SCREEN_WIDTH - BUTTON_WIDTH) / 2.0f;
        Rectangle btnTA = { bx, 380.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
        Rectangle btnSV = { bx, 465.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
        Rectangle btnMR = { bx, 550.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
        Rectangle btnSC = { bx, 650.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };

        if (CheckCollisionPointRec(pos, btnTA)) { InitGame(MODE_TIMEATTACK); return; }
        if (CheckCollisionPointRec(pos, btnSV)) { InitGame(MODE_SURVIVAL); return; }
        if (CheckCollisionPointRec(pos, btnMR)) { InitGame(MODE_MARATHON); return; }
        if (CheckCollisionPointRec(pos, btnSC)) { game.screen = SCREEN_SCORES; return; }
    }
}

void UpdateScoresScreen(float dt)
{
    (void)dt;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 pos = GetMousePosition();
        Rectangle btnBack = { (float)(SCREEN_WIDTH - BUTTON_WIDTH) / 2.0f, 650.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
        if (CheckCollisionPointRec(pos, btnBack)) {
            game.screen = SCREEN_TITLE;
            PlayWebBGM(0);
        }
    }
}

void UpdatePause(float dt)
{
    (void)dt;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 pos = GetMousePosition();
        float bx = (float)(SCREEN_WIDTH - BUTTON_WIDTH) / 2.0f;
        Rectangle btnResume = { bx, 300.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
        Rectangle btnRetry  = { bx, 385.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
        Rectangle btnTitle  = { bx, 470.0f, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };

        if (CheckCollisionPointRec(pos, btnResume)) {
            game.screen = SCREEN_COUNTDOWN;
            game.countdownNumber = 3;
            game.countdownTimer = COUNTDOWN_INTERVAL;
            return;
        }
        if (CheckCollisionPointRec(pos, btnRetry)) { InitGame(game.mode); return; }
        if (CheckCollisionPointRec(pos, btnTitle)) { game.screen = SCREEN_TITLE; PlayWebBGM(0); return; }
    }
}

void UpdateCountdown(float dt)
{
    game.countdownTimer -= dt;
    if (game.countdownTimer <= 0.0f) {
        game.countdownNumber--;
        if (game.countdownNumber <= 0) game.screen = SCREEN_PLAYING;
        else game.countdownTimer = COUNTDOWN_INTERVAL;
    }
}

void UpdateResult(float dt)
{
    (void)dt;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        game.screen = SCREEN_TITLE;
        PlayWebBGM(0);
    }
}

void SaveScores(void)
{
#ifdef __EMSCRIPTEN__
    int i;
    for (i = 0; i < MAX_RANKINGS; i++) {
        EM_ASM({ localStorage.setItem("sataro_ta_" + $0, $1); }, i, scores.timeattack[i]);
        EM_ASM({ localStorage.setItem("sataro_sv_" + $0, $1); }, i, scores.survival[i]);
        EM_ASM({ localStorage.setItem("sataro_mr_" + $0, $1); }, i, scores.marathon[i]);
    }
#else
    FILE *fp = fopen("scores.dat", "wb");
    if (fp) { fwrite(&scores, sizeof(ScoreData), 1, fp); fclose(fp); }
#endif
}

void LoadScores(void)
{
    memset(&scores, 0, sizeof(ScoreData));
#ifdef __EMSCRIPTEN__
    int i;
    for (i = 0; i < MAX_RANKINGS; i++) {
        scores.timeattack[i] = EM_ASM_INT({ return parseInt(localStorage.getItem("sataro_ta_" + $0)) || 0; }, i);
        scores.survival[i]   = EM_ASM_INT({ return parseInt(localStorage.getItem("sataro_sv_" + $0)) || 0; }, i);
        scores.marathon[i]   = EM_ASM_INT({ return parseInt(localStorage.getItem("sataro_mr_" + $0)) || 0; }, i);
    }
#else
    FILE *fp = fopen("scores.dat", "rb");
    if (fp) { fread(&scores, sizeof(ScoreData), 1, fp); fclose(fp); }
#endif
}

bool RegisterScore(GameMode mode, int value)
{
    int *ranking = NULL;
    bool ascending = false;
    switch (mode) {
        case MODE_TIMEATTACK: ranking = scores.timeattack; ascending = true;  break;
        case MODE_SURVIVAL:   ranking = scores.survival;   ascending = false; break;
        case MODE_MARATHON:   ranking = scores.marathon;   ascending = false; break;
        default: return false;
    }

    int insertPos = -1, i;
    for (i = 0; i < MAX_RANKINGS; i++) {
        if (ascending) { if (ranking[i] == 0 || value < ranking[i]) { insertPos = i; break; } }
        else           { if (ranking[i] == 0 || value > ranking[i]) { insertPos = i; break; } }
    }
    if (insertPos == -1) return false;

    for (i = MAX_RANKINGS - 1; i > insertPos; i--)
        ranking[i] = ranking[i - 1];
    ranking[insertPos] = value;
    return true;
}
