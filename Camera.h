#pragma once
#include "Matrix.h"

void InitCamera(void);
void UpdateCamera(void);

float GetCameraZoom(void);

Matrix3x3 GetWorldToScreenMatrix(void);
// 演出用の画面揺れ。powerはワールド座標単位、framesは継続フレーム数。
void ShakeCamera(float power, int frames);

// ボス戦では画面を固定。旧インターフェースを残して互換性を維持する。
void SetCameraFollowY(float playerWorldY);

// 一瞬だけズームして、部位破壊や大技の重みを出す（追加描画ゼロ）。
void PunchCamera(float zoomAmount, int frames);
