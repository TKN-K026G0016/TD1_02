#include "Camera.h"

#include "Vector2.h"
#include "Common.h"
#include "Matrix.h"
#include "Tool.h"
#include <cmath>


struct Camera {
    //座標
    Vector2 pos = { 640, 360 };
    Vector2 shakeOffset = {};
    int shakeRemaining = 0;
    int shakeTotal = 0;
    float shakePower = 0.0f;
    int shakeTick = 0;
    float followTargetY = 360.0f; // 上下スクロール用の目標値
    float zoomPunch = 0.0f;
    int zoomPunchRemaining = 0;
    int zoomPunchTotal = 0;

    //ズーム率
    float zoom = 1.0f; // 【サイズ厳守】ボス256x256を画面上でも1倍表示

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
    Matrix3x3 worldMatrix = MakeWorldMatrix({ camera.zoom, camera.zoom }, camera.rotateTheta,
        { camera.pos.x + camera.shakeOffset.x, camera.pos.y + camera.shakeOffset.y });
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

void SetCameraFollowY(float playerWorldY) {
    // 【ステージ1】固定カメラ。ワイヤー回避中も視界を上下させない。
    // 関数名はほかのステージが使っても困らないよう残している。
    (void)playerWorldY;
    camera.followTargetY = 360.0f;
}

void UpdateCamera(void) {
    // 座標をいきなり変えず、8%ずつ追従。描画を一切増やさず滑らかにする。
    camera.pos.y += (camera.followTargetY - camera.pos.y) * 0.08f;
    // ボス着地や外装剥がれでは、一瞬だけ世界を拡大する。オブジェクト追加なし。
    if (camera.zoomPunchRemaining > 0) {
        //const float t = camera.zoomPunchRemaining / static_cast<float>(camera.zoomPunchTotal);
        camera.zoom = 1.0f; // 背景も含めて描画1倍を維持。衝撃は揺れで演出。
        --camera.zoomPunchRemaining;
    }
    else {
        camera.zoom = 1.0f;
    }
    if (camera.shakeRemaining > 0) {
        float decay = static_cast<float>(camera.shakeRemaining) /
            static_cast<float>(camera.shakeTotal > 0 ? camera.shakeTotal : 1);
        ++camera.shakeTick;
        camera.shakeOffset.x = std::sin(camera.shakeTick * 2.37f) * camera.shakePower * decay;
        camera.shakeOffset.y = std::cos(camera.shakeTick * 1.71f) * camera.shakePower * decay;
        --camera.shakeRemaining;
    }
    else {
        camera.shakeOffset = {};
    }
    UpdateWorldToScreenMatrix();
}

void PunchCamera(float zoomAmount, int frames) {
    if (zoomAmount <= 0.0f || frames <= 0) return;
    camera.zoomPunch = zoomAmount;
    camera.zoomPunchRemaining = frames;
    camera.zoomPunchTotal = frames;
}

void ShakeCamera(float power, int frames) {
    if (frames <= 0 || power <= 0.0f) return;
    camera.shakePower = power;
    camera.shakeTotal = frames;
    camera.shakeRemaining = frames;
    camera.shakeTick = 0;
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
