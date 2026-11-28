#include "PortalPlayer.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include <cmath>
using namespace PortalMath;

void PortalPlayer::Draw() const {
    for (int i = 0; i < PortalConfig::kGateCount; ++i) gates_[i].Draw(position_, i);
    const float sway = std::sin(animationFrame_ * 0.19f);
    const float runCycle = std::sin(animationFrame_ * 0.28f);
    const float speed = std::fabs(velocity_.x);
    const float bob = grounded_ ? std::fabs(runCycle) * Clamp(speed * 0.33f, 0.0f, 3.0f) : sway * 1.8f;
    const Vector2 drawCenter = Add(position_, { bodyLean_ * 0.55f,bob + landingBounce_ * 3.0f });

    // 動画のキャラクター画像を優先して使う。サイズ64×64・横3コマ。
    // 4状態が同一画像を使用しているので、1個のハンドルを再利用。
    static const int playerTexture = Novice::LoadTexture("./resources/texture/player/video_sample/playerIdle1.png");
    if (playerTexture >= 0) {
        const Vector2i center = PortalDraw::ToScreen(drawCenter);
        const int half = PortalDraw::Scale(32.0f);
        const int frame = (animationFrame_ / 6) % 3;
        // 原画の左右反転。画面座標の上下はDrawQuadの順に従う。
        const int left = center.x - half, right = center.x + half;
        const int x0 = facing_ > 0 ? left : right;
        const int x1 = facing_ > 0 ? right : left;
        Novice::DrawQuad(x0, center.y - half, x1, center.y - half,
            x0, center.y + half, x1, center.y + half,
            frame * 64, 0, 64, 64, playerTexture, 0xFFFFFFFFu);
    }
    else {
        // 尾（BOXのみ）。移動速度・グリップで動く。
        // 【軽量化】身体の尾BOXを33→18枚。揺れ方は同じ補間式。
        for (int tail = 0;tail < 3;++tail) for (int i = 0;i < 6;++i) {
            const float t = (i + 1) / 6.0f;
            const float drift = std::sin(animationFrame_ * (0.16f + tail * 0.018f) - t * 4.0f + tail) * t * 10.0f;
            const float motionDrag = velocity_.x * t * 1.35f;
            Vector2 now = { drawCenter.x + (tail - 1) * 7.0f + drift - motionDrag,
                         drawCenter.y - 12.0f - t * (24.0f + gripStretch_ * 10.0f) };
            const float width = 8.0f * (1.0f - t) + 2.0f;
            PortalDraw::Box(now, width, width, tail == 1 ? 0xA94BE2FF : 0x632086FF);
        }

        const float bodyWidth = 35.0f + landingBounce_ * 9.0f - gripStretch_ * 3.0f;
        const float bodyHeight = 61.0f - landingBounce_ * 13.0f + gripStretch_ * 9.0f;
        const bool hurtBlink = hurtFrames_ > 0 && ((hurtFrames_ / 4) % 2) == 0;
        const unsigned int color = hurtBlink ? 0xFFFFFFFF :
            (hitConfirmFrames_ > 0 ? 0xFFE3A3FF :
                (armorPullFailFrames_ > 0 ? 0xFFA86CFF : 0xA83FDEFF)); // 命中時だけ本体の既存BOXを発光色に
        const Vector2 body = Add(drawCenter, { 0.0f,6.0f + gripStretch_ * 2.0f });
        // 回転攻撃中は「本体も2回転」する。見た目のみで当たり判定は変えない。
        if (spinFrames_ > 0) {
            const float progress = 1.0f - spinFrames_ / static_cast<float>(PortalConfig::kSpinFrames);
            const float angle = progress * 12.56637f;
            PortalDraw::RotatedBox(body, bodyWidth, bodyHeight, angle, 0xF0C8FFFF);
            PortalDraw::RotatedBox(body, bodyWidth - 6.0f, bodyHeight - 6.0f, angle, color);
            const Vector2 eye = Add(body, Rotate({ facing_ * 9.0f,7.0f }, angle));
            PortalDraw::RotatedBox(eye, 3.0f, 23.0f, angle, 0xFFF0FFFF);
        }
        else {
            // 【グラップル中の軽い回転】元の身体BOXを揺らすだけ。描画を増やさない。
            const float bodyAngle = IsGripping()
                ? (grappleSpinTick_ * 0.24f + airTilt_ * 0.3f) : airTilt_;
            if (std::fabs(bodyAngle) > 0.015f) {
                PortalDraw::RotatedBox(body, bodyWidth, bodyHeight, bodyAngle, 0xF0C8FFFF);
                PortalDraw::RotatedBox(body, bodyWidth - 6.0f, bodyHeight - 6.0f, bodyAngle, color);
                PortalDraw::RotatedBox(Add(body, Rotate({ facing_ * (9.0f + sway),7.0f }, bodyAngle)), 3.0f, 23.0f, bodyAngle, 0xFFF0FFFF);
            }
            else {
                PortalDraw::Box(body, bodyWidth, bodyHeight, 0xF0C8FFFF);
                PortalDraw::Box(body, bodyWidth - 6.0f, bodyHeight - 6.0f, color);
                PortalDraw::Box(Add(body, { facing_ * (9.0f + sway),7.0f }), 3.0f, 23.0f, 0xFFF0FFFF);
            }
        }

    }
    // 動画と同じ左右2色のL/R表示（32×32コマ）。
    static const int lrTexture = Novice::LoadTexture("./resources/texture/player/video_sample/ui_lr.png");
    if (lrTexture >= 0) {
        for (int side = 0; side < 2; ++side) {
            const Vector2 world = Add(position_, { side == 0 ? -28.0f : 28.0f, -37.0f });
            const Vector2i p = PortalDraw::ToScreen(world);
            const int half = PortalDraw::Scale(12.0f);
            const unsigned int tint = side == 0 ? 0xFF78BCFFu : 0x62E07AFFu;
            Novice::DrawQuad(p.x - half, p.y - half, p.x + half, p.y - half, p.x - half, p.y + half, p.x + half, p.y + half,
                side * 32, 0, 32, 32, lrTexture, tint);
        }
    }

    // 【Jが必ず出たと分かる演出】通常攻撃時、細長い2本の斬線のみ追加。
    // フレームにより長さを変え、粒子や三角形を大量生成せず動きを表す。
    if (normalAttackFrames_ > 0) {
        const float t = 1.0f - normalAttackFrames_ / static_cast<float>(PortalConfig::kNormalAttackFrames);
        const float length = PortalConfig::kAttackReach * (0.22f + 0.78f * std::sin(t * 3.14159265f));
        const Vector2 end = Add(position_, Mul(normalAttackDirection_, length));
        const Vector2 normal = { -normalAttackDirection_.y,normalAttackDirection_.x };
        PortalDraw::Line(Add(position_, Mul(normal, 5)), Add(end, Mul(normal, 5)), 0xFFFFFFFF);
        PortalDraw::Line(Sub(position_, Mul(normal, 5)), Sub(end, Mul(normal, 5)), 0xBE4BFFFF);
        PortalDraw::Box(end, 13.0f, 13.0f, 0xF7D6FFFF);
    }
    // 【空中回転攻撃】画像なし：円を描くかわりに短い線の触手が周囲を回る。
    // 24分割×2層なので大量の三角形を使わない。
    if (spinFrames_ > 0) {
        const float progress = 1.0f - spinFrames_ / static_cast<float>(PortalConfig::kSpinFrames);
        const float base = progress * 12.56637f; // 2回転分（4π）
        const float radius = PortalConfig::kSpinRadius * (0.75f + 0.25f * std::sin(progress * 3.14159f));
        for (int arm = 0;arm < 4;++arm) {
            const float theta = base + arm * 1.5707963f;
            Vector2 previous = position_;
            // 【軽量化】回転攻撃の触手を7→4区間。回転速度は据え置き。
            for (int i = 1;i <= 4;++i) {
                const float t = i / 4.0f;
                const float angle = theta + t * 0.55f * std::sin(animationFrame_ * 0.4f + arm);
                const Vector2 now = Add(position_, { std::cos(angle) * radius * t,std::sin(angle) * radius * t });
                PortalDraw::Line(previous, now, 0xE5B4FFFF);
                previous = now;
            }
            PortalDraw::Box(previous, 9.0f, 9.0f, 0xFFFFFFFF);
        }
    }

    // 【グラップル中の小回転攻撃】2本の線だけで触手の回転を表現。
    // プレイヤーの身体BOXの回転と同じ周期。大きなパーティクルは生成しない。
    // 実際のヒット判定はHitTestEnemy()の半径kGrappleSpinRadiusで管理する。
    if (IsGripping() && spinFrames_ == 0) {
        const float angle = grappleSpinTick_ * 0.24f;
        const Vector2 tip = { std::cos(angle) * PortalConfig::kGrappleSpinRadius,
                           std::sin(angle) * PortalConfig::kGrappleSpinRadius };
        PortalDraw::Line(Add(position_, Mul(tip, 0.24f)), Add(position_, tip), 0xF5B9FFFF);
        PortalDraw::Line(Sub(position_, Mul(tip, 0.24f)), Sub(position_, tip), 0xB769EFFF);
    }

    // 【自動射出モーション】足元の小さなBOXの噴き出しだけでスピード感を出す。
    if (launchVisualFrames_ > 0) {
        const float ratio = launchVisualFrames_ / static_cast<float>(PortalConfig::kLaunchVisualFrames);
        const Vector2 behind = Sub(position_, Mul(Unit(velocity_), 30.0f));
        PortalDraw::Box(behind, 10.0f + 12.0f * ratio, 6.0f, 0xFFFFFFFF);
        PortalDraw::Box(Sub(behind, Mul(Unit(velocity_), 20.0f)), 6.0f, 4.0f, 0xD391FFFF);
    }
    if (castPulse_ > 0.03f) {
        const float burst = (1.0f - castPulse_) * 23.0f + 15.0f;
        PortalDraw::Box(Add(drawCenter, { burst,0 }), 4, 4, 0xE3B0FFFF);
        PortalDraw::Box(Add(drawCenter, { -burst,0 }), 4, 4, 0xE3B0FFFF);
        PortalDraw::Box(Add(drawCenter, { 0,burst }), 4, 4, 0xE3B0FFFF);
        PortalDraw::Box(Add(drawCenter, { 0,-burst }), 4, 4, 0xE3B0FFFF);
    }
    // グリップできる杭があるときは照準を緑にして分かりやすく。
    const unsigned int aimColor = selectedAnchor_ >= 0 ? 0xB2FFA8FF : 0xFFFFFFFF;
    for (int i = 0;i < 6;++i) {
        const float moving = std::fmod(animationFrame_ * 0.6f + i * 4.0f, 20.0f);
        const Vector2 dot = Add(position_, Mul(aim_, 35.0f + i * 4.4f + moving * 0.23f));
        PortalDraw::Box(dot, 3, 3, aimColor);
    }
    PortalDraw::Box(Add(position_, Mul(aim_, 63.0f + sway * 3.0f)), 8, 8, aimColor);
}
