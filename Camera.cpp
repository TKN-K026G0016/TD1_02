#include "Camera.h"

#include "Vector2.h"
#include "Common.h"
#include "Matrix.h"
#include "Tool.h"


struct Camera {
	//座標
	Vector2 pos = { 640, 360 };
	Vector2 shakeOffset = {};

	//ズーム率
	float zoom = 1.3f;

	//回転量
	float rotateTheta = 0.0f;

	//描画範囲
	Vector2 viewSize = { ToFloat(kWindowSize.x), ToFloat(kWindowSize.y) };

	//画面変換行列
	Matrix3x3 worldToScreenMatrix = {};
};
Camera camera;

/// <summary>
/// 画面変換行列の更新処理
/// </summary>
/// <param name=""></param>
static void UpdateWorldToScreenMatrix(void) {
	//ワールド行列作成
	Matrix3x3 worldMatrix = MakeWorldMatrix({ camera.zoom, camera.zoom }, camera.rotateTheta, camera.pos);
	//ビュー行列
	Matrix3x3 viewMatrix = Inverse(worldMatrix);
	//正射影行列
	Matrix3x3 orthoMatrix = MakeOrthoMatrix(camera.viewSize);
	//ビューポート行列
	Matrix3x3 viewportMatrix = MakeViewportMatrix(camera.viewSize);


	camera.worldToScreenMatrix = Multiply(viewMatrix, orthoMatrix);
	camera.worldToScreenMatrix = Multiply(camera.worldToScreenMatrix, viewportMatrix);
}


void InitCamera(void) {
	camera = {};
}

void UpdateCamera(void) {
	UpdateWorldToScreenMatrix();
}

#pragma region 関数: 外部参照用

/// <summary>
/// カメラの拡大率取得
/// </summary>
/// <returns>camera.zoom</returns>
float GetCameraZoom(void) {
	return camera.zoom;
}

/// <summary>
/// 画面変換行列の取得
/// </summary>
/// <returns>camera.worldToScreenMatrix</returns>
Matrix3x3 GetWorldToScreenMatrix(void) {
	return camera.worldToScreenMatrix;
}

#pragma endregion
