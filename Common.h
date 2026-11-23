#pragma once
#include "Vector2.h"

// ゲーム本体のウィンドウサイズ（元の1280x720を維持）
inline constexpr Vector2i kWindowSize = { 1280, 720 };

// 画面中央座標
inline constexpr Vector2 kWindowCenter = { 640.0f, 360.0f };

// ワールド座標（中央が原点・Y上向き）から画面座標への変換オフセット
inline constexpr Vector2 kDelayBetweenWToS = { 640.0f, 360.0f };
