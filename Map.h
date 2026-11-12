#pragma once
#include "Vector2.h"

//マップチップの種類
enum class TypeMapChip {
	None = -1,
	Ground = 1,

	Count
};

//どの方向からぶつかってきたか
enum class CollisionDirection {
	None,

	Right,
	Left,
	Up,
	Down,
};

/// <summary>
/// 座標軸の種類
/// </summary>
enum class MoveAxis {
	X,
	Y
};

struct MoveResult {
	Vector2 pos = {};

	CollisionDirection direction = CollisionDirection::None;
};

void InitMap(void);
void UpdateMap(void);
void DrawMap(void);

Vector2 GetChipSize(void);
Vector2i GetNowMapPos(Vector2 pos);
bool CheckOutMap(Vector2i mapPos);
TypeMapChip GetTypeMapChip(Vector2i mapPos);

MoveResult CheckCanMove(const Vector2& prePos, const Vector2& nextPos, const Vector2& size, MoveAxis axis);
MoveResult CheckCanMove(const Vector2& prePos, const Vector2& nextPos, const float& radius, MoveAxis axis);