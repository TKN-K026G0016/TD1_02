#include "Stage.h"

#pragma region データ: 重力関係

//重力の強さ
static constexpr Vector2 kGravityScale = { 0.0f, -1.2f };

#pragma endregion

void InitStage(void) {

}

void UpdateStage(void) {

}

void DrawStage(void) {

}

#pragma region 関数: 外部参照

/// <summary>
/// 重力の強さ取得
/// </summary>
/// <returns>kGravityScale</returns>
Vector2 GetGravityScale(void) {
	return kGravityScale;
}

#pragma endregion
