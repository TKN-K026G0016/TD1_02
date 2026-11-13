#include "PortalInput.h"
#include "PortalConfig.h"
#include "PortalMath.h"
#include <Novice.h>
#include <cmath>
using namespace PortalMath;

namespace {
    Vector2 ReadStick(int x, int y) {
        // NoviceのスティックYは下が正。ゲーム内は上が正なので反転する。
        Vector2 v = { static_cast<float>(x), -static_cast<float>(y) };
        const float length = Length(v);
        if (length < PortalConfig::kStickDeadZone) return {};
        const float magnitude = Clamp((length - PortalConfig::kStickDeadZone) /
            (32767.0f - PortalConfig::kStickDeadZone), 0.0f, 1.0f);
        return Mul(Unit(v), magnitude);
    }
    Vector2 EightDirections(Vector2 v) {
        // 右スティックを45度ごとの8方向へ吸着。カメラの向きには依存しない。
        const float angle = std::round(std::atan2(v.y, v.x) / 0.785398163f) * 0.785398163f;
        return { std::cos(angle),std::sin(angle) };
    }
    int DirectionSector(Vector2 v) {
        return static_cast<int>(std::round(std::atan2(v.y, v.x) / 0.785398163f));
    }
}

PortalCommands PortalInput::Read() {
    PortalCommands out;
    char keys[256] = {};
    Novice::GetHitKeyStateAll(keys);
    out.move = static_cast<float>((keys[DIK_D] != 0) - (keys[DIK_A] != 0));
    Vector2 arrows = { static_cast<float>((keys[DIK_RIGHT] != 0) - (keys[DIK_LEFT] != 0)),
                      static_cast<float>((keys[DIK_UP] != 0) - (keys[DIK_DOWN] != 0)) };
    Vector2 keyboardAim = { out.move,static_cast<float>((keys[DIK_W] != 0) - (keys[DIK_S] != 0)) };
    out.aim = Length(arrows) > 0.05f ? EightDirections(arrows) : keyboardAim;
    out.aimExplicit = Length(arrows) > 0.05f || keys[DIK_W] != 0 || keys[DIK_S] != 0;
    bool attack = (keys[DIK_J] != 0); // キーボードのJは従来どおり残す。
    bool jump = (keys[DIK_SPACE] != 0);
    bool reset = (keys[DIK_R] != 0);
    out.connected = Novice::GetNumberOfJoysticks() > 0;

    bool rightActive = false;
    bool rightPulse = false;
    if (out.connected) {
        int lx = 0, ly = 0, rx = 0, ry = 0;
        Novice::GetAnalogInputLeft(0, &lx, &ly);
        Novice::GetAnalogInputRight(0, &rx, &ry);
        const Vector2 left = ReadStick(lx, ly);
        const Vector2 right = ReadStick(rx, ry);
        // 【左スティック】左右だけを移動に使用。上方向はジャンプ。
        // ボタンなしでも移動・ジャンプ・攻撃・グラップルができる。
        out.move = std::fabs(left.x) > 0.08f ? left.x : out.move;
        const bool stickJump = left.y > PortalConfig::kLeftStickJumpThreshold;
        jump = jump || stickJump ||
            Novice::IsPressButton(0, static_cast<PadButton>(PortalConfig::kJumpButton)) != 0;
        // 【右スティック】倒す=8方向触手攻撃。攻撃ボタンは読まない。
        // 入力開始と方向を変えた瞬間に反応。その後も間隔を空けて連続攻撃。
        const float rightLength = Length(right);
        rightActive = previousRightHeld_
            ? rightLength >= PortalConfig::kRightStickReleaseThreshold
            : rightLength >= PortalConfig::kRightStickPressThreshold;
        if (rightActive) {
            const int sector = DirectionSector(right);
            out.aim = EightDirections(right);
            out.aimExplicit = true;
            out.stickAction = true;
            if (!previousRightHeld_) {
                rightHoldFrames_ = 0;
                rightPulse = true;
            }
            else {
                ++rightHoldFrames_;
                // 方向を変えた場合、最短間隔を守って即座に次の攻撃へ。
                const bool changed = sector != previousRightSector_;
                rightPulse = (changed && rightHoldFrames_ >= PortalConfig::kRightStickChangeMinFrames) ||
                    rightHoldFrames_ >= PortalConfig::kRightStickRepeatFrames;
            }
            if (rightPulse) rightHoldFrames_ = 0;
            previousRightSector_ = sector;
        }
        else {
            previousRightSector_ = 999;
            rightHoldFrames_ = 0;
            // 右スティックを離したら、最後の攻撃方向で勝手にグラップルしない。
            out.aimExplicit = Length(arrows) > 0.05f || keys[DIK_W] != 0 || keys[DIK_S] != 0;
        }
        reset = reset || Novice::IsPressButton(0, static_cast<PadButton>(PortalConfig::kResetButton)) != 0;
    }
    else {
        previousRightSector_ = 999;
        rightHoldFrames_ = 0;
    }
    if (!out.stickAction && Length(out.aim) > 0.05f) out.aim = EightDirections(out.aim);
    // プレイヤーへ渡す1Fの攻撃パルス。右スティックではボタン不要。
    out.attack = (attack && !previousAttack_) || rightPulse;
    out.attackHeld = attack || rightActive;
    out.jump = jump && !previousJump_;
    out.jumpHeld = jump;
    out.reset = reset && !previousReset_;
    previousAttack_ = attack;
    previousRightHeld_ = rightActive;
    previousJump_ = jump;
    previousReset_ = reset;
    return out;
}
