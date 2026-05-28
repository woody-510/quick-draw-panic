#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include <stdbool.h>

#define SCREEN_WIDTH        450
#define SCREEN_HEIGHT       800
#define TARGET_FPS          60

#define PLAY_AREA_TOP       100
#define PLAY_AREA_BOTTOM    750
#define PLAY_AREA_LEFT      25
#define PLAY_AREA_RIGHT     425

#define LANE_COUNT          3
#define MAX_VISIBLE_ROWS    6

#define TARGET_SIZE_FRONT   120
#define TARGET_SIZE_BACK    40

#define ROW_OVERLAP_RATIO   0.35f

#define TIMEATTACK_GOAL     50
#define TIMEATTACK_LIMIT    60.0f
#define MISS_FREEZE_TIME    1.0f
#define MISS_FLASH_TIME     0.15f
#define TOTAL_STAGES        16

#define MARATHON_DUAL_CHANCE  0.05f
#define MARATHON_DECAY_ACCEL  0.008f
#define MARATHON_INITIAL_DECAY 1.0f

#define COUNTDOWN_INTERVAL  1.0f

#define TEXTURE_LOAD_SIZE   256

#define SLIDE_INTERVAL      3.0f
#define SLIDE_FADE_SPEED    2.0f

#define BUTTON_WIDTH        300
#define BUTTON_HEIGHT       65
#define BUTTON_MARGIN       20
#define PAUSE_BTN_SIZE      40
#define PAUSE_BTN_X         (SCREEN_WIDTH - PAUSE_BTN_SIZE - 10)
#define PAUSE_BTN_Y         10

#define MAX_RANKINGS        5

typedef enum {
    SCREEN_TITLE,
    SCREEN_PLAYING,
    SCREEN_PAUSE,
    SCREEN_COUNTDOWN,
    SCREEN_RESULT,
    SCREEN_SCORES
} ScreenState;

typedef enum {
    MODE_TIMEATTACK,
    MODE_SURVIVAL,
    MODE_MARATHON
} GameMode;

typedef enum {
    TAP_NONE,
    TAP_HIT,
    TAP_MISS
} TapResult;

typedef struct {
    int lane;
    int row;
    int imageIndex;
    bool isCorrect;
    bool isActive;
} Target;

typedef struct {
    ScreenState screen;
    GameMode mode;
    int stage;
    int score;
    float elapsedTime;
    float gaugeValue;
    float gaugeMax;
    float gaugeDecaySpeed;
    float freezeTimer;
    float flashTimer;
    float countdownTimer;
    int countdownNumber;
    bool isGameOver;
    bool isNewRecord;
    bool isTimedOut;
    Target targets[MAX_VISIBLE_ROWS][LANE_COUNT];
    float slideTimer;
    int slideIndex;
    int slideIndexNext;
    float slideAlpha;
} GameState;

typedef struct {
    int timeattack[MAX_RANKINGS];
    int survival[MAX_RANKINGS];
    int marathon[MAX_RANKINGS];
} ScoreData;

static const char *STAGE_NAMES[TOTAL_STAGES] = {
    "Grassland", "Volcano", "Glacier", "Ghost Manor",
    "Desert", "Deep Sea", "Neon City", "Clock Tower",
    "Sky", "Toxic Swamp", "Colosseum", "Crystal Cave",
    "Heavy Ind.", "Outer Space", "Heaven", "Abyss"
};

static const Color STAGE_BG_COLORS[TOTAL_STAGES] = {
    { 102, 255, 102, 255 }, { 139,   0,   0, 255 },
    { 165, 242, 243, 255 }, {   0,   0,   0, 255 },
    { 244, 164,  96, 255 }, {  25,  25, 112, 255 },
    { 169, 169, 169, 255 }, { 181, 166,  66, 255 },
    { 135, 206, 235, 255 }, { 128,   0, 128, 255 },
    { 179,  57,  57, 255 }, {   0,   0, 128, 255 },
    { 183,  65,  14, 255 }, {  12,  12,  12, 255 },
    { 255, 215,   0, 255 }, { 102,   0,   0, 255 },
};

static const Color STAGE_BLANK_COLORS[TOTAL_STAGES] = {
    { 139,  90,  43, 255 }, {  54,  69,  79, 255 },
    { 250, 250, 250, 255 }, {  48,  25,  52, 255 },
    { 226, 114,  91, 255 }, { 127, 255, 212, 255 },
    { 255,  20, 147, 255 }, {  70, 130, 180, 255 },
    { 245, 245, 245, 255 }, {  50, 205,  50, 255 },
    { 209, 204, 192, 255 }, {   0, 255, 255, 255 },
    {  42,  52,  57, 255 }, { 192, 192, 192, 255 },
    { 240, 248, 255, 255 }, {  11,  11,  11, 255 },
};

static const int STAGE_THRESHOLDS[TOTAL_STAGES - 1] = {
    3, 6, 9, 12, 16, 20, 25, 30, 36, 42, 49, 57, 66, 76, 88
};

static const int TIMEATTACK_STAGE_THRESHOLDS[TOTAL_STAGES - 1] = {
    3, 6, 9, 12, 15, 18, 21, 25, 28, 31, 34, 37, 40, 43, 46
};

extern GameState game;
extern ScoreData scores;
extern Texture2D textures[TOTAL_STAGES];

#endif
