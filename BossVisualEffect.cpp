#include "BossVisualEffect.h"
#include "PortalDraw.h"
#include "Camera.h"
#include "Common.h"
#include "BossStage1Tuning.h"
#include <Novice.h>
#include <cmath>

namespace {
    float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
}

// 【超軽量版】このファイルには粒子や大量の線分を描かない。
// 描画回数を使わず、BOXの位置・横幅・縦幅・タイミングで迫力を作る。
// Boss.h側でAIと実体の衝突を管理し、ここは見た目のみ。
float BossVisualEffect::Random01() {
    randomSeed_ = randomSeed_ * 1664525u + 1013904223u;
    return static_cast<float>((randomSeed_ >> 8) & 0xffffffu) / 16777215.0f;
}
void BossVisualEffect::Emit(Vector2, int, int) {
    // 意図的に空にする。描画制限回避のため粒子は使わない。
}
void BossVisualEffect::Initialize(Vector2 bossPosition, float groundY) {
    mode_ = BossVfxMode::Idle;
    globalFrame_ = modeFrame_ = 0;
    impactFrame_ = muzzleFrame_ = -1;
    warningProgress_ = 0;
    rotorAngle_ = 0;
    groundY_ = groundY;
    aimPosition_ = impactPosition_ = muzzlePosition_ = bossPosition;
    lastPosition_ = bossPosition;
    lastVelocityX_ = lastVelocityY_ = 0;
}
void BossVisualEffect::SetMode(BossVfxMode mode) {
    if (mode_ == mode)return;
    mode_ = mode;
    modeFrame_ = 0;
}
void BossVisualEffect::SetWarning(float t, Vector2 aim) { warningProgress_ = Clamp01(t);aimPosition_ = aim; }
void BossVisualEffect::SetAim(Vector2 p) { aimPosition_ = p; }
void BossVisualEffect::OnImpact(Vector2 p) { impactPosition_ = p;impactFrame_ = 0; }
void BossVisualEffect::OnMuzzleFlash(Vector2 p) { muzzlePosition_ = p;muzzleFrame_ = 0; }
void BossVisualEffect::Update(Vector2 bossPosition) {
    ++globalFrame_;++modeFrame_;
    rotorAngle_ += (mode_ == BossVfxMode::Warning ? 0.6f : 0.22f);
    if (impactFrame_ >= 0 && ++impactFrame_ > 17)impactFrame_ = -1;
    if (muzzleFrame_ >= 0 && ++muzzleFrame_ > 6)muzzleFrame_ = -1;
    lastVelocityX_ = bossPosition.x - lastPosition_.x;
    lastVelocityY_ = bossPosition.y - lastPosition_.y;
    lastPosition_ = bossPosition;
}
void BossVisualEffect::DrawBehind(Vector2 pos) const {
    // 影は1BOX。高度を受け取って横幅だけ変化。
    const float h = pos.y - groundY_;
    PortalDraw::Box({ pos.x,groundY_ + 2.0f },
        75.0f - Clamp01(h / 500.0f) * 28.0f, 4.0f, 0x1E253755);
    if (mode_ == BossVfxMode::Warning) {
        // 段階的に詰まる「照準BOX」4個。線を大量に使わない。
        const float r = 76.0f - 52.0f * warningProgress_;
        const unsigned int color = (modeFrame_ / 4) % 2 ? 0xFC4E4EFF : 0xFFAAA5FF;
        PortalDraw::Box({ aimPosition_.x - r,aimPosition_.y }, 4, 18, color);
        PortalDraw::Box({ aimPosition_.x + r,aimPosition_.y }, 4, 18, color);
        PortalDraw::Box({ aimPosition_.x,aimPosition_.y - r * 0.33f }, 20, 4, color);
        PortalDraw::Box({ aimPosition_.x,aimPosition_.y + r * 0.33f }, 20, 4, color);
    }
    // 残像は2個の薄いBOXだけ（高速で動くときだけ）。
    if (mode_ == BossVfxMode::Rush ||
        (mode_ == BossVfxMode::Press && std::fabs(lastVelocityY_) > 9.0f)) {
        const Vector2 backward = { pos.x - lastVelocityX_ * 1.5f,pos.y - lastVelocityY_ * 1.5f };
        PortalDraw::Box(backward, 88.0f, 58.0f, 0xFFFFFF33);
        PortalDraw::Box({ pos.x - lastVelocityX_ * 2.4f,pos.y - lastVelocityY_ * 2.4f },
            57.0f, 38.0f, 0xE0B9FF22);
    }
    if (impactFrame_ >= 0) {
        // 地面衝撃は左右に伸びる2BOX。拡大は幅だけを変更する。
        const float w = 25.0f + impactFrame_ * 16.0f;
        PortalDraw::Box({ impactPosition_.x - w * 0.5f,impactPosition_.y + 3 }, w, 5, 0xFFD89ADD);
        PortalDraw::Box({ impactPosition_.x + w * 0.5f,impactPosition_.y + 3 }, w, 5, 0xFFEFA855);
    }
}
void BossVisualEffect::DrawBody(Vector2 pos) const {
    // 【サイズ厳守】ユーザー指定の256×256、倍率1.0固定。攻撃中もサイズは変えない。
    // 派手さは色・傾き・位置移動・硬直で表す。Noviceへの描画はBOX1枚。
    Vector2 draw = pos;
    if (mode_ == BossVfxMode::Idle)draw.y += std::sin(globalFrame_ * 0.055f) * 2.5f;
    if (mode_ == BossVfxMode::Warning)draw.x += std::sin(modeFrame_ * 0.8f) * 3.0f;
    const float lean = mode_ == BossVfxMode::Rush ?
        (lastVelocityX_ >= 0.0f ? -0.12f : 0.12f) : 0.0f;
    const unsigned int color = mode_ == BossVfxMode::Warning && (modeFrame_ / 5) % 2 ?
        0xFF9090FF : 0xFFFFFFFF;
    if (lean != 0.0f)PortalDraw::RotatedBox(draw, 256.0f, 256.0f, lean, color);
    else PortalDraw::Box(draw, 256.0f, 256.0f, color);
}

void BossVisualEffect::DrawFront() const {
    // 射撃時だけ1BOXを一瞬発光させる。パーティクルは出さない。
    if (muzzleFrame_ >= 0) {
        const float size = 21.0f - muzzleFrame_ * 2.6f;
        PortalDraw::Box(muzzlePosition_, size, size, 0xFFB052AA);
    }
    // 接地時のフラッシュは全画面BOX1枚。2フレームのみ。
    if (impactFrame_ >= 0 && impactFrame_ < 2)
        Novice::DrawBox(0, 0, kWindowSize.x, kWindowSize.y, 0.0f, 0xFFFFFF22, kFillModeSolid);
}
void BossVisualEffect::DrawBullet(Vector2 p, Vector2 velocity) const {
    PortalDraw::Box(p, 13.0f, 13.0f, 0xFFD488FF);
    PortalDraw::Box({ p.x - velocity.x * 1.3f,p.y - velocity.y * 1.3f }, 7, 7, 0xFFB26B66);
}
