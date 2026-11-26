#pragma once
#include "Vector2.h"
#include "Timer.h"

//エフェクトの描画タイプ
enum class TypeDrawEffect {
	Obj,
	UI,
};

enum class TypeEffect {
	RotateTriangle,
	ScaleUpCircle,
	ScaleUpSquare,
};

#pragma region データ: rotateTriangle

class RotateTriangle {
public:


private:
	Vector2 pos_ = {};
	Vector2 size = {};

};

#pragma endregion

void InitEffect(void);
void UpdateEffect(void);
void DrawEffectAsObj(void);
void DrawEffectAsUI(void);