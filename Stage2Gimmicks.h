#pragma once
// Stage 2 prototype extras. Player sample's physics/draw are intentionally untouched.
#include "Vector2.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include "Stage2Tuning.h"
#include "AssetEditor.h"
#include "Camera.h"
#include "BossCombatEditorSettings.h"
#include <cmath>

class Stage2Gimmicks {
public:
    enum class Pattern { Wait, Homing, Cutter };
    void Reset() {
        frame_ = 0; hitTimer_ = 0; cycleFrame_ = 0;
        specialMode_ = 0; specialFrame_ = 0; previousSpecialMode_ = 0;
        loosen_ = 0.0f; split_ = false;
        testMissileOnce_ = false; testCutterOnce_ = false;
        for (int i = 0; i < 4; ++i) { missiles_[i] = {}; }
    }
    void TestMissile() { testMissileOnce_ = true; testCutterOnce_ = false; cycleFrame_ = 0; for (auto& m : missiles_)m.active = false; }
    void TestCutter() { testCutterOnce_ = true; testMissileOnce_ = false; cycleFrame_ = 0; }
    void Update(Vector2 player, Vector2 boss, int bossHp, int bossMaxHp, bool usingGrip,
        int bossSpecialMode, int bossSpecialFrame) {
        (void)usingGrip;

        ++frame_;
        // ボスAIの特殊攻撃状態だけで武装を発動。独立した540F周期は廃止。
        // エディターの単独テストだけは例外として独立起動できる。
        if (bossSpecialMode != 0) {
            testMissileOnce_ = testCutterOnce_ = false;
            specialMode_ = bossSpecialMode;
            specialFrame_ = bossSpecialFrame;
        }
        else if (testMissileOnce_ || testCutterOnce_) {
            specialMode_ = testMissileOnce_ ? 1 : 2;
            ++cycleFrame_;
            specialFrame_ = cycleFrame_;
            if (cycleFrame_ >= (specialMode_ == 1 ? 200 : 40 + Stage2Tuning::values.cutterDuration + 25)) {
                testMissileOnce_ = testCutterOnce_ = false;
                specialMode_ = 0;
            }
        }
        else {
            specialMode_ = 0;
            specialFrame_ = 0;
        }
        if (specialMode_ != previousSpecialMode_) {
            if (specialMode_ == 2) for (auto& m : missiles_) m.active = false;
            previousSpecialMode_ = specialMode_;
        }
        if (hitTimer_ > 0) --hitTimer_;
        split_ = bossMaxHp > 0 && bossHp * 2 <= bossMaxHp;
        // Both grapple routes remain usable: the exposed GP coordinates move with the scenery.
        // Grip near boss alone no longer counts as screw damage.
        // ArmorTensionSystem owns screw-by-screw loosening.
        if (specialMode_ == 1 && boss.x > 50.0f && boss.x < 1230.0f &&
            boss.y > 30.0f && boss.y < 690.0f && specialFrame_ >= 32 && specialFrame_ <= 142 &&
            (specialFrame_ - 32) % (std::max)(20, Stage2Tuning::values.missileInterval) == 0) {
            for (int i = 0; i < 4; ++i) if (!missiles_[i].active) {
                // Bullet-Bill style: large missile exits from the aircraft,
                // curves toward the player briefly, then flies straight.
                const float side = player.x >= boss.x ? 1.0f : -1.0f;
                missiles_[i].position = { boss.x + side * 95.0f + BossCombatEditor::values.missileOffsetX,
                    boss.y - 25.0f + BossCombatEditor::values.missileOffsetY };
                missiles_[i].velocity = { side * Stage2Tuning::values.missileStartSpeed,0.0f };
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
            if (m.age <= Stage2Tuning::values.missileHomingFrames) {
                const float wanted = std::atan2(player.y - m.position.y, player.x - m.position.x);
                const float current = std::atan2(m.velocity.y, m.velocity.x);
                float delta = wanted - current;
                while (delta > 3.14159265f) delta -= 6.2831853f;
                while (delta < -3.14159265f) delta += 6.2831853f;
                const float maxTurn = Stage2Tuning::values.missileTurnDeg * (3.14159265f / 180.0f);
                if (delta > maxTurn) delta = maxTurn;
                if (delta < -maxTurn) delta = -maxTurn;
                const float angle = current + delta;
                const float speed = Stage2Tuning::values.missileStartSpeed +
                    static_cast<float>(m.age) * Stage2Tuning::values.missileAcceleration;
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
        const float offset = std::fmod(static_cast<float>(frame_) * Stage2Tuning::values.gpScrollSpeed, 1280.0f);
        float x = Stage2Tuning::values.gpStartX + static_cast<float>(index) * Stage2Tuning::values.gpSpacing - offset;
        while (x < 45.0f) x += 1280.0f;
        while (x > 1240.0f) x -= 1280.0f;
        return { x, index % 2 ? Stage2Tuning::values.gpRightY : Stage2Tuning::values.gpLeftY };
    }
    bool IsSplit() const { return split_; }
    float Loosen() const { return loosen_; }
    bool ConsumeLoosen() {
        if (loosen_ < 100.0f) return false;
        loosen_ = 0.0f;return true;
    }
    bool CanCounter() const { return cycleFrame_ >= 400 && cycleFrame_ < 525; }
    Pattern CurrentPattern() const {
        if (specialMode_ == 1 && specialFrame_ >= 30 && specialFrame_ < 175) return Pattern::Homing;
        if (specialMode_ == 2 && specialFrame_ >= 40 &&
            specialFrame_ < 40 + Stage2Tuning::values.cutterDuration) return Pattern::Cutter;
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
            // PNGを登録していれば専用画像で描画し、未登録時は従来の図形に戻す。
            const Missile& m = missiles_[i];
            const auto& missileImage = AssetEditor::assets[AssetEditor::Stage2Missile];
            if (missileImage.handle >= 0 && missileImage.width > 0) {
                const float scale = GetCameraZoom() * AssetEditor::Scale(AssetEditor::Stage2Missile);
                const Vector2i screen = PortalDraw::ToScreen(m.position);
                const int dw = int(missileImage.width * scale);
                const int dh = int(missileImage.height * scale);
                const float direction = std::atan2(m.velocity.y, m.velocity.x);
                Novice::DrawSprite(screen.x - dw / 2, screen.y - dh / 2,
                    missileImage.handle, scale, scale, -direction, 0xFFFFFFFF);
                continue;
            }
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
            const auto& cutterImage = AssetEditor::assets[AssetEditor::Stage2Cutter];
            if (cutterImage.handle >= 0 && cutterImage.width > 0) {
                const float scale = GetCameraZoom() * AssetEditor::Scale(AssetEditor::Stage2Cutter);
                const Vector2i screen = PortalDraw::ToScreen(c);
                const int dw = int(cutterImage.width * scale), dh = int(cutterImage.height * scale);
                Novice::DrawSprite(screen.x - dw / 2, screen.y - dh / 2,
                    cutterImage.handle, scale, scale, frame_ * 0.22f, 0xFFFFFFFF);
            }
            else {
                PortalDraw::RotatedBox(c, 68, 15, frame_ * 0.22f, 0xF15684FF);
                PortalDraw::Circle(c, 13, 0xFFF1DBFF);
            }
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
    static float Dist2(Vector2 a, Vector2 b) { return PortalMath::Dist2(a, b); }
    static float Min(float a, float b) { return a < b ? a : b; }
    Vector2 CutterPosition() const {
        const float t = static_cast<float>(specialFrame_ - 40) /
            static_cast<float>(Stage2Tuning::values.cutterDuration);
        Vector2 p = Stage2Tuning::CutterPoint(t);
        p.x += BossCombatEditor::values.cutterOffsetX;
        p.y -= BossCombatEditor::values.cutterOffsetY; // CutterPointは画面座標
        p.y = 720.0f - p.y; // PortalDraw/当たり判定はY上向きの世界座標
        return p;
    }
    bool testMissileOnce_ = false, testCutterOnce_ = false;
    int specialMode_ = 0, specialFrame_ = 0, previousSpecialMode_ = 0;
    Missile missiles_[4] = {};
    int frame_ = 0, cycleFrame_ = 0, hitTimer_ = 0;
    float loosen_ = 0;bool split_ = false;
};
