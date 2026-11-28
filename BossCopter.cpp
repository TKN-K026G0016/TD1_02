// このボス専用のAI・移動・攻撃処理。
#include "BossBase.h"
#include "BossCombatEditorSettings.h"
#include "Camera.h"
#include <Novice.h>
#include "PortalDraw.h"
#include <cmath>
#include <cstring>


#include "BossHelpers.h"
using namespace BossHelpers;
using PortalMath::Clamp;
using PortalMath::Abs;
using PortalMath::Dist2;
using PortalMath::Approach;

// このステージ専用のボスAI（共通Bossクラスのメンバー関数）。
void Boss::ChooseAttackStage1() {
    // 【重要】攻撃種類は従来の5種類のまま。
    // 毎回固定の並びにせず、プレイヤーの位置・動き・ワイヤー使用を採点する。
    // 直前と2回前の攻撃にはペナルティを付けて単調な繰り返しを防ぐ。
    // 【注目】【攻撃選択】チュートリアル固定順が有効な場合だけ最初の5技を順に見せる。
    if (BossStage1::kScriptedTutorialFirstFive && attackCount_ < 5) {
        constexpr BossAttack order[5] = {
            BossAttack::Rush,BossAttack::Shot,BossAttack::Press,
            BossAttack::Top,BossAttack::YoYo
        };
        nextAttack_ = order[attackCount_];
        if (!BossCombatEditor::Enabled(int(nextAttack_))) {
            for (const BossAttack a : order)if (BossCombatEditor::Enabled(int(a))) { nextAttack_ = a;break; }
        }
    }
    else {
        const float dx = targetPosition_ ? Abs(targetPosition_->x - position_.x) : 480.0f;

        const float moving = Abs(targetVelocity_.x);
        const bool airborne = !targetGrounded_;
        // 【注目】【AIの採点表】0プレス・1射撃・2突進・3コマ・4ヨーヨーの優先度。
        float score[5] = {};
        // 0 Press 1 Shot 2 Rush 3 Top 4 YoYo
        score[0] = 16.0f + (dx < 245.0f ? 38.0f : 0.0f)
            + (targetGrounded_ ? 13.0f : 0.0f) - (dx > 500.0f ? 30.0f : 0.0f);
        score[1] = 25.0f + (dx > 155.0f && dx < 720.0f ? 24.0f : 0.0f)
            + (airborne ? 15.0f : 0.0f);
        score[2] = 19.0f + (dx > 340.0f ? 40.0f : 0.0f)
            + (targetGrounded_ ? 15.0f : -11.0f)
            + (targetUsingWire_ ? 8.0f : 0.0f);
        score[3] = 17.0f + (targetGrounded_ ? 24.0f : 0.0f)
            + (targetStillFrames_ > 30 ? 22.0f : 0.0f)
            + (dx < 400.0f ? 7.0f : 0.0f);
        score[4] = 18.0f + (airborne ? 19.0f : 0.0f)
            + (targetUsingWire_ ? 27.0f : 0.0f)
            + (moving > 9.0f ? 11.0f : 0.0f);
        if (IsEnraged()) { // HP半分以下：同じ技を連打せず攻め手だけ強める
            score[2] += 10.0f;
            score[4] += 8.0f;
        }
        const int prev = static_cast<int>(lastAttack_);
        const int older = static_cast<int>(previousAttack_);
        // 【注目】【繰り返し防止】直前と2回前の技に点数ペナルティをかける。
        score[prev] -= BossStage1::kAIRepeatPenalty;
        score[older] -= BossStage1::kAIPreviousTwoPenalty;
        // 地上にいるプレイヤー相手でもヨーヨーが出なくならないようにする。
        // 長期間使っていない技ほど少し優先する（5種類すべてが出る）。
        for (int i = 0; i < 5; ++i) {
            score[i] += Clamp(static_cast<float>(attackStaleness_[i]) * 11.0f, 0.0f, 70.0f);
            if (!BossCombatEditor::Enabled(i))score[i] = -1000000.0f;
        }
        int best = 0;
        float bestValue = -100000.0f;
        for (int i = 0;i < 5;++i) {
            // 簡単な乱数：固定順にならないための小さな揺らぎ。
            // 描画や追加の敵オブジェクトは必要ない。
            aiRandomState_ ^= aiRandomState_ << 13;
            aiRandomState_ ^= aiRandomState_ >> 17;
            aiRandomState_ ^= aiRandomState_ << 5;
            const float jitter = (aiRandomState_ % 1000u) / 999.0f * BossStage1::kAIJitter;
            const float evaluated = score[i] + jitter;
            if (evaluated > bestValue) { bestValue = evaluated;best = i; }
        }
        constexpr BossAttack kAttacks[5] = {
            BossAttack::Press,BossAttack::Shot,BossAttack::Rush,
            BossAttack::Top,BossAttack::YoYo
        };
        nextAttack_ = kAttacks[best];
    }
    previousAttack_ = lastAttack_;
    lastAttack_ = nextAttack_;
    for (int i = 0; i < 5; ++i) {
        if (i == static_cast<int>(nextAttack_)) attackStaleness_[i] = 0;
        else if (attackStaleness_[i] < 9) ++attackStaleness_[i];
    }
    ++attackCount_;
}

void Boss::UpdatePressStage1() {
    ++pressFrame_;
    switch (pressPhase_) {
    case PressPhase::Align:
        // ① 狙いを先に固定→機体だけ斜め上へ移動。照準を追尾しすぎない。
        position_.x += (pressTargetX_ - position_.x) * 0.18f;
        position_.y += 3.0f;
        if (pressFrame_ >= 18) ChangePressPhase(PressPhase::Charge);
        break;
    case PressPhase::Charge:
        // ② ため。最後に一瞬逆方向へ跳ねてから一気に落ちる。
        position_.y += (pressFrame_ < 10 ? 0.8f : -0.9f);
        if (pressFrame_ >= 16) ChangePressPhase(PressPhase::Dive);
        break;
    case PressPhase::Dive:
        // ③ 等加速度運動で落下。BOXは1枚でも落下速度の差が迫力になる。
        pressFallSpeed_ = Clamp(pressFallSpeed_ + battleSettings_.pressAccel,
            0.0f, battleSettings_.pressSpeed);
        position_.y -= pressFallSpeed_;
        if (position_.y <= BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f) {
            position_.y = BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f;
            ChangePressPhase(PressPhase::Impact);
            hitstop_ = BossStage1::kImpactHitstop;
            visual_.OnImpact({ position_.x,BossStage1::kFloorY + 5.0f });
            ShakeCamera(9.0f, 13);
            PunchCamera(0.10f, 10);
            // 【ガラクタを落とす】既存の弾配列を再利用し、左右に破片を2つ飛ばす。
            // パーティクルを大量発生させない。
            for (int direction = -1;direction <= 1;direction += 2) {
                Bullet& b = bullets_[nextBulletIndex_];
                nextBulletIndex_ = (nextBulletIndex_ + 1) % kMaxBullets;
                b = {};b.active = true;b.debris = true;b.life = 44;
                b.position = { position_.x + direction * 37.0f,BossStage1::kFloorY + 12.0f };
                b.velocity = { direction * 8.5f,2.2f };
            }
        }
        break;
    case PressPhase::Impact:
        if (pressFrame_ >= 5) ChangePressPhase(PressPhase::Stun);
        break;
    case PressPhase::Stun:
        // ④ プレス後の無防備な隙。外装が剥がれていればコアへの攻撃倍率UP。
        if (pressFrame_ >= (IsEnraged() ? 31 : battleSettings_.pressStun))
            ChangePressPhase(PressPhase::Return);
        break;
    case PressPhase::Return: {
        // ⑤ 行き過ぎて小さく戻る、二次補間。衝撃後の機械らしい復帰。
        float t = Clamp(pressFrame_ / 34.0f, 0.0f, 1.0f);
        float ease = 1.0f - (1.0f - t) * (1.0f - t);
        position_.x = pressReturnStart_.x + (pressReturnDestination_.x - pressReturnStart_.x) * ease;
        position_.y = pressReturnStart_.y + (pressReturnDestination_.y - pressReturnStart_.y) * ease;
        if (pressFrame_ >= 34) { ChangeState(BossState::Idle); }
        break;
    }
    }
}

void Boss::UpdateFlightStage1() {
    if (--flightTimer_ <= 0) {
        const int interval = IsEnraged() ? battleSettings_.flightIntervalPhase2 : battleSettings_.flightInterval;
        flightTimer_ = interval > 0 ? interval : 1;

        // 固定の基準位置や「プレイヤーから一定距離」の二択は使わない。
        // アリーナ内から、現在位置と十分離れた目的地を毎回選ぶ。
        // シードは以前から使っているAI用乱数状態を引き継ぐ。
        for (int attempt = 0; attempt < 8; ++attempt) {
            aiRandomState_ ^= aiRandomState_ << 13;
            aiRandomState_ ^= aiRandomState_ >> 17;
            aiRandomState_ ^= aiRandomState_ << 5;
            const float rx = (aiRandomState_ % 10000u) / 9999.0f;
            aiRandomState_ ^= aiRandomState_ << 13;
            aiRandomState_ ^= aiRandomState_ >> 17;
            aiRandomState_ ^= aiRandomState_ << 5;
            const float ry = (aiRandomState_ % 10000u) / 9999.0f;

            const Vector2 candidate = { 235.0f + rx * 810.0f, 350.0f + ry * 200.0f };
            const float dx = candidate.x - position_.x;
            const float dy = candidate.y - position_.y;
            flightDestination_ = candidate;
            if (dx * dx + dy * dy >= 170.0f * 170.0f) break;
        }
        flightDirection_ = flightDestination_.x >= position_.x ? 1 : -1;
    }

    // 目標速度に向かって徐々に加減速させる。座標を一瞬で代入しない。
    const float maxSpeed = IsEnraged() ? battleSettings_.flightSpeedPhase2 : battleSettings_.flightSpeed;
    const float wantedX = Clamp((flightDestination_.x - position_.x) * 0.065f, -maxSpeed, maxSpeed);
    const float wantedY = Clamp((flightDestination_.y - position_.y) * 0.050f, -maxSpeed * 0.65f, maxSpeed * 0.65f);
    flightVelocity_.x += (wantedX - flightVelocity_.x) * 0.14f;
    flightVelocity_.y += (wantedY - flightVelocity_.y) * 0.12f;
    // 突進後は床上Y=192付近から再浮上する。
    // ここで最低高度345へClampすると153pxワープするので、床の高さを下限にする。
    position_.x = Clamp(position_.x + flightVelocity_.x,
        BossStage1::kArenaLeft + BossStage1::kBossWidth * 0.5f,
        BossStage1::kArenaRight - BossStage1::kBossWidth * 0.5f);
    position_.y = Clamp(position_.y + flightVelocity_.y,
        BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f, 555.0f);
}

void Boss::UpdateToy() {
    if (nextAttack_ == BossAttack::Top) {
        // 落下と壁の反射を別々に管理し、空中で壁に当たっても落下を続ける。
        ++topElapsed_;
        const float floorCenter = BossStage1::kFloorY + kTopRadius;
        if (!topLanded_) {
            toyVelocity_.y = Clamp(toyVelocity_.y - battleSettings_.topGravity, -26.0f, 12.0f);
            toyPosition_.y += toyVelocity_.y;
            if (toyPosition_.y <= floorCenter) {
                toyPosition_.y = floorCenter;
                toyVelocity_.y = 0.0f;
                topLanded_ = true;
                ShakeCamera(3.0f, 5);
            }
        }
        else {
            toyPosition_.y = floorCenter + battleSettings_.topBounceHeight *
                Abs(std::sin(stateFrame_ * 0.14f));
        }

        // 最初の壁を起点に、壁間を4回横切ると2往復になる。
        constexpr int kWallHitsToExit = 5;
        const float speed = battleSettings_.topInitialSpeed;
        if (topWallHits_ > 0) {
            const float wanted = std::copysign(speed, toyVelocity_.x);
            toyVelocity_.x += (wanted - toyVelocity_.x) * battleSettings_.topReturnAccel;
        }
        toyPosition_.x += toyVelocity_.x;
        if (topWallHits_ < kWallHitsToExit && topElapsed_ < battleSettings_.topMaxLifetime) {
            const float halfWidth = kTopVisibleHalfWidth * TopWidthFactor(stateFrame_);
            const float left = kTopFloorLeft + halfWidth;
            const float right = kTopFloorRight - halfWidth;
            const bool wallHit = (toyVelocity_.x < 0.0f && toyPosition_.x <= left) ||
                (toyVelocity_.x > 0.0f && toyPosition_.x >= right);
            if (wallHit && ++topWallHits_ < kWallHitsToExit) {
                toyPosition_.x = Clamp(toyPosition_.x, left, right);
                toyVelocity_.x = -std::copysign(speed * 0.65f, toyVelocity_.x);
                ShakeCamera(3.0f, 5);
            }
        }
        else {
            // カメラの揺れ・ズームを含め、画像が画面の外に出切ってから終了する。
            constexpr int kStageViewportWidth = 1280;
            const int screenX = PortalDraw::ToScreen(toyPosition_).x;
            const int halfWidth = PortalDraw::Scale(2.0f * kTopRadius) / 2;
            if (screenX + halfWidth < 0 || screenX - halfWidth >= kStageViewportWidth)
                ChangeState(BossState::Idle);
        }
        return;
    }
    // 【ヨーヨー】伸ばす→緩める→戻すをsinで往復させる。
    const float pi = 3.14159265f;
    const float t = Clamp(stateFrame_ / static_cast<float>(battleSettings_.yoyoDuration), 0.0f, 1.0f);
    const float extend = std::sin(pi * t);
    const float baseY = position_.y - 20.0f;
    toyPosition_.x = position_.x + toyDirection_ * (47.0f + extend * battleSettings_.yoyoRange);
    toyPosition_.y = baseY + (toyAimY_ - baseY) * extend +
        BossStage1::kYoYoHeightWave * std::sin(2.0f * pi * t) * extend;
    if (stateFrame_ >= battleSettings_.yoyoDuration) ChangeState(BossState::Idle);
}

void Boss::UpdateRushStage1() {
    position_.y = rushTargetY_;
    const float maxSpeed = (battleSettings_.rushSpeed > 1.0f ? battleSettings_.rushSpeed : 1.0f) *
        (IsEnraged() ? 1.25f : 1.0f);
    const float accel = battleSettings_.rushAccel > 0.1f ? battleSettings_.rushAccel : 0.1f;
    rushVelocity_ = Clamp(rushVelocity_ + accel, 0.0f, maxSpeed);
    position_.x += rushDirection_ * rushVelocity_;
}

// 各画像の見えている絵の中心が、機体のどこに配置されるか（+Yが上）。
// PNGは送付されたform1直下のファイルをそのまま使用。proto/photoは使用しない。
namespace {
    // 画像ごとの配置は完成絵 boss_new.png（512x512）から測定した左上座標。
    // 完成絵の配置座標を基準に、各パーツPNGを等倍で重ねる。
    // 各PNGの透明余白も含めた位置なので、分割しても完成絵に重なる。
    struct BossPartArt { const char* path; float left; float top; int width; int height; };
    constexpr float kArtScale = 1.0f; // 送付PNGは等倍表示。カメラ倍率は別管理。
    // 本体 / プロペラ1〜3（本体と目は外れない）
    const BossPartArt kBreakArt[4] = {
        {"./resources/texture/boss/form1/boss_body.png",79,111,256,256},
        {"./resources/texture/boss/form1/boss_propeller1.png",231,107,256,256},
        {"./resources/texture/boss/form1/boss_propeller2.png",91,75,256,256},
        {"./resources/texture/boss/form1/boss_propeller3.png",-47,108,256,256}
    };
    // ジェット1〜4 / パイプ / 目
    const BossPartArt kAccessoryArt[Boss::kAccessoryGripCount] = {
        {"./resources/texture/boss/form1/boss_jet1.png",45,243,256,256},
        {"./resources/texture/boss/form1/boss_jet2.png",115,247,256,256},
        {"./resources/texture/boss/form1/boss_jet3.png",116,236,256,256},
        {"./resources/texture/boss/form1/boss_jet4.png",304,216,256,256},
        {"./resources/texture/boss/form1/boss_pipes.png",208,99,256,256},
        {"./resources/texture/boss/form1/boss_eye.png",82,204,128,128}
    };
    Vector2 ImageOffset(const BossPartArt& art) {
        return { (art.left + art.width * 0.5f - 256.0f) * kArtScale,
                (256.0f - art.top - art.height * 0.5f) * kArtScale };
    }
    void DrawImagePart(const BossPartArt& art, Vector2 base, float angle, int& handle) {
        if (handle == -2) handle = Novice::LoadTexture(art.path);
        if (std::strcmp(art.path, "./resources/texture/boss/form1/boss_body.png") == 0)
            handle = AssetEditor::Handle(AssetEditor::BossBody);
        const Vector2 offset = ImageOffset(art);
        const Vector2 center = { base.x + offset.x, base.y + offset.y };
        if (handle >= 0) {
            const Vector2i screen = PortalDraw::ToScreen(center);
            const bool isBossBody = std::strcmp(art.path,
                "./resources/texture/boss/form1/boss_body.png") == 0;
            const float extraScale = isBossBody ? AssetEditor::Scale(AssetEditor::BossBody) : 1.0f;
            const float drawScale = GetCameraZoom() * kArtScale * extraScale;
            // 差し替えたボス本体が256px以外でも、実際のPNG寸法で中央揃えする。
            const int imageW = isBossBody && AssetEditor::assets[AssetEditor::BossBody].width > 0
                ? AssetEditor::assets[AssetEditor::BossBody].width : art.width;
            const int imageH = isBossBody && AssetEditor::assets[AssetEditor::BossBody].height > 0
                ? AssetEditor::assets[AssetEditor::BossBody].height : art.height;
            const int w = static_cast<int>(imageW * drawScale);
            const int h = static_cast<int>(imageH * drawScale);
            Novice::DrawSprite(screen.x - w / 2, screen.y - h / 2,
                handle, drawScale, drawScale, -angle, 0xFFFFFFFF);
        }
        else {
            // 画像を見つけられない場合でもゲームを止めず、目印のBOXを表示する。
            PortalDraw::Box(center, 22.0f, 22.0f, 0xA9ACC6FF);
        }
    }
}

void Boss::DrawCopterArt(float bodyLean) const {
    static int breakHandles[BossStage1::kArmorPlates] = { -2,-2,-2,-2 };
    static int extraHandles[kAccessoryGripCount] = { -2,-2,-2,-2,-2,-2 };
    // 背面のパーツから描く。最後に本体と目を置くことで重なりが自然になる。
    for (int order = 0; order < kAccessoryGripCount - 1; ++order) {
        const int i = (order == 0 ? 4 : order - 1);
        if (!accessoryAttached_[i] && detachedAccessoryFrames_[i] <= 0) continue;
        Vector2 center = position_;
        float angle = bodyLean;
        if (!accessoryAttached_[i]) {
            const float t = 100.0f - detachedAccessoryFrames_[i];
            center = detachedAccessoryPosition_[i];
            center.x += (i % 2 == 0 ? -1.0f : 1.0f) * t * 2.1f;
            center.y += t * 1.3f - t * t * 0.18f;
            angle = t * (i % 2 == 0 ? -0.05f : 0.05f);
            if (center.y < -180.0f) continue;
        }
        DrawImagePart(kAccessoryArt[i], center, angle, extraHandles[i]);
    }
    for (int i = 1; i < BossStage1::kArmorPlates; ++i) {
        if (i != 0 && !armorAttached_[i] && peeledFrames_[i] <= 0) continue;
        Vector2 center = position_;
        float angle = bodyLean;
        if (i != 0 && !armorAttached_[i]) {
            const float t = 100.0f - peeledFrames_[i];
            center = peeledPosition_[i];
            center.x += (i % 2 == 0 ? -1.0f : 1.0f) * t * 2.3f;
            center.y += t * 1.8f - t * t * 0.20f;
            angle = t * (i % 2 == 0 ? -0.045f : 0.045f);
            if (center.y < -180.0f) continue;
        }
        DrawImagePart(kBreakArt[i], center, angle, breakHandles[i]);
    }
    // 本体装甲は最後まで残し、ほかのパーツの上に重ねる。
    DrawImagePart(kBreakArt[0], position_, bodyLean, breakHandles[0]);
    // 目だけは常に一番前に描画。最後に弱点になるが、剥がれたり消えたりしない。
    DrawImagePart(kAccessoryArt[kAccessoryGripCount - 1], position_, bodyLean,
        extraHandles[kAccessoryGripCount - 1]);
}
