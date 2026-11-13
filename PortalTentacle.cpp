#include "PortalTentacle.h"
#include "PortalMath.h"
#include "PortalDraw.h"
#include "PortalConfig.h"
#include <cmath>

using namespace PortalMath;

namespace {
    constexpr float kPi = 3.14159265f;
    constexpr float kTau = 6.28318530f;
    constexpr int kTendrilSteps = 6; // 【軽量化】9→6段。3本のうねりは残す
    constexpr int kRingSteps = 8; // 【軽量化】輪の頂点を12→8。装飾線を約1/3削減

    float EaseOutCubic(float t) {
        t = Clamp(t, 0.0f, 1.0f);
        return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    }
}

void PortalTentacle::Start(Vector2 origin, Vector2 direction, const Vector2* anchor) {
    origin_ = origin;
    direction_ = Unit(direction);
    end_ = anchor ? *anchor : Add(origin_, Mul(direction_, PortalConfig::kAttackReach));
    hasAnchor_ = anchor != nullptr;
    armorTether_ = false;
    frame_ = 0;
    poseHoldFrames_ = 0;
    amount_ = 0.0f;
    releaseAmount_ = 0.0f;
    motionTime_ = 0.0f;
    phaseOffset_ = std::fmod(std::fabs(origin.x * 0.073f + origin.y * 0.041f), kTau);
    state_ = TentacleState::Extending;
}

void PortalTentacle::StartArmorTether(Vector2 origin, Vector2 armorPoint) {
    const Vector2 direction = Unit(Sub(armorPoint, origin));
    Start(origin, direction, &armorPoint);
    armorTether_ = true; // Startの後で指定する。これが通常ワイヤーと違う点。
}
void PortalTentacle::SetArmorTarget(Vector2 point) {
    if (armorTether_ && IsActive()) end_ = point;
}

void PortalTentacle::Release() {
    if (state_ == TentacleState::Inactive || state_ == TentacleState::Retracting) return;
    releaseAmount_ = amount_;
    frame_ = 0;
    state_ = TentacleState::Retracting;
}

void PortalTentacle::HoldAttackPose(int frames) {
    // 【演出用】グリップ中のワイヤーに止め絵を入れない。
    if (!IsActive() || hasAnchor_) return;
    if (frames > poseHoldFrames_) poseHoldFrames_ = frames;
}

void PortalTentacle::Update() {
    if (!IsActive()) return;
    // 【重要】画面に表示する触手だけ3F程度停止させる。
    // プレイヤーの位置・重力・グラップル物理はいつも通り更新する。
    if (poseHoldFrames_ > 0 && !hasAnchor_) {
        --poseHoldFrames_;
        return;
    }
    motionTime_ += 1.0f;
    ++frame_;
    switch (state_) {
    case TentacleState::Extending:
        // 杭に命中したグラップルはJを離しても自動で伸び続ける。
        // 杭へ近付くとPlayer側から自動Releaseされて次の杭へ射出。
        amount_ = EaseOutCubic(frame_ / static_cast<float>(PortalConfig::kTentacleExtendFrames));
        if (frame_ >= PortalConfig::kTentacleExtendFrames) {
            amount_ = 1.0f;
            frame_ = 0;
            state_ = TentacleState::Holding;
        }
        break;
    case TentacleState::Holding:
        // 通常攻撃だけ自動収納。杭に接続中は自動射出まで維持する。
        if (!hasAnchor_ && frame_ >= 9) Release();
        break;
    case TentacleState::Retracting: {
        float remaining = 1.0f - Clamp(frame_ / static_cast<float>(PortalConfig::kTentacleRetractFrames), 0.0f, 1.0f);
        amount_ = releaseAmount_ * remaining * remaining;
        if (frame_ >= PortalConfig::kTentacleRetractFrames) {
            amount_ = 0.0f;
            state_ = TentacleState::Inactive;
        }
        break;
    }
    default: break;
    }
}

void PortalTentacle::FollowOwner(Vector2 playerPosition) {
    if (!IsActive()) return;
    // The portal floats BEHIND the player, following with spring-like lag.
    // This affects only the portal's visual origin, not the fixed grip anchor.
    const Vector2 desiredOrigin = Sub(playerPosition, Mul(direction_, 66.0f));
    const float followRate = IsGripping() ? 0.26f : 0.17f;
    origin_ = Lerp(origin_, desiredOrigin, followRate);
}

// -------------------------------------------------------------
// 【攻撃判定】細い線分に厚みを持たせて円と当たるかを調べる。
// 演出のうねりと判定を一致させすぎると判定が不安定になるため、
// 攻撃の主軸とグリップの綱だけを判定する。毎フレーム利用可能。
// -------------------------------------------------------------
bool PortalTentacle::HitsCircle(Vector2 target, float radius) const {
    if (!IsActive() || amount_ < 0.25f || armorTether_) return false;
    // 戻る途中は攻撃しない（見た目だけ）。
    if (state_ == TentacleState::Retracting) return false;
    const Vector2 a = origin_;
    const Vector2 b = hasAnchor_ ? Lerp(origin_, end_, amount_)
        : Add(origin_, Mul(direction_, PortalConfig::kAttackReach * amount_));
    const Vector2 segment = Sub(b, a);
    const float lengthSquared = PortalMath::Dot(segment, segment);
    if (lengthSquared < 0.0001f) return false;
    const float t = Clamp(PortalMath::Dot(Sub(target, a), segment) / lengthSquared, 0.0f, 1.0f);
    const Vector2 closest = Add(a, Mul(segment, t));
    const float thickness = hasAnchor_ ? 14.0f : 22.0f;
    const Vector2 delta = Sub(target, closest);
    const float hitRadius = radius + thickness;
    return PortalMath::Dot(delta, delta) <= hitRadius * hitRadius;
}

void PortalTentacle::Draw(Vector2 playerPosition) const {
    if (!IsActive()) return;

    const float phase = motionTime_ * 0.17f + phaseOffset_;
    const float active = Clamp(amount_ * 1.25f, 0.0f, 1.0f);
    const float pulse = 0.5f + 0.5f * std::sin(phase * 2.7f);
    const int branchCount = hasAnchor_ ? 1 : 3; // Attack: three lively whips; grip: one clear tether.
    const Vector2 portalNormal = { -direction_.y, direction_.x };

    // Three limbs have different phases, so they do not move like one stiff rod.
    // 【描画制限】1枝9区間×2色のラインだけ。大量の三角形を発生させない。
    for (int branch = 0; branch < branchCount; ++branch) {
        const float fan = (branch - (branchCount - 1) * 0.5f) * 0.25f;
        const Vector2 branchDirection = Rotate(direction_, fan);
        const Vector2 target = hasAnchor_ ? end_ : Add(origin_, Mul(branchDirection, PortalConfig::kAttackReach * (0.87f + branch * 0.07f)));
        const float extension = amount_;
        Vector2 tip = Lerp(origin_, target, extension);
        Vector2 travel = Unit(Sub(tip, origin_));
        Vector2 normal = { -travel.y, travel.x };
        if (hasAnchor_ && (IsGripping() || armorTether_)) tip = end_;

        // The attack tip overshoots and curls. The grip stays locked to the anchor.
        if (!hasAnchor_) {
            const float tipSway = (12.0f + 9.0f * std::sin(phase * 1.6f + branch * 1.4f)) * active;
            tip = Add(tip, Mul(normal, tipSway));
        }
        Vector2 last = origin_;
        for (int i = 1; i <= kTendrilSteps; ++i) {
            const float t = static_cast<float>(i) / kTendrilSteps;
            const float envelope = std::sin(t * kPi);
            const float travelWave = std::sin(t * 9.0f - phase * 2.4f + branch * 1.7f);
            const float slowWave = std::sin(t * 3.8f + phase * 1.15f + branch);
            const float gripFactor = hasAnchor_ ? 0.44f : 1.0f;
            const float bend = (travelWave * 21.0f + slowWave * 10.0f) * envelope * gripFactor * active;
            Vector2 now = Add(Lerp(origin_, tip, t), Mul(normal, bend));
            // Outlines and inner reflection, all just lines (no sprites or polygons).
            // 黒い輪郭線は省略（触手が複数本あると描画数上限に近づくため）
            PortalDraw::Line(last, now, 0x66208DFF);
            PortalDraw::Line(Add(last, Mul(normal, 1.7f)), Add(now, Mul(normal, 1.7f)), 0xC96DFFFF);
            last = now;
        }
        // Animated curled tip; short, low-cost segments give the whip a living silhouette.
        if (!hasAnchor_ && active > 0.18f) {
            Vector2 curl = tip;
            const float curlAngle = (branch - 1) * 0.7f + 0.85f * std::sin(phase * 1.5f + branch);
            Vector2 curlDir = Rotate(travel, curlAngle);
            for (int i = 0; i < 2; ++i) {
                curlDir = Rotate(curlDir, 0.27f * (branch % 2 ? 1.0f : -1.0f));
                Vector2 next = Add(curl, Mul(curlDir, (8.0f - i * 2.0f) * active));
                PortalDraw::Line(curl, next, 0xC96DFFFF);
                curl = next;
            }
        }
        if (hasAnchor_) {
            // Pulse at the hook, but keep the tip firmly connected to its anchor.
            const float size = (9.0f + pulse * 5.0f) * active;
            PortalDraw::Box(end_, size, 3.0f, 0xF5C8FFFF);
            PortalDraw::Box(end_, 3.0f, size, 0xF5C8FFFF);
        }
    }

    // Continuous rotating, breathing magic gate: two 20-segment ellipses + 6 spokes.
    const float open = Clamp(amount_ * 3.0f, 0.04f, 1.0f);
    const float breathing = 1.0f + 0.045f * std::sin(phase * 2.0f);
    for (int ring = 0; ring < 2; ++ring) {
        Vector2 last = {};
        for (int i = 0; i <= kRingSteps; ++i) {
            const float angle = (i / static_cast<float>(kRingSteps)) * kTau + (ring == 0 ? phase * 0.48f : -phase * 0.72f);
            const float width = (41.0f + ring * 5.0f) * open * breathing;
            const float depth = (13.0f + ring * 2.0f) * open;
            Vector2 now = Add(origin_, Add(Mul(portalNormal, std::cos(angle) * width), Mul(direction_, std::sin(angle) * depth)));
            if (i > 0) PortalDraw::Line(last, now, ring == 0 ? 0xAB38EAFF : 0xEDD0FFFF);
            last = now;
        }
    }
    for (int i = 0; i < 4; ++i) {
        const float angle = i * kTau / 4.0f + phase * 0.65f;
        const Vector2 start = Add(origin_, Add(Mul(portalNormal, std::cos(angle) * 25.0f * open), Mul(direction_, std::sin(angle) * 7.0f * open)));
        const Vector2 finish = Add(origin_, Add(Mul(portalNormal, std::cos(angle) * 35.0f * open), Mul(direction_, std::sin(angle) * 10.0f * open)));
        PortalDraw::Line(start, finish, 0xE5AAFFFF);
    }

    // A short flowing bridge from the player's back to the floating gate.
    if (hasAnchor_ && IsGripping()) {
        Vector2 last = playerPosition;
        const Vector2 delta = Sub(origin_, playerPosition);
        const float distance = Length(delta);
        const Vector2 bridgeNormal = { -direction_.y, direction_.x };
        for (int i = 1; i <= 3; ++i) {
            const float t = i / 3.0f;
            Vector2 now = Add(Lerp(playerPosition, origin_, t),
                Mul(bridgeNormal, std::sin(t * kPi) * std::sin(phase * 2.2f - t * 2.0f) * Clamp(distance * 0.07f, 0.0f, 7.0f)));
            PortalDraw::Line(last, now, 0xA957DEFF);
            last = now;
        }
    }
}
