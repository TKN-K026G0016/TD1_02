#pragma once
#include "Vector2.h"
#include "Timer.h"
#include "Tool.h"
#include "Texture.h"

#define _USE_MATH_DEFINES
#include <math.h>

//エフェクトの描画タイプ
enum class TypeDrawEffect {
	Obj,
	UI,
};

#pragma region データ: TriangleEffect

class TriangleEffect {
public:
	TriangleEffect(Vector2 pos, float sizeValue, float moveSpeed, float moveTheta, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw);

	void Update();
	void Draw(TypeDrawEffect typeDraw);

	void Move();
	void Easing();

	//void Create(Vector2 pos, float moveSpeed, float moveTheta, float easingSpeed, TypeDrawEffect typeDraw);
	bool GetIsShoot() const;

private:
	Vector2 pos_ = {};

	bool isShoot_ = false;
	TypeDrawEffect typeDraw_ = TypeDrawEffect::Obj;

	float sizeValue_ = 0.0f;

	//<移動関係>
	float moveTheta_ = 0.0f;
	float moveSpeed_ = 0.0f;

	//<回転関係>
	float rotateTheta_ = 0.0f;

	//<easing関係>
	float easingT_ = 0.0f;
	float easingSpeed_ = 0.0f;

	//不透明度
	int alpha_ = 0;

	//色
	unsigned int color_ = { };
};

#pragma endregion

#pragma region データ: ScaleUpCircleEffect

class CircleEffect {
public:
	CircleEffect(Vector2 pos, float endSizeValue, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw);

	void Update(void);
	void Draw(TypeDrawEffect typeDraw);

	bool GetIsShoot(void) const;

private:
	Vector2 pos_ = {};
	bool isShoot_ = false;
	TypeDrawEffect typeDraw_ = TypeDrawEffect::Obj;

	//<サイズ関係>
	float sizeValue_ = 0.0f;
	float endSizeValue_ = 0.0f;

	//<イージング関係>
	float easingT_ = 0.0f;
	float easingSpeed_ = 0.0f;

	int alpha_ = 0;

	unsigned int color_ = WHITE;
};

#pragma endregion

#pragma region データ: ExplosionEffect

class ExplosionEffect {
public:
	ExplosionEffect(Vector2 pos, unsigned int color,  TypeDrawEffect typeDraw);

	void Update(void);
	void Draw(TypeDrawEffect typeDraw);

	bool GetIsShoot(void) const;

private:
	Vector2 pos_ = {};
	bool isShoot_ = false;

	TypeDrawEffect typeDraw_ = TypeDrawEffect::Obj;

	unsigned int color_ = WHITE;

	Texture texture_ = {};
};

#pragma endregion

void InitEffect(void);
void UpdateEffect(void);
void DrawEffectAsObj(void);
void DrawEffectAsUI(void);

void CreateTriangleEffect(Vector2 pos, float sizeValue, float moveSpeed, float moveTheta, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw);
void CreateCircleEffect(Vector2 pos, float endSizeValue, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw);
void CreateExplosionEffect(Vector2 pos, unsigned int color, TypeDrawEffect typeDraw);