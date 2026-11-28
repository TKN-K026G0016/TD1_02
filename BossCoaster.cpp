// このボス専用のAI・移動・攻撃処理。
#include "BossBase.h"
#include "BossCombatEditorSettings.h"
#include "Camera.h"
#include "PortalDraw.h"
#include <Novice.h>
#include "AssetEditor.h"
#include "Stage2Tuning.h"
#include <cmath>
#include <algorithm>


#include "BossHelpers.h"
using namespace BossHelpers;
using PortalMath::Clamp;
using PortalMath::Abs;
using PortalMath::Dist2;
using PortalMath::Approach;

// このステージ専用のボスAI（共通Bossクラスのメンバー関数）。
void Boss::ChooseAttackStage2() {
    // 第2ステージ専用の軽量AI。7種類すべてを機械的に順番再生するのではなく、
    // プレイヤーの高度・横移動・グリップ状態・直前の技から攻撃を選択する。
    // 0=縦突進、1=射撃、2=横突進、5=ミサイル、6=カッター。
    const float x = targetPosition_ ? targetPosition_->x : 640.0f;
    const float y = targetPosition_ ? targetPosition_->y : 300.0f;
    const float speed = Abs(targetVelocity_.x);
    const int slots[] = { 2, 0, 5, 6, 1 };
    float score[] = {
        45.0f + (targetUsingWire_ ? 12.0f : 0.0f),          // 横突進
        35.0f + (y < 320.0f ? 21.0f : 0.0f),               // 縦突進
        39.0f + (speed < 3.5f ? 18.0f : 0.0f),              // ミサイル
        33.0f + (targetUsingWire_ ? 19.0f : 0.0f),         // 8の字カッター
        28.0f + (x > 330.0f && x < 960.0f ? 10.0f : 0.0f) // 射撃
    };
    int bestSlot = -1;
    float bestScore = -1.0e9f;
    for (int i = 0; i < 5; ++i) {
        const int slot = slots[i];
        const bool enabled = slot == 5 ? BossCombatEditor::values.missileEnabled :
            slot == 6 ? BossCombatEditor::values.cutterEnabled : BossCombatEditor::Enabled(slot);
        if (!enabled) continue;
        float v = score[i];
        if (slot == stage2LastSlot_) v -= 46.0f; // 同じ攻撃の連続を避ける
        if (slot == 6 && attackCount_ % 4 == 2) v += 32.0f; // カッターを定期的に採用
        if (slot == 0 && attackCount_ % 4 == 3) v += 30.0f; // 縦突進も忘れない
        // 毎回固定順にならない程度の小さな決定的変化（ランダム生成なし）。
        v += float((attackCount_ * 11 + slot * 19) % 17);
        if (v > bestScore) { bestScore = v; bestSlot = slot; }
    }
    if (bestSlot < 0) return;
    stage2LastSlot_ = bestSlot;
    ++attackCount_;
    if (bestSlot >= 5) {
        stage2SpecialMode_ = bestSlot - 4;
        ChangeState(BossState::Stage2Special);
    }
    else {
        nextAttack_ = static_cast<BossAttack>(bestSlot);
    }
}

void Boss::UpdatePressStage2() {
    ++pressFrame_;
    switch (pressPhase_) {
    case PressPhase::Align:
        position_.x = Approach(position_.x, pressTargetX_, Stage2Tuning::values.alignXSpeed);
        position_.y = Approach(position_.y, Stage2Tuning::values.verticalStartHeight + BossCombatEditor::values.startOffsetY[int(BossAttack::Press)], Stage2Tuning::values.alignYSpeed);
        if (Abs(position_.x - pressTargetX_) < 1.0f &&
            Abs(position_.y - (Stage2Tuning::values.verticalStartHeight + BossCombatEditor::values.startOffsetY[int(BossAttack::Press)])) < 1.0f)
            ChangePressPhase(PressPhase::Charge);
        break;
    case PressPhase::Charge:
        // A visible pause makes the vertical lane readable.
        if (pressFrame_ >= Stage2Tuning::values.verticalWaitFrames) ChangePressPhase(PressPhase::Dive);
        break;
    case PressPhase::Dive:
        pressFallSpeed_ = Clamp(pressFallSpeed_ + Stage2Tuning::values.verticalAccel,
            0.0f, Stage2Tuning::values.verticalMaxSpeed);
        position_.y -= pressFallSpeed_;
        // 機体全体が画面下へ抜けてから攻撃終了。床で急停止させない。
        // 画面外で復帰を始めるため、予兆なしの戻りに当たり判定はない。
        if (position_.y <= -Stage2VisualHalfHeight() - Stage2Tuning::values.flybyOutsideMargin) {
            ChangePressPhase(PressPhase::Impact);
            ShakeCamera(4.0f, 5);
        }
        break;
    case PressPhase::Impact:
        if (pressFrame_ >= 8) ChangePressPhase(PressPhase::Stun);
        break;
    case PressPhase::Stun:
        if (pressFrame_ >= Stage2Tuning::values.verticalRecoveryFrames)
            ChangePressPhase(PressPhase::Return);
        break;
    case PressPhase::Return: {
        // まず画面下を水平飛行し、その後画面外の側面を上昇。
        // 画面内にテレポートしたり、無警告で上昇衝突したりしない。
        const float sideX = pressTargetX_ < 640.0f ?
            -Stage2VisualHalfWidth() - Stage2Tuning::values.flybyOutsideMargin - 20.0f :
            1280.0f + Stage2VisualHalfWidth() + Stage2Tuning::values.flybyOutsideMargin + 20.0f;
        if (Abs(position_.x - sideX) > 1.0f) {
            position_.x = Approach(position_.x, sideX, 21.0f);
        }
        else {
            position_.y = Approach(position_.y, Stage2Tuning::values.verticalReturnHeight, 17.0f);
            if (Abs(position_.y - Stage2Tuning::values.verticalReturnHeight) < 1.0f) {
                flightDestination_ = { 640.0f, Stage2Tuning::values.verticalReturnHeight };
                flightTimer_ = 120;
                ChangeState(BossState::Idle);
            }
        }
        break;
    }
    }
}

void Boss::UpdateFlightStage2() {
    // Fighter-like patrol: long lateral passes with gradual banking,
    // rather than small random hovering movements. No teleporting.
    if (--flightTimer_ <= 0 ||
        Abs(flightDestination_.x - position_.x) < 35.0f) {
        const bool toRight = position_.x < 640.0f;
        flightDestination_ = { toRight ? 1070.0f : 210.0f,
            390.0f + static_cast<float>((attackCount_ % 3) - 1) * 65.0f };
        flightTimer_ = 110;
    }
    const float dx = flightDestination_.x - position_.x;
    const float dy = flightDestination_.y - position_.y;
    const float desiredX = Clamp(dx * 0.085f, -12.0f, 12.0f);
    const float desiredY = Clamp(dy * 0.052f, -4.0f, 4.0f);
    flightVelocity_.x += (desiredX - flightVelocity_.x) * 0.065f;
    flightVelocity_.y += (desiredY - flightVelocity_.y) * 0.055f;
    // 画面外への突進後は、毎フレーム移動して画面内へ再進入する。
    // Clampで突然150/1130へワープさせない。
    // 画面外からの帰還中も座標を突然クランプしない（不自然なワープの原因）。
    position_.x += flightVelocity_.x;
    position_.y += flightVelocity_.y;
    if (Abs(flightVelocity_.x) > 0.6f)
        stage2FacingLeft_ = flightVelocity_.x < 0.0f;
}

void Boss::StartCoasterStage() {
    coasterStage_ = true;
    armorPlates_ = 0;
    for (int i = 1; i < BossStage1::kArmorPlates; ++i) armorAttached_[i] = false;
    for (int i = 0; i < kAccessoryGripCount - 1; ++i) accessoryAttached_[i] = false;
}

bool Boss::TriggerFlybyForEditor(int style) {
    if (!coasterStage_ || style < 0 || style > 3) return false;
    forcedFlybyStyle_ = style;
    if (TriggerAttackForEditor(BossAttack::Rush)) return true;
    forcedFlybyStyle_ = -1;
    return false;
}

void Boss::UpdateStage2Special() {
    // 画面外からは安全に中央付近へ戻ってから予兆を開始する。
    // 帰還が終わるまでタイマーを進めないため、ミサイルだけ場外発射されない。
    if (position_.x < 270.0f || position_.x > 1010.0f ||
        position_.y < 290.0f || position_.y > 520.0f) {
        // 戦闘機は画面外から滑らかに帰還。ワープせず戻りきってから警告を開始。
        const float wantedX = Clamp((640.0f - position_.x) * 0.055f, -14.0f, 14.0f);
        const float wantedY = Clamp((410.0f - position_.y) * 0.06f, -10.0f, 10.0f);
        flightVelocity_.x += (wantedX - flightVelocity_.x) * 0.14f;
        flightVelocity_.y += (wantedY - flightVelocity_.y) * 0.14f;
        position_.x += flightVelocity_.x;
        position_.y += flightVelocity_.y;
        if (Abs(flightVelocity_.x) > 0.6f) stage2FacingLeft_ = flightVelocity_.x < 0.0f;
        stateFrame_ = 0;
        return;
    }
    flightVelocity_ = {};
    // ミサイルは実際の機首側から出るよう、攻撃前に狙う方向へ向ける。
    if (stage2SpecialMode_ == 1 && targetPosition_)
        stage2FacingLeft_ = targetPosition_->x < position_.x;
    // 0〜30F: 予兆 / 30F以降: 武装使用 / 終了後: 復帰。
    // ミサイル・カッターの生成/当たり判定はStage2Gimmicks側で同期更新。
    const int total = stage2SpecialMode_ == 1 ? 200 :
        40 + Stage2Tuning::values.cutterDuration + 25;
    if (stateFrame_ >= total) { stage2SpecialMode_ = 0; ChangeState(BossState::Recovery); }
}

void Boss::UpdateRushStage2() {
    const float maxSpeed = (battleSettings_.rushSpeed > 1.0f ? battleSettings_.rushSpeed : 1.0f) *
        (Stage2Tuning::values.rushSpeedMultiplier * Stage2Tuning::values.flybySpeed);
    const float accel = battleSettings_.rushAccel > 0.1f ? battleSettings_.rushAccel : 0.1f;
    rushVelocity_ = Clamp(rushVelocity_ + accel, 0.0f, maxSpeed);
    position_.x += rushDirection_ * rushVelocity_;
    // 前方から後方まで滑らかに高度を変える。画面内でワープしない。
    const float startX = rushDirection_ > 0 ?
        -Stage2VisualHalfWidth() - Stage2Tuning::values.flybyOutsideMargin :
        1280.0f + Stage2VisualHalfWidth() + Stage2Tuning::values.flybyOutsideMargin;
    const float span = (std::max)(1.0f, std::fabs(rushTargetX_ - startX));
    flybyProgress_ = Clamp(std::fabs(position_.x - startX) / span, 0.0f, 1.0f);
    const float arc = Stage2Tuning::values.flybyArcHeight;
    constexpr float pi = 3.14159265f;
    switch (flybyStyle_) {
    case 1: position_.y = flybyStartY_; break; // 上空を水平通過
    case 2: position_.y = flybyStartY_ + arc * flybyProgress_; break; // 急上昇
    case 3: position_.y = flybyStartY_ + arc * std::sin(flybyProgress_ * pi * 2.0f); break; // S字
    default: position_.y = flybyStartY_; break; // 低空水平
    }
}

void Boss::DrawCoasterArt(float bodyLean) const {
    const auto& a = AssetEditor::assets[AssetEditor::BossForm2];
    const float scale = GetCameraZoom() * AssetEditor::Scale(AssetEditor::BossForm2);
    const Vector2i point = PortalDraw::ToScreen(position_);
    const int w = static_cast<int>(a.width * scale);
    const int h = static_cast<int>(a.height * scale);
    float bank = 0.0f;
    if (state_ == BossState::Rush) {
        if (flybyStyle_ == 2) bank = rushDirection_ * 0.20f;
        if (flybyStyle_ == 3) bank = rushDirection_ * 0.18f *
            std::cos(flybyProgress_ * 6.2831853f);
    }
    // 戦闘機らしい機体の傾きは見た目のみ。ネジ/GPの座標はそのまま。
    // 元画像は左向きが基準。進行方向と画像の頭の向きを一致させる。
    // ステージ2で縦急降下中は必ず機首を真下へ向ける。
    const bool dive = state_ == BossState::Press && pressPhase_ == PressPhase::Dive;
    const bool faceLeft = dive ? true :
        (state_ == BossState::Rush ? rushDirection_ < 0 : stage2FacingLeft_);
    if (dive) {
        // 画像の元の機首は左。進行方向（画面下）へ機首を90度回す。
        bank = 1.5707963f;
    }
    // 反転・回転は画像中心を基準とする4頂点で描画する。
    // DrawSpriteの負倍率と回転を混ぜると原点がずれ、画面端で「尻尾」が残る。
    const float angle = -bodyLean - bank;
    const float ca = std::cos(angle), sa = std::sin(angle);
    const float flipX = faceLeft ? 1.0f : -1.0f;
    auto vertex = [&](float px, float py) -> Vector2i {
        px *= flipX;
        return { static_cast<int>(std::lround(point.x + px * ca - py * sa)),
                 static_cast<int>(std::lround(point.y + px * sa + py * ca)) };
        };
    const float halfW = w * 0.5f, halfH = h * 0.5f;
    const Vector2i lt = vertex(-halfW, -halfH);
    const Vector2i rt = vertex(halfW, -halfH);
    const Vector2i lb = vertex(-halfW, halfH);
    const Vector2i rb = vertex(halfW, halfH);
    Novice::DrawQuad(lt.x, lt.y, rt.x, rt.y, lb.x, lb.y, rb.x, rb.y,
        0, 0, a.width, a.height, a.handle, 0xFFFFFFFF);
}
