#include "game.h"
#include "logic.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

GameState game = {0};
ScoreData scores = {0};
Texture2D textures[TOTAL_STAGES] = {0};

static RenderTexture2D renderTarget = {0};

static Texture2D LoadAndCropSquare(const char *filename)
{
    Image img = LoadImage(filename);
    if (img.data == NULL) {
        Texture2D emptyTex = {0};
        return emptyTex;
    }
    int cropSize = (img.width < img.height) ? img.width : img.height;
    int offsetX = (img.width - cropSize) / 2;
    int offsetY = (img.height - cropSize) / 2;
    ImageCrop(&img, (Rectangle){ (float)offsetX, (float)offsetY, (float)cropSize, (float)cropSize });
    ImageResize(&img, TEXTURE_LOAD_SIZE, TEXTURE_LOAD_SIZE);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

static void LoadAllTextures(void)
{
    for (int i = 0; i < TOTAL_STAGES; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "image/%d.jpeg", i + 1);
        textures[i] = LoadAndCropSquare(filename);
    }
}

static void UnloadAllTextures(void)
{
    for (int i = 0; i < TOTAL_STAGES; i++) {
        UnloadTexture(textures[i]);
    }
}

void PlayWebBGM(int trackId)
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (!window.myBgmAudio) {
            window.myBgmAudio = new Audio();
            window.myBgmAudio.loop = true;
        }
        var tracks = [
            "audio/start.mp3",
            "audio/timeattack.mp3",
            "audio/survival.mp3",
            "audio/marathon.mp3",
            "audio/result.mp3"
        ];
        if ($0 >= 0 && $0 < tracks.length) {
            window.myBgmAudio.src = tracks[$0];
            window.myBgmAudio.play().catch(function(e){ console.log("BGM待機中"); });
        } else {
            window.myBgmAudio.pause();
        }
    }, trackId);
#else
    (void)trackId;
#endif
}

void UpdateDrawFrame(void)
{
    float dt = GetFrameTime();

    float scaleX = (float)GetScreenWidth() / SCREEN_WIDTH;
    float scaleY = (float)GetScreenHeight() / SCREEN_HEIGHT;
    float finalScale = (scaleX < scaleY) ? scaleX : scaleY;
    int offsetX = (int)((GetScreenWidth() - (SCREEN_WIDTH * finalScale)) * 0.5f);
    int offsetY = (int)((GetScreenHeight() - (SCREEN_HEIGHT * finalScale)) * 0.5f);
    SetMouseOffset(-offsetX, -offsetY);
    SetMouseScale(1.0f / finalScale, 1.0f / finalScale);

    switch (game.screen) {
        case SCREEN_TITLE:     UpdateTitle(dt); break;
        case SCREEN_PLAYING:   UpdatePlaying(dt); break;
        case SCREEN_PAUSE:     UpdatePause(dt); break;
        case SCREEN_COUNTDOWN: UpdateCountdown(dt); break;
        case SCREEN_RESULT:    UpdateResult(dt); break;
        case SCREEN_SCORES:    UpdateScoresScreen(dt); break;
    }

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
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle sourceRec = { 0.0f, 0.0f, (float)renderTarget.texture.width, -(float)renderTarget.texture.height };
    Rectangle destRec = { (float)offsetX, (float)offsetY, (float)SCREEN_WIDTH * finalScale, (float)SCREEN_HEIGHT * finalScale };
    DrawTexturePro(renderTarget.texture, sourceRec, destRec, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndDrawing();
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Quick Draw Panic");
    renderTarget = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_BILINEAR);
    InitAudioDevice();
    LoadAllTextures();
    LoadScores();

    game.screen = SCREEN_TITLE;
    game.slideIndex = 0;
    game.slideIndexNext = 1;
    game.slideTimer = SLIDE_INTERVAL;
    game.slideAlpha = 1.0f;
    PlayWebBGM(0);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(TARGET_FPS);
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    UnloadAllTextures();
    UnloadRenderTexture(renderTarget);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
