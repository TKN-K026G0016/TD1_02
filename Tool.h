#pragma once
#include "vector2.h"
#include "matrix.h"

//頂点の数
constexpr int kVertexNum = 4;

enum class EasingMode {
	//一定の速度
	Linear,
	//前半ゆっくり
	EaseIn,
	//後半ゆっくり
	EaseOut,
	//前後半ゆっくり
	EaseInOut,
	//前半に後退する
	EaseInBack,
	//後半に後退する
	EaseOutBack,
};
constexpr float kOvershootAmount = 1.70158f;

//当たり判定の透明度
constexpr unsigned int kDebugHitColorAlpha = 0x55;

template <typename T>
//static_cast<int>()の代わり
constexpr int ToInt(T value) {
	return static_cast<int>(value);
}

template <typename T>
//static_cast<float>()の代わり
constexpr float ToFloat(T value) {
	return static_cast<float>(value);
}

void InitTool(void);


Vector2i ConvertPosWToSUI(Vector2 posW);

void SetVertex(Vector2 pos, Vector2 size, float rotateTheta, Vector2 vertex[kVertexNum]);

Matrix3x3 Multiply(Matrix3x3 matrix1, Matrix3x3 matrix2);
Matrix3x3 MakeRotateMatrix(float theta);
Matrix3x3 MakeScaleMatrix(Vector2 size);
Matrix3x3 MakeTranslateMatrix(Vector2 translate);
Matrix3x3 MakeWorldMatrix(Vector2 scale, float rotateTheta, Vector2 pos);
Matrix3x3 Inverse(Matrix3x3 matrix);
Matrix3x3 MakeOrthoMatrix(Vector2 viewSize);
Matrix3x3 MakeViewportMatrix(Vector2 viewSize);
Vector2 Transform(Vector2 vector, Matrix3x3 matrix);

float GetDistancePointToSegment(Vector2 p, Vector2 a, Vector2 b);
bool CheckCollisionCircleVsCircle(const Vector2& circle1Pos, float radius1, const Vector2& circle2Pos, float radius2);
bool CheckCollisionCircleVsQuad(const Vector2& circlePos, float circleRadius, const Vector2 vertex[4]);
bool CheckCollisionCircleVsCapsule(const Vector2& circlePos, float circleRadius, const Vector2 capsulePos[2], float capsuleRadius);

float CreateEasedT(float t, EasingMode mode);
Vector2 UpdateValueByEasing(Vector2 startValue, Vector2 endValue, float easedT);
int UpdateValueByEasing(int startValue, int endValue, float easedT);
float UpdateValueByEasing(float startValue, float endValue, float easedT);

void DrawHitBox(Vector2 hitVertex0, Vector2 hitVertex1);

Vector2i ConvertVector2(Vector2 vector);
Vector2 ConvertVector2(Vector2i vector);
Vector2 AddVector2(Vector2 vector1, Vector2 vector2);
Vector2i AddVector2(Vector2i vector1, Vector2i vector2);

void DrawHitCircle(Vector2 centerPos, float radius, unsigned int color, bool solidSwitch);
void DrawHitBox(Vector2 vertex[kVertexNum], unsigned int color);

int GetRand(int min, int max);
int GetValueAtDigit(int targetNum, int targetDigit);