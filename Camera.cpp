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
    bool bossMashCamera = false;
    Vector2 mashFocus = { 640.0f,360.0f };
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

void SetBossMashCamera(bool enabled, float bossX, float bossY) {
    camera.bossMashCamera = enabled;
    camera.mashFocus = { bossX,bossY };
}

void SetCameraFollowY(float playerWorldY) {
    // 【ステージ1】固定カメラ。ワイヤー回避中も視界を上下させない。
    // 関数名はほかのステージが使っても困らないよう残している。
    (void)playerWorldY;
    camera.followTargetY = 360.0f;
}

void UpdateCamera(void) {
    // 座標をいきなり変えず、8%ずつ追従。描画を一切増やさず滑らかにする。
    const float goalZoom = camera.bossMashCamera ? 1.28f : 1.0f;
    // 【注意】通常時の描画倍率は1.0。連打中だけカメラを一時的に寄せる。
    camera.zoom += (goalZoom - camera.zoom) * 0.12f;
    if (std::fabs(camera.zoom - goalZoom) < 0.001f)camera.zoom = goalZoom;
    const float focusX = camera.bossMashCamera ? camera.mashFocus.x : 640.0f;
    const float focusY = camera.bossMashCamera ? camera.mashFocus.y : camera.followTargetY;
    camera.pos.x += (focusX - camera.pos.x) * 0.12f;
    camera.pos.y += (focusY - camera.pos.y) * 0.12f;
    // ボス着地や外装剥がれでは、一瞬だけ世界を拡大する。オブジェクト追加なし。
    if (camera.zoomPunchRemaining > 0) {
        // ズームは上の専用補間で制御。既存のPunch演出はシェイク優先。
        --camera.zoomPunchRemaining;
    }
    else {
        // 通常時は上の補間で1.0fへ戻す。
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
