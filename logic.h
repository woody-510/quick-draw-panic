#ifndef LOGIC_H
#define LOGIC_H

#include "game.h"

void InitGame(GameMode mode);
void GenerateTargetRow(int row);
void InitAllTargetRows(void);
void SlideTargetsForward(void);
TapResult HandleTargetTap(Vector2 pos);
void UpdatePlaying(float dt);
int GetCurrentStage(int score);
float GetMarathonGaugeMax(int stage);
float GetTargetSize(int row);
Rectangle GetTargetRect(int row, int lane);
void SaveScores(void);
void LoadScores(void);
bool RegisterScore(GameMode mode, int value);
void UpdateTitle(float dt);
void UpdateScoresScreen(float dt);
void UpdatePause(float dt);
void UpdateCountdown(float dt);
void UpdateResult(float dt);

#endif
