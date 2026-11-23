#pragma once
// Stage 2 prototype extras. Player sample's physics/draw are intentionally untouched.
#include "Vector2.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include <cmath>

class Stage2Gimmicks {
public:
    enum class Pattern { Wait, Homing, Cutter };
    void Reset() {
        frame_ = 0; hitTimer_ = 0; cycleFrame_ = 0;
        loosen_ = 0.0f; split_ = false;
        for (int i = 0; i < 4; ++i) { missiles_[i] = {}; }
    }
    void Update(Vector2 player, Vector2 boss, int bossHp, int bossMaxHp, bool usingGrip) {
        ++frame_; ++cycleFrame_;
        if (cycleFrame_ >= 540) cycleFrame_ = 0;
        if (hitTimer_ > 0) --hitTimer_;
        split_ = bossMaxHp > 0 && bossHp * 2 <= bossMaxHp;
        // Both grapple routes remain usable: the exposed GP coordinates move with the scenery.
        if (usingGrip && Dist2(player, boss) < 260.0f * 260.0f)
            loosen_ = Min(100.0f, loosen_ + 0.40f);
        if (cycleFrame_ >= 20 && cycleFrame_ < 170 && cycleFrame_ % 50 == 20) {
            for (int i = 0; i < 4; ++i) if (!missiles_[i].active) {
                missiles_[i].position = boss;
                float dx = player.x - boss.x, dy = player.y - boss.y;
                const float l = std::sqrt(dx * dx + dy * dy) + 0.001f;
                missiles_[i].velocity = { dx / l * 4.0f,dy / l * 4.0f };
                missiles_[i].life = 160;
                missiles_[i].active = true;
                break;
            }
        }
        for (int i = 0; i < 4; ++i) {
            Missile& m = missiles_[i];
            if (!m.active) continue;
            if (--m.life <= 0) { m.active = false;continue; }
            const float dx = player.x - m.position.x, dy = player.y - m.position.y;
            const float d = std::sqrt(dx * dx + dy * dy) + 0.001f;
            m.velocity.x = m.velocity.x * 0.965f + (dx / d) * 0.24f;
            m.velocity.y = m.velocity.y * 0.965f + (dy / d) * 0.24f;
            const float speed = std::sqrt(m.velocity.x * m.velocity.x + m.velocity.y * m.velocity.y);
            if (speed > 5.5f) { m.velocity.x *= 5.5f / speed;m.velocity.y *= 5.5f / speed; }
            m.position.x += m.velocity.x;m.position.y += m.velocity.y;
        }
    }
    Vector2 Anchor(int index) const {
        const float offset = std::fmod(static_cast<float>(frame_) * 1.6f, 320.0f);
        const float baseX = 135.0f + static_cast<float>(index) * 320.0f;
        float x = baseX - offset;
        if (x < 45.0f) x += 1280.0f;
        return { x, index % 2 ? 540.0f : 470.0f };
    }
    bool IsSplit() const { return split_; }
    float Loosen() const { return loosen_; }
    bool ConsumeLoosen() {
        if (loosen_ < 100.0f) return false;
        loosen_ = 0.0f;return true;
    }
    bool CanCounter() const { return cycleFrame_ >= 400 && cycleFrame_ < 525; }
    Pattern CurrentPattern() const {
        if (cycleFrame_ < 180) return Pattern::Homing;
        if (cycleFrame_ >= 210 && cycleFrame_ < 375) return Pattern::Cutter;
        return Pattern::Wait;
    }
    int CheckHit(Vector2 player, float playerRadius, Vector2* source) {
        if (hitTimer_ > 0) return 0;
        for (int i = 0;i < 4;++i) {
            if (missiles_[i].active && Dist2(player, missiles_[i].position) < (playerRadius + 16) * (playerRadius + 16)) {
                *source = missiles_[i].position;
                missiles_[i].active = false;hitTimer_ = 42;return 7;
            }
        }
        if (CurrentPattern() == Pattern::Cutter && Dist2(player, CutterPosition()) < (playerRadius + 31) * (playerRadius + 31)) {
            *source = CutterPosition();hitTimer_ = 42;return 10;
        }
        return 0;
    }
    void Draw() const {
        for (int i = 0;i < 4;++i) if (missiles_[i].active) {
            PortalDraw::Circle(missiles_[i].position, 15.0f, 0xF8AC52FF);
            PortalDraw::Circle(missiles_[i].position, 6.0f, 0xFFF1C1FF);
        }
        if (CurrentPattern() == Pattern::Cutter) {
            Vector2 c = CutterPosition();
            PortalDraw::RotatedBox(c, 68, 15, frame_ * 0.22f, 0xF15684FF);
            PortalDraw::Circle(c, 13, 0xFFF1DBFF);
        }
    }
    void DrawHud() const {
        const char* move = CurrentPattern() == Pattern::Homing ? "HOMING MISSILES - GRAPPLE DODGE" :
            CurrentPattern() == Pattern::Cutter ? "FIGURE-8 CUTTER - LOOSEN SCREW" :
            CanCounter() ? "COUNTER WINDOW - PEEL NOW" : "WAIT / PREPARE";
        Novice::ScreenPrintf(18, 232, "STAGE 2: %s", move);
        Novice::DrawBox(18, 254, 202, 12, 0, 0x253044FF, kFillModeSolid);
        Novice::DrawBox(19, 255, static_cast<int>(loosen_ * 2.0f), 10, 0,
            loosen_ >= 100.0f ? 0x62E7A7FF : 0xFFCB6AFF, kFillModeSolid);
        Novice::ScreenPrintf(18, 272, "SCREW LOOSEN %d%% - grip near boss", static_cast<int>(loosen_));
        if (split_) Novice::ScreenPrintf(18, 291, "COASTER SPLIT: CENTER GAP (VISUAL PROTOTYPE)");
    }
private:
    struct Missile { Vector2 position = {}, velocity = {};int life = 0;bool active = false; };
    static float Dist2(Vector2 a, Vector2 b) { const float x = a.x - b.x, y = a.y - b.y;return x * x + y * y; }
    static float Min(float a, float b) { return a < b ? a : b; }
    Vector2 CutterPosition() const {
        const float t = (cycleFrame_ - 210) * 0.075f;
        const float denom = 1.0f + std::cos(t) * std::cos(t);
        return { 640.0f + 400.0f * std::sin(t) / denom,
                340.0f + 225.0f * std::sin(t) * std::cos(t) / denom };
    }
    Missile missiles_[4] = {};
    int frame_ = 0, cycleFrame_ = 0, hitTimer_ = 0;
    float loosen_ = 0;bool split_ = false;
};
