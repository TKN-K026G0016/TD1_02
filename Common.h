#pragma once
#include "vector2.h"

//ウィンドウの大きさ
constexpr Vector2i kWindowSize = { 1280, 720 };
//ウィンドウの中央座標
constexpr Vector2 kWindowCenter = { 640, 360 };

//worldPosとscreenPosのズレ
constexpr Vector2i kDelayBetweenWToS = { 0, kWindowSize.y };