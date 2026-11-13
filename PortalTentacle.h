#pragma once
#include "Vector2.h"

// 触手の寿命とアニメーション状態。
enum class TentacleState { Inactive, Extending, Holding, Retracting };

class PortalTentacle {
public:
    void Start(Vector2 origin, Vector2 direction, const Vector2* anchor);
    void StartArmorTether(Vector2 origin, Vector2 armorPoint); // ボス外装：プレイヤーは引き寄せない
    void SetArmorTarget(Vector2 point); // ボスが動いても外装の取っ掛かりを追う
    bool IsArmorTether() const { return armorTether_ && IsActive(); }
    // 開始したグラップルはボタンを離しても維持。Playerが近距離で自動解除。
    void Update();
    // 【ヒットストップ】描画アニメーションだけを停止。移動・グリップ物理は止めない。
    void HoldAttackPose(int frames);
    bool IsRetracting() const { return state_ == TentacleState::Retracting; }
    void FollowOwner(Vector2 playerPosition);
    void Release();
    void Draw(Vector2 playerPosition) const;

    bool IsActive() const { return state_ != TentacleState::Inactive; }
    // 接続した瞬間から巻き取り開始：先端の見た目が伸びている間も移動できる（描画追加なし）。
    bool IsGripping() const {
        return hasAnchor_ &&
            (state_ == TentacleState::Holding || state_ == TentacleState::Extending) && !armorTether_;
    }
    bool IsGrapple() const { return hasAnchor_ && !armorTether_; }
    Vector2 GetAnchor() const { return end_; }

    // 【当たり判定】触手を線分（太さ付き）として判定。グリップ中もtrueを返す。
    // anchorがある時は杭までの線、無い時は通常攻撃の線を調べる。
    bool HitsCircle(Vector2 target, float radius) const;

private:
    TentacleState state_ = TentacleState::Inactive;
    Vector2 origin_ = {}, direction_ = { 1.0f, 0.0f }, end_ = {};
    float amount_ = 0.0f;
    float releaseAmount_ = 0.0f;
    float motionTime_ = 0.0f;
    float phaseOffset_ = 0.0f;
    int frame_ = 0;
    int poseHoldFrames_ = 0;
    bool hasAnchor_ = false;
    bool armorTether_ = false; // 取っ掛かりへ撃つがプレイヤー自身は移動しない
};
