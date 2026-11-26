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
        (void)usingGrip;

        ++frame_; ++cycleFrame_;
        if (cycleFrame_ >= 540) cycleFrame_ = 0;
        if (hitTimer_ > 0) --hitTimer_;
        split_ = bossMaxHp > 0 && bossHp * 2 <= bossMaxHp;
        // Both grapple routes remain usable: the exposed GP coordinates move with the scenery.
        // Grip near boss alone no longer counts as screw damage.
        // ArmorTensionSystem owns screw-by-screw loosening.
        if (cycleFrame_ >= 20 && cycleFrame_ < 170 && cycleFrame_ % 65 == 20) {
            for (int i = 0; i < 4; ++i) if (!missiles_[i].active) {
                // Bullet-Bill style: large missile exits from the aircraft,
                // curves toward the player briefly, then flies straight.
                const float side = player.x >= boss.x ? 1.0f : -1.0f;
                missiles_[i].position = { boss.x + side * 95.0f,boss.y - 25.0f };
                missiles_[i].velocity = { side * 5.0f,0.0f };
                missiles_[i].age = 0;
                missiles_[i].life = 190;
                missiles_[i].active = true;
                break;
            }
        }
        for (int i = 0; i < 4; ++i) {
            Missile& m = missiles_[i];
            if (!m.active) continue;
            if (--m.life <= 0) { m.active = false;continue; }
            ++m.age;
            // 50-frame steering period. Limited turning speed avoids
            // unnaturally sharp chasing, like a heavy guided missile.
            if (m.age <= 50) {
                const float wanted = std::atan2(player.y - m.position.y, player.x - m.position.x);
                const float current = std::atan2(m.velocity.y, m.velocity.x);
                float delta = wanted - current;
                while (delta > 3.14159265f) delta -= 6.2831853f;
                while (delta < -3.14159265f) delta += 6.2831853f;
                if (delta > 0.027f) delta = 0.027f;
                if (delta < -0.027f) delta = -0.027f;
                const float angle = current + delta;
                const float speed = 5.0f + static_cast<float>(m.age) * 0.065f;
                m.velocity = { std::cos(angle) * speed,std::sin(angle) * speed };
            }
            m.position.x += m.velocity.x;
            m.position.y += m.velocity.y;
            if (m.position.x < -110.0f || m.position.x > 1390.0f ||
                m.position.y < -100.0f || m.position.y > 820.0f)
                m.active = false;
        }
    }
    Vector2 Anchor(int index) const {
        // Two GP per screen, spaced by 640px, flowing slowly to the left.
        // Wrap each GP cleanly after leaving the screen.
        const float offset = std::fmod(static_cast<float>(frame_) * 1.15f, 1280.0f);
        float x = 115.0f + static_cast<float>(index) * 640.0f - offset;
        while (x < 45.0f) x += 1280.0f;
        while (x > 1240.0f) x -= 1280.0f;
        return { x, index % 2 ? 545.0f : 470.0f };
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
            if (missiles_[i].active && Dist2(player, missiles_[i].position) < (playerRadius + 35) * (playerRadius + 35)) {
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
            // 96x58px stylised large missile, facing its actual velocity.
            // Drawn with primitives, so no new resource is required.
            const Missile& m = missiles_[i];
            const float angle = std::atan2(m.velocity.y, m.velocity.x);
            const Vector2 forward = { std::cos(angle),std::sin(angle) };
            const Vector2 up = { -forward.y,forward.x };
            const Vector2 tail = { m.position.x - forward.x * 35.0f,
                                m.position.y - forward.y * 35.0f };
            PortalDraw::RotatedBox(m.position, 82.0f, 52.0f, angle, 0x282C3DFF);
            PortalDraw::Circle({ m.position.x + forward.x * 39.0f,
                                m.position.y + forward.y * 39.0f }, 25.0f, 0x252A38FF);
            PortalDraw::RotatedBox(tail, 26.0f, 72.0f, angle, 0x42495DFF);
            PortalDraw::RotatedBox({ m.position.x - forward.x * 12.0f,
                                    m.position.y - forward.y * 12.0f }, 9.0f, 48.0f, angle, 0xA7AFC1FF);
            PortalDraw::Circle({ m.position.x + forward.x * 18.0f + up.x * 12.0f,
                                m.position.y + forward.y * 18.0f + up.y * 12.0f }, 7.0f, 0xFFFFFFFF);
            PortalDraw::Circle({ m.position.x + forward.x * 21.0f + up.x * 12.0f,
                                m.position.y + forward.y * 21.0f + up.y * 12.0f }, 3.0f, 0x111521FF);
            PortalDraw::Circle({ tail.x - forward.x * 23.0f,tail.y - forward.y * 23.0f },
                8.0f + static_cast<float>(m.age % 5), 0xFFAA48AA);
        }
        if (CurrentPattern() == Pattern::Cutter) {
            Vector2 c = CutterPosition();
            PortalDraw::RotatedBox(c, 68, 15, frame_ * 0.22f, 0xF15684FF);
            PortalDraw::Circle(c, 13, 0xFFF1DBFF);
        }
    }
    void DrawHud() const {
        const char* move = CurrentPattern() == Pattern::Homing ? "LARGE MISSILES - GRAPPLE DODGE" :
            CurrentPattern() == Pattern::Cutter ? "FIGURE-8 CUTTER - LOOSEN SCREW" :
            CanCounter() ? "OPENING - PREPARE TWO-ARM PEEL" : "WAIT / PREPARE";
        Novice::ScreenPrintf(18, 232, "STAGE 2: %s", move);
        Novice::ScreenPrintf(18, 254, "ARMOR: WIND SCREWS FIRST, THEN YANK WITH BOTH ARMS");
        if (split_) Novice::ScreenPrintf(18, 291, "COASTER SPLIT: CENTER GAP (VISUAL PROTOTYPE)");
    }
private:
    struct Missile { Vector2 position = {}, velocity = {};int life = 0, age = 0;bool active = false; };
    static float Dist2(Vector2 a, Vector2 b) { const float x = a.x - b.x, y = a.y - b.y;return x * x + y * y; }
    static float Min(float a, float b) { return a < b ? a : b; }
    Vector2 CutterPosition() const {
        // Wide sideways figure eight: reach both screen edges and run
        // vertically through the centre of the arena on each half loop.
        // The pairs near x=640 form the conspicuous central up/down stroke.
        constexpr Vector2 path[] = {
            {640.0f,360.0f}, {410.0f,625.0f}, {85.0f,570.0f},
            {48.0f,360.0f}, {85.0f,150.0f}, {410.0f,75.0f},
            {640.0f,85.0f}, {640.0f,360.0f}, {640.0f,635.0f},
            {875.0f,625.0f}, {1190.0f,570.0f}, {1232.0f,360.0f},
            {1190.0f,150.0f}, {875.0f,75.0f}, {640.0f,85.0f},
            {640.0f,360.0f}, {640.0f,635.0f}
        };
        constexpr int count = sizeof(path) / sizeof(path[0]);
        constexpr float duration = 165.0f;
        float t = static_cast<float>(cycleFrame_ - 210) / duration;
        t = t - std::floor(t);
        const float scaled = t * static_cast<float>(count - 1);
        const int part = static_cast<int>(scaled);
        const float u = scaled - static_cast<float>(part);
        // Cubic smoothstep avoids abrupt velocity changes at the turns.
        const float smooth = u * u * (3.0f - 2.0f * u);
        const Vector2 a = path[part], b = path[part + 1];
        return { a.x + (b.x - a.x) * smooth,a.y + (b.y - a.y) * smooth };
    }
    Missile missiles_[4] = {};
    int frame_ = 0, cycleFrame_ = 0, hitTimer_ = 0;
    float loosen_ = 0;bool split_ = false;
};
