#include "tool.h"
#include "vector2.h"
#include "common.h"
#include "matrix.h"
#include "Camera.h"

#include <Novice.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

void InitTool(void) {
	srand(ToInt(time(nullptr)));
}

/// <summary>
/// 頂点座標の取得
/// </summary>
/// <param name="pos">中心座標</param>
/// <param name="size">サイズ</param>
/// <param name="rotateTheta">回転量</param>
/// <param name="vertex">頂点座標格納データ</param>
void SetVertex(Vector2 pos, Vector2 size, float rotateTheta, Vector2 vertex[kVertexNum]) {
	Matrix3x3 rotateMatrix = MakeRotateMatrix(rotateTheta);

	for (int i = 0; i < kVertexNum; i++) {
		//ローカル座標(中心座標が原点の際)
		Vector2 local = { 0, 0 };
		switch (i) {
		case 0:
			local = { -size.x / 2.0f, size.y / 2.0f };
			break;
		case 1:
			local = { size.x / 2.0f, size.y / 2.0f };
			break;
		case 2:
			local = { -size.x / 2.0f, -size.y / 2.0f };
			break;
		case 3:
			local = { size.x / 2.0f, -size.y / 2.0f };
			break;
		}

		//ローカル座標が回転後の座標
		Vector2 rotatedVertex = Transform(local, rotateMatrix);
		//ワールド座標に戻して、変更
		vertex[i] = { pos.x + rotatedVertex.x, pos.y + rotatedVertex.y };
	}
}

#pragma region 関数: 描画関係

/// <summary>
/// ワールドからスクリーン座標への変換
/// </summary>
/// <param name="posW">ワールド座標</param>
/// <returns>スクリーン座標</returns>
Vector2i ConvertPosWToSUI(Vector2 posW) {

	Vector2 posS = { 0, 0 };
	// まず通常のスクリーン座標へ変換
	posS.x = (posW.x + kDelayBetweenWToS.x);
	posS.y = (-posW.y + kDelayBetweenWToS.y);

	return ConvertVector2(posS);
}

#pragma endregion

#pragma region 関数: 行列関係

/// <summary>
/// Matrix3x3同士の積
/// </summary>
/// <param name="matrix1">行列1</param>
/// <param name="matrix2">行列2</param>
/// <returns>積</returns>
Matrix3x3 Multiply(Matrix3x3 matrix1, Matrix3x3 matrix2) {
	Matrix3x3 result = {};
	result.m[0][0] = matrix1.m[0][0] * matrix2.m[0][0] + matrix1.m[0][1] * matrix2.m[1][0] + matrix1.m[0][2] * matrix2.m[2][0];
	result.m[0][1] = matrix1.m[0][0] * matrix2.m[0][1] + matrix1.m[0][1] * matrix2.m[1][1] + matrix1.m[0][2] * matrix2.m[2][1];
	result.m[0][2] = matrix1.m[0][0] * matrix2.m[0][2] + matrix1.m[0][1] * matrix2.m[1][2] + matrix1.m[0][2] * matrix2.m[2][2];

	result.m[1][0] = matrix1.m[1][0] * matrix2.m[0][0] + matrix1.m[1][1] * matrix2.m[1][0] + matrix1.m[1][2] * matrix2.m[2][0];
	result.m[1][1] = matrix1.m[1][0] * matrix2.m[0][1] + matrix1.m[1][1] * matrix2.m[1][1] + matrix1.m[1][2] * matrix2.m[2][1];
	result.m[1][2] = matrix1.m[1][0] * matrix2.m[0][2] + matrix1.m[1][1] * matrix2.m[1][2] + matrix1.m[1][2] * matrix2.m[2][2];

	result.m[2][0] = matrix1.m[2][0] * matrix2.m[0][0] + matrix1.m[2][1] * matrix2.m[1][0] + matrix1.m[2][2] * matrix2.m[2][0];
	result.m[2][1] = matrix1.m[2][0] * matrix2.m[0][1] + matrix1.m[2][1] * matrix2.m[1][1] + matrix1.m[2][2] * matrix2.m[2][1];
	result.m[2][2] = matrix1.m[2][0] * matrix2.m[0][2] + matrix1.m[2][1] * matrix2.m[1][2] + matrix1.m[2][2] * matrix2.m[2][2];

	return result;
}

/// <summary>
/// Rotate行列作成
/// </summary>
/// <param name="theta">回転量</param>
/// <returns>Rotate行列</returns>
Matrix3x3 MakeRotateMatrix(float theta) {
	Matrix3x3 result = {};

	result.m[0][0] = static_cast<float>(cosf(theta));
	result.m[0][1] = sinf(theta);
	result.m[0][2] = 0;

	result.m[1][0] = -sinf(theta);
	result.m[1][1] = cosf(theta);
	result.m[1][2] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;

	return result;
}

/// <summary>
/// Scale行列作成
/// </summary>
/// <param name="size">スケール</param>
/// <returns>Scale行列</returns>
Matrix3x3 MakeScaleMatrix(Vector2 size) {
	Matrix3x3 result = {};

	result.m[0][0] = size.x;
	result.m[0][1] = 0;
	result.m[0][2] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = size.y;
	result.m[1][2] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;

	return result;
}

/// <summary>
/// Translate行列作成
/// </summary>
/// <param name="translate">座標</param>
/// <returns>Translate行列</returns>
Matrix3x3 MakeTranslateMatrix(Vector2 translate) {
	Matrix3x3 result = {};
	result.m[0][0] = 1;
	result.m[0][1] = 0;
	result.m[0][2] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = 1;
	result.m[1][2] = 0;

	result.m[2][0] = translate.x;
	result.m[2][1] = translate.y;
	result.m[2][2] = 1;

	return result;
}

/// <summary>
/// ワールド行列の作成
/// </summary>
/// <param name="scale">スケール</param>
/// <param name="rotateTheta">回転量</param>
/// <param name="pos">座標</param>
/// <returns>ワールド行列</returns>
Matrix3x3 MakeWorldMatrix(Vector2 scale, float rotateTheta, Vector2 pos) {
	Matrix3x3 scaleMatrix = MakeScaleMatrix(scale);
	Matrix3x3 rotateMatrix = MakeRotateMatrix(rotateTheta);
	Matrix3x3 translateMatrix = MakeTranslateMatrix(pos);
	Matrix3x3 worldMatrix = Multiply(scaleMatrix, rotateMatrix);
	worldMatrix = Multiply(worldMatrix, translateMatrix);

	return worldMatrix;
}

/// <summary>
/// 逆行列の作成
/// </summary>
/// <param name="matrix">元のMatrix</param>
/// <returns>逆行列</returns>
Matrix3x3 Inverse(Matrix3x3 matrix) {
	Matrix3x3 result = {};

	float det = (matrix.m[0][0] * matrix.m[1][1] * matrix.m[2][2]) + (matrix.m[0][1] * matrix.m[1][2] * matrix.m[2][0]) + (matrix.m[0][2] * matrix.m[1][0] * matrix.m[2][1])
		- (matrix.m[0][2] * matrix.m[1][1] * matrix.m[2][0]) - (matrix.m[0][1] * matrix.m[1][0] * matrix.m[2][2]) - (matrix.m[0][0] * matrix.m[1][2] * matrix.m[2][1]);
	if (det != 0) {
		result.m[0][0] = (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1]) / det;
		result.m[0][1] = -(matrix.m[0][1] * matrix.m[2][2] - matrix.m[0][2] * matrix.m[2][1]) / det;
		result.m[0][2] = (matrix.m[0][1] * matrix.m[1][2] - matrix.m[0][2] * matrix.m[1][1]) / det;

		result.m[1][0] = -(matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0]) / det;
		result.m[1][1] = (matrix.m[0][0] * matrix.m[2][2] - matrix.m[0][2] * matrix.m[2][0]) / det;
		result.m[1][2] = -(matrix.m[0][0] * matrix.m[1][2] - matrix.m[0][2] * matrix.m[1][0]) / det;

		result.m[2][0] = (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]) / det;
		result.m[2][1] = -(matrix.m[0][0] * matrix.m[2][1] - matrix.m[0][1] * matrix.m[2][0]) / det;
		result.m[2][2] = (matrix.m[0][0] * matrix.m[1][1] - matrix.m[0][1] * matrix.m[1][0]) / det;
	}

	return result;
}

/// <summary>
/// 
/// </summary>
/// <param name="viewSize"></param>
/// <returns></returns>
Matrix3x3 MakeOrthoMatrix(Vector2 viewSize) {
	float left = -viewSize.x / 2;
	float right = viewSize.x / 2;
	float bottom = -viewSize.y / 2;
	float top = viewSize.y / 2;

	Matrix3x3 result = {};
	result.m[0][0] = 2.0f / (right - left);
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[1][2] = 0.0f;

	result.m[2][0] = (left + right) / (left - right);
	result.m[2][1] = (top + bottom) / (bottom - top);
	result.m[2][2] = 1.0f;

	return result;
}

/// <summary>
/// ビューポート行列作成
/// </summary>
/// <param name="viewSize"></param>
/// <returns>ビューポート行列</returns>
Matrix3x3 MakeViewportMatrix(Vector2 viewSize) {
	Matrix3x3 result = {};

	result.m[0][0] = viewSize.x / 2.0f;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = -viewSize.y / 2.0f;
	result.m[1][2] = 0.0f;

	result.m[2][0] = /*left + */viewSize.x / 2.0f;
	result.m[2][1] = /*top + */viewSize.y / 2.0f;
	result.m[2][2] = 1.0f;

	return result;
}

/// <summary>
/// Vector2とMatrixの積
/// </summary>
/// <param name="vector"></param>
/// <param name="matrix"></param>
/// <returns></returns>
Vector2 Transform(Vector2 vector, Matrix3x3 matrix) {
	Vector2 result = {};
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + 1.0f * matrix.m[2][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + 1.0f * matrix.m[2][1];
	float w = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + 1.0f * matrix.m[2][2];

	assert(w != 0.0f);	//0.0の場合、プログラムを停止する

	result.x /= w;
	result.y /= w;
	return result;
}


#pragma endregion

#pragma region 関数: 接触判定

/// <summary>
/// 点と線分の最短距離を求める
/// </summary>
/// <param name="p">点の座標</param>
/// <param name="a">線分の始点</param>
/// <param name="b">線分の終点</param>
/// <returns></returns>
float DistancePointToSegment(Vector2 p, Vector2 a, Vector2 b) {
	//A→Bのベクトル
	Vector2 vectorAtoB = { b.x - a.x, b.y - a.y };
	//A→Pのベクトル
	Vector2 vectorAtoP = { p.x - a.x, p.y - a.y };

	float abLenSq = vectorAtoB.x * vectorAtoB.x + vectorAtoB.y * vectorAtoB.y;
	float t = (vectorAtoP.x * vectorAtoB.x + vectorAtoP.y * vectorAtoB.y) / abLenSq;

	t = std::clamp(t, 0.0f, 1.0f);

	Vector2 closest = { a.x + vectorAtoB.x * t, a.y + vectorAtoB.y * t };

	float dx = p.x - closest.x;
	float dy = p.y - closest.y;

	return sqrtf(dx * dx + dy * dy);
}

/// <summary>
/// CircleVSCircleの当たり判定
/// </summary>
/// <param name="circle1Pos">円1の座標</param>
/// <param name="radius1">円1の半径</param>
/// <param name="circle2Pos">円2の座標</param>
/// <param name="radius2">円2の半径</param>
/// <returns>接触しているか</returns>
bool CheckCollisionCircleVSCircle(Vector2 circle1Pos, float radius1, Vector2 circle2Pos, float radius2) {
	float disX = circle1Pos.x - circle2Pos.x;
	float disY = circle1Pos.y - circle2Pos.y;
	float disTotal = sqrtf(disX * disX + disY * disY);

	float disRad = radius1 + radius2;

	if (disTotal <= disRad) {
		return true;
	} else {
		return false;
	}

}

/// <summary>
/// 矩形と円の当たり判定
/// </summary>
/// <param name="circlePos">円の座標</param>
/// <param name="circleRadius">円の半径</param>
/// <param name="boxVertex0">矩形の頂点座標(左上)</param>
/// <param name="boxVertex3">矩形の頂点座標(右下)</param>
/// <returns>衝突しているか</returns>
bool CheckCollisionBoxVSCircle(Vector2 circlePos, float circleRadius, Vector2 vertex[4]) {

	//円が四角の内部にあるかの判定
	Vector2 edgeX = { vertex[1].x - vertex[0].x, vertex[1].y - vertex[0].y }; // 左上から右上へのベクトル
	Vector2 edgeY = { vertex[2].x - vertex[0].x, vertex[2].y - vertex[0].y }; // 左上から左下へのベクトル

	// 円の中心から左上（vertex[0]）へのベクトル
	Vector2 v = { circlePos.x - vertex[0].x, circlePos.y - vertex[0].y };

	// 各辺の長さの2乗
	float edgeXLenSq = edgeX.x * edgeX.x + edgeX.y * edgeX.y;
	float edgeYLenSq = edgeY.x * edgeY.x + edgeY.y * edgeY.y;

	// 内積を用いて、各軸上にどれだけ進んでいるか（射影）を計算
	float dotX = v.x * edgeX.x + v.y * edgeX.y;
	float dotY = v.x * edgeY.x + v.y * edgeY.y;

	if (dotX >= 0.0f && dotX <= edgeXLenSq && dotY >= 0.0f && dotY <= edgeYLenSq) {
		return true;
	}

	// 4辺の線分距離判定
	for (int i = 0; i < 4; i++) {
		Vector2 a = vertex[i];
		Vector2 b = vertex[(i + 1) % 4];

		float dist = DistancePointToSegment(circlePos, a, b);
		if (dist <= circleRadius) {
			return true;
		}
	}

	return false;
}

#pragma endregion

#pragma region 関数: イージング関係

/// <summary>
/// EasedTの作成関数
/// </summary>
/// <param name="t">媒介変数</param>
/// <param name="mode">イージングモード</param>
/// <returns>easedT</returns>
float CreateEasedT(float t, EasingMode mode) {
	float easedT = 0.0f;

	switch (mode) {
	case EasingMode::Linear:
	{
		easedT = t;

		if (easedT > 1.0f) {
			easedT = 1.0f;
		}

		break;
	}
	case  EasingMode::EaseIn:
	{
		easedT = t * t;

		if (easedT > 1.0f) {
			easedT = 1.0f;
		}

		break;
	}
	case  EasingMode::EaseOut:
	{
		easedT = 1.0f - powf(1.0f - t, 3.0f);

		if (easedT > 1.0f) {
			easedT = 1.0f;
		}

		break;
	}
	case  EasingMode::EaseInOut:
	{
		easedT = -(cosf(static_cast<float>(M_PI) * t) - 1.0f) / 2.0f;

		if (easedT > 1.0f) {
			easedT = 1.0f;
		}

		break;
	}
	case  EasingMode::EaseInBack:
	{
		float c1 = kOvershootAmount;
		float c3 = c1 + 1;

		easedT = c3 * t * t * t - c1 * t * t;

		if (easedT > 1.0f) {
			easedT = 1.0f;
		}

		break;
	}
	case  EasingMode::EaseOutBack:
	{
		float c1 = kOvershootAmount;
		float c3 = c1 + 1;

		easedT = 1 + c3 * static_cast<float>(pow(t - 1, 3)) + c1 * static_cast<float>(pow(t - 1, 2));

		break;
	}
	}

	return easedT;
}

/// <summary>
/// イージングでの値更新(Vector2)
/// </summary>
/// <param name="startValue">0.0f時の値</param>
/// <param name="endValue">1.0f時の値</param>
/// <param name="easedT">easedT</param>
/// <returns></returns>
Vector2 UpdateValueByEasing(Vector2 startValue, Vector2 endValue, float easedT) {
	return { (1.0f - easedT) * startValue.x + easedT * endValue.x,(1.0f - easedT) * startValue.y + easedT * endValue.y };
}

/// <summary>
/// イージングでの値更新(int)
/// </summary>
/// <param name="startValue">0.0f時の値</param>
/// <param name="endValue">1.0f時の値</param>
/// <param name="easedT">easedT</param>
/// <returns></returns>
int UpdateValueByEasing(int startValue, int endValue, float easedT) {
	return ToInt((1.0f - easedT) * startValue + easedT * endValue);
}

/// <summary>
/// イージングでの値更新(float)
/// </summary>
/// <param name="startValue">0.0f時の値</param>
/// <param name="endValue">1.0f時の値</param>
/// <param name="easedT">easedT</param>
/// <returns></returns>
float UpdateValueByEasing(float startValue, float endValue, float easedT) {
	return (1.0f - easedT) * startValue + easedT * endValue;
}

#pragma endregion

#pragma region 関数: データ変換関係

/// <summary>
/// Vector2→Vector2iに変換
/// </summary>
/// <param name="vector">Vector2</param>
/// <returns>Vector2i</returns>
Vector2i ConvertVector2(Vector2 vector) {
	return { ToInt(vector.x), ToInt(vector.y) };
}

/// <summary>
/// Vector2i→Vector2に変換
/// </summary>
/// <param name="vector">Vector2i</param>
/// <returns>Vector2</returns>
Vector2 ConvertVector2(Vector2i vector) {
	return { ToFloat(vector.x), ToFloat(vector.y) };
}

/// <summary>
/// Vector2の加算処理
/// </summary>
/// <param name="vector1">vector1</param>
/// <param name="vector2">vector2</param>
/// <returns>加算結果</returns>
Vector2 AddVector2(Vector2 vector1, Vector2 vector2) {
	Vector2 result = { vector1.x + vector2.x, vector1.y + vector2.y };
	return result;
}

/// <summary>
/// Vector2iの加算処理
/// </summary>
/// <param name="vector1">vector1</param>
/// <param name="vector2">vector2</param>
/// <returns>加算結果</returns>
Vector2i AddVector2(Vector2i vector1, Vector2i vector2) {
	Vector2i result = { vector1.x + vector2.x, vector1.y + vector2.y };
	return result;
}


#pragma endregion

#pragma region デバッグ関係

/// <summary>
/// 円の当たり判定表示
/// </summary>
/// <param name="centerPos">中心座標</param>
/// <param name="radius">半径</param>
/// <param name="color">表示色</param>
/// <param name="solidSwitch">塗りつぶしスイッチ</param>
void DrawHitCircle(Vector2 centerPos, float radius, unsigned int color, bool solidSwitch) {
	Vector2i posS = ConvertVector2(Transform(centerPos, GetWorldToScreenMatrix()));

	int viewRadius = ToInt(radius * GetCameraZoom());

	unsigned int viewColor = (color & 0xFFFFFF00) | kDebugHitColorAlpha;

	if (solidSwitch) {
		Novice::DrawEllipse(
			posS.x, posS.y, viewRadius, viewRadius, 0.0f, viewColor, kFillModeSolid
		);
	} else {
		Novice::DrawEllipse(
			posS.x, posS.y, viewRadius, viewRadius, 0.0f, viewColor, kFillModeWireFrame
		);
	}
}

/// <summary>
/// 矩形の当たり判定表示
/// </summary>
/// <param name="vertex">頂点座標</param>
/// <param name="color">表示色</param>
void DrawHitBox(Vector2 vertex[kVertexNum], unsigned int color) {
	Vector2i vertexS[kVertexNum] = {};
	for (int i = 0; i < kVertexNum; i++) {
		vertexS[i] = ConvertVector2(Transform(vertex[i], GetWorldToScreenMatrix()));
	}

	unsigned int lineColor = (color & 0xFFFFFF00) | kDebugHitColorAlpha;

	Novice::DrawLine(vertexS[0].x, vertexS[0].y, vertexS[1].x, vertexS[1].y, lineColor);
	Novice::DrawLine(vertexS[1].x, vertexS[1].y, vertexS[3].x, vertexS[3].y, lineColor);
	Novice::DrawLine(vertexS[3].x, vertexS[3].y, vertexS[2].x, vertexS[2].y, lineColor);
	Novice::DrawLine(vertexS[2].x, vertexS[2].y, vertexS[0].x, vertexS[0].y, lineColor);
}

#pragma endregion

#pragma region 関数: その他

/// <summary>
/// ランダムな数値を取得
/// </summary>
/// <param name="min">最小値</param>
/// <param name="max">最大値</param>
/// <returns>ランダムな値</returns>
int GetRand(int min, int max) {
	if (min >= max) {
		return 0;
	}

	return (rand() % (max - min + 1)) + min;
};

/// <summary>
/// 指定の桁の数値を取得
/// </summary>
/// <param name="targetNum">目的の数値</param>
/// <param name="targetDigit">取りたい桁(右端は0)</param>
/// <returns>取りたい桁の数値</returns>
int GetValueAtDigit(int targetNum, int targetDigit) {
	int digit;

	digit = targetNum / static_cast<int>(pow(10, targetDigit));
	digit %= 10;

	return digit;
}

#pragma endregion
