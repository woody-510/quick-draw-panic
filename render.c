#include "game.h"
#include "render.h"
#include "logic.h"
#include <stdio.h>
#include <math.h>

static Rectangle GetButtonRect(int index, int startY)
{
    Rectangle rect;
    rect.x      = (float)(SCREEN_WIDTH - BUTTON_WIDTH) / 2.0f;
    rect.y      = (float)startY + (float)index * (float)(BUTTON_HEIGHT + BUTTON_MARGIN);
    rect.width  = (float)BUTTON_WIDTH;
    rect.height = (float)BUTTON_HEIGHT;
    return rect;
}

static bool IsButtonClicked(Rectangle rect)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(GetMousePosition(), rect);
}

static void DrawButton(const char *text, Rectangle rect, Color bgColor,
                       Color textColor, bool hovered)
{
    Color drawColor = bgColor;
    if (hovered) {
        drawColor.r = (unsigned char)((bgColor.r + 30 > 255) ? 255 : bgColor.r + 30);
        drawColor.g = (unsigned char)((bgColor.g + 30 > 255) ? 255 : bgColor.g + 30);
        drawColor.b = (unsigned char)((bgColor.b + 30 > 255) ? 255 : bgColor.b + 30);
    }
    Rectangle shadowRect = { rect.x + 2.0f, rect.y + 3.0f, rect.width, rect.height };
    DrawRectangleRounded(shadowRect, 0.3f, 8, (Color){ 0, 0, 0, 80 });
    DrawRectangleRounded(rect, 0.3f, 8, drawColor);

    int fontSize = 26;
    int textWidth = MeasureText(text, fontSize);
    int textX = (int)(rect.x + (rect.width - (float)textWidth) / 2.0f);
    int textY = (int)(rect.y + (rect.height - (float)fontSize) / 2.0f);
    DrawText(text, textX + 1, textY + 1, fontSize, (Color){ 0, 0, 0, 100 });
    DrawText(text, textX, textY, fontSize, textColor);
}

static void DrawSingleTarget(int row, int lane)
{
    Rectangle rect = GetTargetRect(row, lane);
    Target t = game.targets[row][lane];
    if (!t.isActive) return;

    int stageIdx = game.stage - 1;
    if (stageIdx < 0) stageIdx = 0;
    if (stageIdx >= TOTAL_STAGES) stageIdx = TOTAL_STAGES - 1;
    Color bgColor    = STAGE_BG_COLORS[stageIdx];
    Color blankColor = STAGE_BLANK_COLORS[stageIdx];
    float roundness = 0.15f;
    int segments = 6;

    if (t.isCorrect) {
        Rectangle shadowRect = { rect.x + 2.0f, rect.y + 3.0f, rect.width, rect.height };
        int shadowAlpha = 100 - row * 12;
        if (shadowAlpha < 30) shadowAlpha = 30;
        DrawRectangleRounded(shadowRect, roundness, segments,
                             (Color){ 0, 0, 0, (unsigned char)shadowAlpha });

        float borderSize = 3.0f;
        Rectangle borderRect = {
            rect.x - borderSize, rect.y - borderSize,
            rect.width + borderSize * 2.0f, rect.height + borderSize * 2.0f
        };
        Color borderColor = {
            (unsigned char)((bgColor.r + 200) / 2),
            (unsigned char)((bgColor.g + 200) / 2),
            (unsigned char)((bgColor.b + 200) / 2), 220
        };
        DrawRectangleRounded(borderRect, roundness, segments, borderColor);

        Texture2D tex = textures[t.imageIndex];
        Rectangle source = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
        Rectangle dest   = { rect.x, rect.y, rect.width, rect.height };
        DrawTexturePro(tex, source, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    } else {
        Color fillColor = { blankColor.r, blankColor.g, blankColor.b, 180 };
        DrawRectangleRounded(rect, roundness, segments, fillColor);

        float margin = rect.width * 0.15f;
        float x1 = rect.x + margin;
        float y1 = rect.y + margin;
        float x2 = rect.x + rect.width - margin;
        float y2 = rect.y + rect.height - margin;
        float lineThickness = rect.width * 0.025f;
        if (lineThickness < 1.0f) lineThickness = 1.0f;
        Color xColor = { blankColor.r, blankColor.g, blankColor.b, 220 };
        DrawLineEx((Vector2){ x1, y1 }, (Vector2){ x2, y2 }, lineThickness, xColor);
        DrawLineEx((Vector2){ x2, y1 }, (Vector2){ x1, y2 }, lineThickness, xColor);
    }
}

static void DrawAllTargets(void)
{
    for (int row = MAX_VISIBLE_ROWS - 1; row >= 0; row--) {
        for (int lane = 0; lane < LANE_COUNT; lane++) {
            DrawSingleTarget(row, lane);
        }
    }
}

static void FormatTime(char *buf, int bufSize, int ms)
{
    int min  = ms / 60000;
    int sec  = (ms % 60000) / 1000;
    int msec = ms % 1000;
    snprintf(buf, (size_t)bufSize, "%02d:%02d.%03d", min, sec, msec);
}

void DrawTitleScreen(void)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 18, 18, 28, 255 });
    DrawRectangle(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2, (Color){ 25, 25, 40, 255 });

    {
        int idx = game.slideIndex;
        if (idx < 0) idx = 0;
        if (idx >= TOTAL_STAGES) idx = 0;
        Texture2D tex = textures[idx];
        if (tex.id > 0) {
            float slideSize = 180.0f;
            float slideX = ((float)SCREEN_WIDTH - slideSize) / 2.0f;
            float slideY = 175.0f;
            Rectangle src  = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
            Rectangle dest = { slideX, slideY, slideSize, slideSize };
            float displayAlpha = game.slideAlpha * 0.35f;
            DrawTexturePro(tex, src, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, Fade(WHITE, displayAlpha));
        }
    }

    {
        const char *titleText = "QUICK DRAW PANIC";
        int titleSize = 42;
        int titleWidth = MeasureText(titleText, titleSize);
        int titleX = (SCREEN_WIDTH - titleWidth) / 2;
        int titleY = 80;
        DrawText(titleText, titleX + 2, titleY + 2, titleSize, (Color){ 0, 0, 0, 180 });
        DrawText(titleText, titleX, titleY, titleSize, (Color){ 255, 220, 60, 255 });

        const char *subText = "- Hayauchi Panic -";
        int subSize = 20;
        int subWidth = MeasureText(subText, subSize);
        int subX = (SCREEN_WIDTH - subWidth) / 2;
        int subY = titleY + titleSize + 10;
        DrawText(subText, subX + 1, subY + 1, subSize, (Color){ 0, 0, 0, 120 });
        DrawText(subText, subX, subY, subSize, (Color){ 200, 200, 220, 255 });
    }

    {
        int btnStartY = 380;
        Vector2 mousePos = GetMousePosition();
        Rectangle btnTA = GetButtonRect(0, btnStartY);
        DrawButton("TIME ATTACK", btnTA, (Color){ 50, 100, 200, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnTA));
        Rectangle btnSV = GetButtonRect(1, btnStartY);
        DrawButton("SURVIVAL", btnSV, (Color){ 200, 50, 50, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnSV));
        Rectangle btnMR = GetButtonRect(2, btnStartY);
        DrawButton("MARATHON", btnMR, (Color){ 40, 170, 60, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnMR));
    }

    {
        Rectangle btnRank = GetButtonRect(0, 650);
        Vector2 mousePos = GetMousePosition();
        DrawButton("RANKINGS", btnRank, (Color){ 80, 80, 100, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnRank));
    }
}

void DrawPlayingScreen(void)
{
    int stageIdx = game.stage - 1;
    if (stageIdx < 0) stageIdx = 0;
    if (stageIdx >= TOTAL_STAGES) stageIdx = TOTAL_STAGES - 1;
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, STAGE_BG_COLORS[stageIdx]);
    DrawAllTargets();

    {
        char stageBuf[64];
        snprintf(stageBuf, sizeof(stageBuf), "Stage %d - %s", game.stage, STAGE_NAMES[stageIdx]);
        DrawText(stageBuf, 11, 11, 20, (Color){ 0, 0, 0, 160 });
        DrawText(stageBuf, 10, 10, 20, WHITE);
    }

    {
        char hudBuf[64];
        if (game.mode == MODE_TIMEATTACK) {
            int elapsedMs = (int)(game.elapsedTime * 1000.0f);
            char timeBuf[32];
            FormatTime(timeBuf, sizeof(timeBuf), elapsedMs);
            int remaining = TIMEATTACK_GOAL - game.score;
            if (remaining < 0) remaining = 0;
            snprintf(hudBuf, sizeof(hudBuf), "%s  Left:%d", timeBuf, remaining);
        } else {
            snprintf(hudBuf, sizeof(hudBuf), "Score: %d", game.score);
        }
        int hudWidth = MeasureText(hudBuf, 22);
        int hudX = (SCREEN_WIDTH - hudWidth) / 2;
        DrawText(hudBuf, hudX + 1, 41, 22, (Color){ 0, 0, 0, 160 });
        DrawText(hudBuf, hudX, 40, 22, WHITE);
    }

    if (game.mode == MODE_MARATHON) {
        float gaugeX = 30.0f, gaugeY = 75.0f, gaugeW = 340.0f, gaugeH = 18.0f;
        DrawRectangleRounded(
            (Rectangle){ gaugeX - 1, gaugeY - 1, gaugeW + 2, gaugeH + 2 },
            0.3f, 4, (Color){ 30, 30, 30, 200 });
        float ratio = (game.gaugeMax > 0.0f) ? game.gaugeValue / game.gaugeMax : 0.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        Color gaugeColor;
        if (ratio > 0.5f) gaugeColor = (Color){ 50, 205, 50, 255 };
        else if (ratio > 0.25f) gaugeColor = (Color){ 255, 200, 0, 255 };
        else {
            float pulse = fabsf(sinf((float)GetTime() * 8.0f));
            gaugeColor = (Color){ 220, 40, 40, (unsigned char)(150.0f + pulse * 105.0f) };
        }
        float fillWidth = gaugeW * ratio;
        if (fillWidth > 0.5f)
            DrawRectangleRounded((Rectangle){ gaugeX, gaugeY, fillWidth, gaugeH }, 0.3f, 4, gaugeColor);
    }

    {
        Rectangle pauseRect = { (float)PAUSE_BTN_X, (float)PAUSE_BTN_Y, (float)PAUSE_BTN_SIZE, (float)PAUSE_BTN_SIZE };
        DrawRectangleRounded(pauseRect, 0.25f, 6, (Color){ 0, 0, 0, 120 });
        float barW = (float)PAUSE_BTN_SIZE * 0.15f;
        float barH = (float)PAUSE_BTN_SIZE * 0.55f;
        float barY = (float)PAUSE_BTN_Y + ((float)PAUSE_BTN_SIZE - barH) / 2.0f;
        float barX1 = (float)PAUSE_BTN_X + (float)PAUSE_BTN_SIZE * 0.30f;
        float barX2 = (float)PAUSE_BTN_X + (float)PAUSE_BTN_SIZE * 0.55f;
        DrawRectangleRounded((Rectangle){ barX1, barY, barW, barH }, 0.2f, 4, WHITE);
        DrawRectangleRounded((Rectangle){ barX2, barY, barW, barH }, 0.2f, 4, WHITE);
    }

    if (game.flashTimer > 0.0f) {
        float alphaRatio = game.flashTimer / MISS_FLASH_TIME;
        if (alphaRatio > 1.0f) alphaRatio = 1.0f;
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                      (Color){ 255, 255, 255, (unsigned char)(200.0f * alphaRatio) });
    }
}

void DrawPauseScreen(void)
{
    DrawPlayingScreen();
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 0, 0, 0, 180 });

    {
        const char *pauseText = "PAUSED";
        int pauseSize = 52;
        int pauseWidth = MeasureText(pauseText, pauseSize);
        int pauseX = (SCREEN_WIDTH - pauseWidth) / 2;
        DrawText(pauseText, pauseX + 2, 182, pauseSize, (Color){ 0, 0, 0, 200 });
        DrawText(pauseText, pauseX, 180, pauseSize, WHITE);
    }

    {
        int btnStartY = 300;
        Vector2 mousePos = GetMousePosition();
        Rectangle btnResume = GetButtonRect(0, btnStartY);
        DrawButton("RESUME", btnResume, (Color){ 40, 170, 60, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnResume));
        Rectangle btnRetry = GetButtonRect(1, btnStartY);
        DrawButton("RETRY", btnRetry, (Color){ 50, 100, 200, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnRetry));
        Rectangle btnTitle = GetButtonRect(2, btnStartY);
        DrawButton("TITLE", btnTitle, (Color){ 100, 100, 110, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnTitle));
    }
}

void DrawCountdownScreen(void)
{
    DrawPlayingScreen();
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 0, 0, 0, 120 });

    {
        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "%d", game.countdownNumber);
        float ratio = game.countdownTimer / COUNTDOWN_INTERVAL;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        int fontSize = 120 + (int)(ratio * 36.0f);
        int textWidth = MeasureText(numBuf, fontSize);
        int textX = (SCREEN_WIDTH - textWidth) / 2;
        int textY = (SCREEN_HEIGHT - fontSize) / 2 - 30;
        DrawText(numBuf, textX + 3, textY + 3, fontSize, (Color){ 0, 0, 0, 200 });
        DrawText(numBuf, textX, textY, fontSize, WHITE);
    }
}

void DrawResultScreen(void)
{
    int stageIdx = game.stage - 1;
    if (stageIdx < 0) stageIdx = 0;
    if (stageIdx >= TOTAL_STAGES) stageIdx = TOTAL_STAGES - 1;
    Color baseBg = STAGE_BG_COLORS[stageIdx];
    Color darkBg = { (unsigned char)(baseBg.r / 2), (unsigned char)(baseBg.g / 2), (unsigned char)(baseBg.b / 2), 255 };
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, darkBg);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 0, 0, 0, 100 });

    {
        const char *modeName = "";
        if (game.mode == MODE_TIMEATTACK) modeName = "TIME ATTACK";
        else if (game.mode == MODE_SURVIVAL) modeName = "SURVIVAL";
        else modeName = "MARATHON";
        int modeSize = 30;
        int modeWidth = MeasureText(modeName, modeSize);
        int modeX = (SCREEN_WIDTH - modeWidth) / 2;
        DrawText(modeName, modeX + 1, 121, modeSize, (Color){ 0, 0, 0, 150 });
        DrawText(modeName, modeX, 120, modeSize, (Color){ 180, 180, 220, 255 });
    }

    {
        char scoreBuf[64];
        int scoreSize = 48;
        if (game.mode == MODE_TIMEATTACK) {
            if (game.isTimedOut) snprintf(scoreBuf, sizeof(scoreBuf), "TIME UP!");
            else { char tb[32]; FormatTime(tb, sizeof(tb), (int)(game.elapsedTime * 1000.0f)); snprintf(scoreBuf, sizeof(scoreBuf), "%s", tb); }
        } else if (game.mode == MODE_SURVIVAL) {
            snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", game.score);
        } else {
            char tb[32]; FormatTime(tb, sizeof(tb), (int)(game.elapsedTime * 1000.0f)); snprintf(scoreBuf, sizeof(scoreBuf), "%s", tb);
        }
        int scoreWidth = MeasureText(scoreBuf, scoreSize);
        int scoreX = (SCREEN_WIDTH - scoreWidth) / 2;
        DrawText(scoreBuf, scoreX + 2, 282, scoreSize, (Color){ 0, 0, 0, 200 });
        DrawText(scoreBuf, scoreX, 280, scoreSize, WHITE);
    }

    if (game.isNewRecord) {
        const char *recordText = "NEW RECORD!!";
        int recordSize = 36;
        int recordWidth = MeasureText(recordText, recordSize);
        int recordX = (SCREEN_WIDTH - recordWidth) / 2;
        float pulse = sinf((float)GetTime() * 6.0f);
        unsigned char alpha = (unsigned char)(128.0f + (pulse + 1.0f) / 2.0f * 127.0f);
        DrawText(recordText, recordX + 2, 382, recordSize, (Color){ 0, 0, 0, alpha });
        DrawText(recordText, recordX, 380, recordSize, (Color){ 255, 215, 0, alpha });
    }

    if (game.isTimedOut) {
        const char *toText = "TIME UP - NO RECORD";
        int toSize = 22;
        int toWidth = MeasureText(toText, toSize);
        DrawText(toText, (SCREEN_WIDTH - toWidth) / 2, 380, toSize, (Color){ 255, 60, 60, 255 });
    }

    {
        const char *tapText = "Tap to continue";
        int tapSize = 22;
        int tapWidth = MeasureText(tapText, tapSize);
        float fadeVal = (sinf((float)GetTime() * 2.0f) + 1.0f) / 2.0f;
        unsigned char tapAlpha = (unsigned char)(80.0f + fadeVal * 175.0f);
        DrawText(tapText, (SCREEN_WIDTH - tapWidth) / 2, 700, tapSize, (Color){ 255, 255, 255, tapAlpha });
    }
}

void DrawScoresScreen(void)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 20, 20, 30, 255 });

    {
        const char *title = "RANKINGS";
        int titleSize = 36;
        int titleWidth = MeasureText(title, titleSize);
        int titleX = (SCREEN_WIDTH - titleWidth) / 2;
        DrawText(title, titleX + 1, 16, titleSize, (Color){ 0, 0, 0, 150 });
        DrawText(title, titleX, 15, titleSize, (Color){ 255, 220, 60, 255 });
    }

    {
        const char *modeNames[3] = { "TIME ATTACK", "SURVIVAL", "MARATHON" };
        Color headerColors[3] = {
            { 100, 160, 255, 255 }, { 255, 100, 100, 255 }, { 100, 220, 100, 255 }
        };
        int sectionStartY = 65;
        int sectionHeight = 190;

        for (int m = 0; m < 3; m++) {
            int secY = sectionStartY + m * sectionHeight;
            DrawRectangle(15, secY, SCREEN_WIDTH - 30, 28, (Color){ 40, 40, 55, 255 });
            int headerWidth = MeasureText(modeNames[m], 22);
            DrawText(modeNames[m], (SCREEN_WIDTH - headerWidth) / 2, secY + 3, 22, headerColors[m]);

            int entryY = secY + 34;
            for (int rank = 0; rank < MAX_RANKINGS; rank++) {
                char entryBuf[64];
                int value = 0;
                if (m == 0)      value = scores.timeattack[rank];
                else if (m == 1) value = scores.survival[rank];
                else             value = scores.marathon[rank];

                if (value == 0) {
                    snprintf(entryBuf, sizeof(entryBuf), "%d.  ---", rank + 1);
                } else if (m == 0 || m == 2) {
                    char timeBuf[32];
                    FormatTime(timeBuf, sizeof(timeBuf), value);
                    snprintf(entryBuf, sizeof(entryBuf), "%d.  %s", rank + 1, timeBuf);
                } else {
                    snprintf(entryBuf, sizeof(entryBuf), "%d.  %d hits", rank + 1, value);
                }

                Color rankColor = WHITE;
                if (rank == 0)      rankColor = (Color){ 255, 215, 0, 255 };
                else if (rank == 1) rankColor = (Color){ 200, 200, 210, 255 };
                else if (rank == 2) rankColor = (Color){ 205, 127, 50, 255 };
                DrawText(entryBuf, 60, entryY + rank * 28, 18, rankColor);
            }
        }
    }

    {
        Rectangle btnBack = GetButtonRect(0, 650);
        Vector2 mousePos = GetMousePosition();
        DrawButton("BACK", btnBack, (Color){ 80, 80, 100, 255 }, WHITE, CheckCollisionPointRec(mousePos, btnBack));
    }
}
