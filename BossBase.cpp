#include "AssetEditor.h"
#include "Stage2Tuning.h"
#include "BossCombatEditorSettings.h"
// 【このファイルの役割】BossBase.cpp — ステージ1・2共通のボス状態・外装・戦闘処理
// 5種類の攻撃、4ネジ解除、墜落、連打スタン、第2段階、被弾、撃破を管理する。

#include "BossBase.h"
#include "BossHelpers.h"
using namespace BossHelpers;
using PortalMath::Clamp;
using PortalMath::Abs;
using PortalMath::Dist2;
using PortalMath::Approach;

#include "Camera.h"
#include "PortalDraw.h"
#include "PortalConfig.h"
#include <cmath>
#include <cstring>
#include <Novice.h>




// オブジェクトを使い始めるときに値を初期状態へ戻す。リトライ時にも重要。
void Boss::Initialize(Vector2 spawnPos) {
    position_ = flightDestination_ = spawnPos;
    flightVelocity_ = {}; flightTimer_ = 0; flightDirection_ = 1;
    targetPosition_ = nullptr; // Stage::InitStageがあとからSetTargetで接続する
    hp_ = GetMaxHp();
    coasterStage_ = false; armorRebuilt_ = rebuildOnWake_ = false; mashCount_ = 0; armorFallVelocity_ = 0.0f;
    stage2SpecialMode_ = 0;
    flybyStyle_ = 0; forcedFlybyStyle_ = -1; flybyStartY_ = 0.0f;
    flybyProgress_ = 0.0f; flybySequence_ = 0; stage2FacingLeft_ = true; stage2LastSlot_ = -1;
    armorPlates_ = BossStage1::kArmorPlates - 1;
    for (int i = 0; i < kAccessoryGripCount - 1; ++i) {
        accessoryAttached_[i] = true;
        detachedAccessoryPosition_[i] = {};
        detachedAccessoryFrames_[i] = 0;
    }
    // 【初期化】毎回すべての外装ポイントを復元する。
    for (int i = 0;i < BossStage1::kArmorPlates;++i) {
        armorAttached_[i] = true; peeledPosition_[i] = {}; peeledFrames_[i] = 0;
    }
    armorDamage_ = hitCooldown_ = hitstop_ = defeatFrame_ = attackCount_ = 0;
    armorStrainPlate_ = -1; armorStrain_ = 0.0f;
    wakeBlastPending_ = false;
    targetVelocity_ = {};targetGrounded_ = true;targetUsingWire_ = false;targetStillFrames_ = 0;
    previousAttack_ = BossAttack::Press; aiRandomState_ = 0x9E3779B9u;
    for (int i = 0; i < 5; ++i) attackStaleness_[i] = 0;
    normalComboHits_ = normalComboTimer_ = spinCounterCooldown_ = 0;
    rushDirection_ = -1;
    wireUsedDuringRush_ = rushHitPlayer_ = rushDodgeStunPending_ = false;
    rushVelocity_ = 0.0f;
    rushTargetY_ = BossStage1::kRushGroundY;
    rushTargetX_ = spawnPos.x;
    toyAimY_ = BossStage1::kFloorY + 90.0f;
    toyPosition_ = toyVelocity_ = {};
    toyHasHitPlayer_ = false;
    nextBulletIndex_ = 0;
    for (auto& b : bullets_) b = {};
    visual_.Initialize(spawnPos, BossStage1::kFloorY);
    ChangeState(BossState::Idle);

    LoadTexture();
}
// ボスが狙うプレイヤー位置の参照先を保存する。ポインタ先の寿命に注意。
void Boss::SetTarget(const Vector2* playerPosition) { targetPosition_ = playerPosition; }
// 【調整ポイント】外装の各取っ掛かりは本体に追従する。
// ボスのX移動と一緒に動く。0=左/1=中央/2=右。
// 既存のコア円とは別の「狙って掴む専用ポイント」。
bool Boss::IsArmorAttached(int plate) const {
    if (plate < 0 || plate >= BossStage1::kArmorPlates || IsDefeated()) return false;
    // 外装は任意の順で攻略可能。全外装が外れた時にのみボスがダウンする。
    // 以前の「ジェット全破壊までプロペラGPを無効化」は撤廃。
    if (plate == 0) {
        // 本体は剥がせないが、プロペラ3本がなくなったら移動用グリップとして使える。
        return armorPlates_ == 0;
    }
    return armorAttached_[plate];
}
Vector2 Boss::GetArmorGripPoint(int plate) const {
    if (plate < 0 || plate >= BossStage1::kArmorPlates) return position_;
    return { position_.x + battleSettings_.partGrips[plate].x,
        position_.y + battleSettings_.partGrips[plate].y };
}
Vector2 Boss::GetAccessoryGripPoint(int index) const {
    if (index < 0 || index >= kAccessoryGripCount) return position_;
    return { position_.x + battleSettings_.accessoryGrips[index].x,
        position_.y + battleSettings_.accessoryGrips[index].y };
}
bool Boss::AllAccessoriesRemoved() const {
    for (int i = 0; i < kAccessoryGripCount - 1; ++i)
        if (accessoryAttached_[i]) return false;
    return true;
}
bool Boss::IsAccessoryGripEnabled(int index) const {
    if (index < 0 || index >= kAccessoryGripCount || IsDefeated()) return false;
    // 最終段階：全ての外装が外れたら目だけをグリップ可能にする。
    if (index == kAccessoryGripCount - 1)
        return armorPlates_ == 0 && AllAccessoriesRemoved();
    // 最初の段階：ジェット4個とパイプは最初から剥がせる。
    return accessoryAttached_[index];
}
bool Boss::TryPeelAccessory(int index) {
    if (index < 0 || index >= kAccessoryGripCount - 1 || !IsAccessoryGripEnabled(index)) return false;
    if (state_ == BossState::ArmorCurl || state_ == BossState::ArmorStun || IsDefeated()) return false;
    accessoryAttached_[index] = false;
    detachedAccessoryPosition_[index] = position_;
    detachedAccessoryFrames_[index] = 100;
    hitstop_ = 3;
    ShakeCamera(2.5f, 4);
    // ジェット・パイプをすべて外しても墜落しない。
    // 次はプロペラ3本を外す段階になる。
    return true;
}
// 外装の引っ張られ具合を演出値として反映する。
void Boss::SetArmorStrain(int plate, float progress) {
    // ボスの外装そのものがねじれて張力を表す。新しい描画物は作らない。
    armorStrainPlate_ = IsArmorAttached(plate) ? plate : -1;
    armorStrain_ = Clamp(progress, 0.0f, 1.0f);
}

// 【破壊ポイント】プロペラ3本は触手で掴むと外れる。本体は外れない。
// ネジを1つ外してもコアは出ない。4点すべて外してから外装がペロっとめくれる。
// ネジの番号が有効ならそのネジを外す。全解除時は外装めくれを開始する。
bool Boss::TryPeelArmor(int plate) {
    // 本体（0番）と目は破壊対象ではない。
    if (plate == 0 || !IsArmorAttached(plate) ||
        state_ == BossState::ArmorCurl || state_ == BossState::ArmorStun) return false;
    armorAttached_[plate] = false;
    --armorPlates_;
    peeledPosition_[plate] = position_; // 離脱時の本体位置を固定。落ちる部品だけ移動する
    peeledFrames_[plate] = 100; // 落下が視認できるよう1秒以上表示する
    hitstop_ = 3;
    ShakeCamera(2.5f, 4);
    if (armorPlates_ == 0 && AllAccessoriesRemoved()) {
        // ジェット・パイプ → プロペラ3本を外し終えたら墜落スタン。
        ChangeState(BossState::ArmorCurl);
        hitstop_ = 0;
        PunchCamera(0.11f, 10);
    }
    return true;
}
// 復帰時の吹き飛ばしイベントを1回だけ外部へ渡す。
bool Boss::ConsumeWakeBlast() {
    const bool result = wakeBlastPending_;
    wakeBlastPending_ = false;
    return result;
}
// プレイヤーの速度・着地・ワイヤー状態をAIの判断材料に保存する。
void Boss::SetTargetMotion(Vector2 velocity, bool grounded, bool usingWire) {
    targetVelocity_ = velocity;
    targetGrounded_ = grounded;
    targetUsingWire_ = usingWire;
    // 【AIの観察】動かず地上にいるプレイヤーはコマ攻撃の優先度が上がる。
    if (grounded && Abs(velocity.x) < 1.5f) {
        if (targetStillFrames_ < 180) ++targetStillFrames_;
    }
    else {
        targetStillFrames_ = 0;
    }
}
// 距離、移動、空中、ワイヤー使用などから次の5技を評価して選択する。
void Boss::ChooseAttack() {
    if (coasterStage_) ChooseAttackStage2();
    else ChooseAttackStage1();
}
// ボスの行動状態を切り替え、状態ごとの開始演出・タイマーを初期化する。
void Boss::ChangeState(BossState next) {
    state_ = next;
    stateFrame_ = 0;
    // 【注目】【状態別の開始処理】新状態になった瞬間に目標固定・弾消去・カメラ演出等を行う。
    switch (state_) {
    case BossState::Idle:
        // 攻撃終了地点を出発点にして、次の巡回先を新しく選ぶ。
        // 速度を急にゼロにせず、少し慣性を残す。
        flightVelocity_.x *= 0.45f;
        flightVelocity_.y *= 0.45f;
        flightTimer_ = 0;
        visual_.SetMode(BossVfxMode::Idle);
        break;
    case BossState::Warning:
        // プレス予兆は最初に標的を決め、攻撃直前には追尾しない。
        pressTargetX_ = (targetPosition_ ? targetPosition_->x : position_.x) + BossCombatEditor::values.startOffsetX[int(BossAttack::Press)];
        // 【床上の突進】プレイヤーが空中にいても、高さを追尾しない。
        // 予兆から本番まで床スレスレの一定高度。ジャンプ/ワイヤーで飛び越せる。
        rushTargetY_ = (coasterStage_ ? Stage2Tuning::values.rushHeight : BossStage1::kRushGroundY)
            + BossCombatEditor::values.startOffsetY[int(BossAttack::Rush)];
        if (coasterStage_ && nextAttack_ == BossAttack::Rush) {
            // 同じ横突進を4つの飛行パターンとして使う。新しい弾や画像は増やさない。
            flybyStyle_ = forcedFlybyStyle_ >= 0 ? forcedFlybyStyle_ : flybySequence_ % 4;
            forcedFlybyStyle_ = -1; ++flybySequence_;
            if (!Stage2Tuning::values.flybyVariants) flybyStyle_ = 0;
            if (flybyStyle_ == 1) rushTargetY_ += Stage2Tuning::values.flybyArcHeight * 0.75f;
            flybyStartY_ = rushTargetY_;
        }
        if (nextAttack_ == BossAttack::Rush) {
            wireUsedDuringRush_ = false;
            rushHitPlayer_ = false;
            rushDodgeStunPending_ = false;
            // 右側にいるプレイヤーには左端から、左側にいるプレイヤーには右端から突進。
            // 予兆開始時に方向を固定し、途中でプレイヤーが動いても反転させない。
            const float middleX = (BossStage1::kArenaLeft + BossStage1::kArenaRight) * 0.5f;
            rushDirection_ = (!targetPosition_ || targetPosition_->x >= middleX) ? 1 : -1;
            if (coasterStage_) stage2FacingLeft_ = rushDirection_ < 0;
            const float halfWidth = coasterStage_ ? Stage2VisualHalfWidth() : BossStage1::kBossWidth * 0.5f;
            if (coasterStage_) {
                const float outside = Stage2Tuning::values.flybyOutsideMargin + halfWidth;
                rushTargetX_ = (rushDirection_ > 0) ? 1280.0f + outside : -outside;
            }
            else {
                rushTargetX_ = (rushDirection_ > 0)
                    ? BossStage1::kArenaRight - halfWidth
                    : BossStage1::kArenaLeft + halfWidth;
            }
        }
        visual_.SetMode(BossVfxMode::Warning);
        break;
    case BossState::Rush:
        // 予兆中に端へ移動済み。ここでは位置を瞬間移動させない。
        rushStart_ = position_;
        rushVelocity_ = 0.0f;
        visual_.SetMode(BossVfxMode::Rush);
        break;
    case BossState::Stage2Special:
        // 特殊攻撃では実行中に別の攻撃を選ばない。
        flightVelocity_ = {};
        visual_.SetMode(BossVfxMode::Warning);
        break;
    case BossState::Recovery: visual_.SetMode(BossVfxMode::Recovery); break;
    case BossState::ScrewRepair:
        flightVelocity_ = {};
        visual_.SetMode(BossVfxMode::Recovery);
        break;
    case BossState::ArmorCurl:
        // ネジ全解除で専用イベントへ移る。飛んでいた弾が連打を邪魔しないよう消す。
        for (auto& b : bullets_)b.active = false;
        visual_.SetMode(BossVfxMode::Recovery);
        ShakeCamera(6.0f, 8);
        break;
    case BossState::ArmorFalling:
        // 【墜落】外装4ネジ解除後、空中から地面へ落ちる。攻撃AIは停止。
        armorFallVelocity_ = 0.0f;
        visual_.SetMode(BossVfxMode::Recovery);
        break;
    case BossState::ArmorImpact:
        // 【激突】機体が一瞬潰れる。追加パーティクルなしで画面揺れを強める。
        visual_.OnImpact(position_);
        ShakeCamera(11.0f, 14);
        break;
    case BossState::ArmorStun:
        mashCount_ = 0; // 毎回の連打タイムでリセット
        visual_.SetMode(BossVfxMode::Recovery);
        break;
    case BossState::WakeBlast:
        if (rebuildOnWake_ && !armorRebuilt_) BeginArmorRebuild();
        visual_.SetMode(BossVfxMode::Warning);
        wakeBlastPending_ = true;
        ShakeCamera(8.0f, 13);
        PunchCamera(0.12f, 12);
        break;
    case BossState::Dizzy:
        // 【回避→反撃→復帰】失速は一度だけ。立ち直った後の無限ループを防ぐ。
        rushDodgeStunPending_ = false;
        visual_.SetMode(BossVfxMode::Recovery);
        ShakeCamera(3.0f, 6);
        break;
    case BossState::Press:
        visual_.SetMode(BossVfxMode::Press);
        // 予兆途中に固定したX座標をそのまま使う。急降下の直前に追尾しない。
        ChangePressPhase(PressPhase::Align);
        break;
    case BossState::Shot: visual_.SetMode(BossVfxMode::Shot); break;
    case BossState::Toy:
        visual_.SetMode(BossVfxMode::Shot);
        // ヨーヨーは構えた瞬間のプレイヤーの高さを狙う。
        // 床にいる相手にも届き、空中なら高く伸びる。途中で追尾しない。
        toyAimY_ = Clamp(targetPosition_ ? targetPosition_->y : position_.y,
            BossStage1::kFloorY + 75.0f, PortalConfig::kWorldCeiling - 60.0f);
        toyDirection_ = (targetPosition_ && targetPosition_->x > position_.x) ? 1 : -1;
        if (nextAttack_ == BossAttack::Top) {
            // 壁に近い場合は、広い方へ投げて十分な走行距離を確保する。
            if ((toyDirection_ < 0 && position_.x - (kTopFloorLeft + kTopVisibleHalfWidth) < 300.0f) ||
                (toyDirection_ > 0 && (kTopFloorRight - kTopVisibleHalfWidth) - position_.x < 300.0f))
                toyDirection_ = -toyDirection_;
        }
        toyPosition_ = { position_.x + toyDirection_ * 47.0f + BossCombatEditor::values.startOffsetX[int(nextAttack_)],
            position_.y - 20.0f + BossCombatEditor::values.startOffsetY[int(nextAttack_)] };
        toyVelocity_ = { toyDirection_ * (nextAttack_ == BossAttack::Top ? battleSettings_.topInitialSpeed : 14.5f),3.0f };
        toyHasHitPlayer_ = false;
        topLanded_ = false;
        topWallHits_ = 0;
        topElapsed_ = 0;
        if (nextAttack_ == BossAttack::Top)
            toyPosition_.x = Clamp(toyPosition_.x, kTopFloorLeft + kTopVisibleHalfWidth,
                kTopFloorRight - kTopVisibleHalfWidth);
        ShakeCamera(2.5f, 4); // 手から放り投げる反動
        break;
    case BossState::Defeated:
        for (auto& b : bullets_) b.active = false;
        visual_.SetMode(BossVfxMode::Recovery);
        ShakeCamera(9.0f, 16);
        PunchCamera(0.13f, 13);
        break;
    }
}

void Boss::BeginArmorRebuild() {
    armorRebuilt_ = true;
    rebuildOnWake_ = false;
    armorPlates_ = BossStage1::kArmorPlates - 1;
    for (int i = 0; i < kAccessoryGripCount - 1; ++i) {
        accessoryAttached_[i] = true; detachedAccessoryFrames_[i] = 0;
    }
    for (int i = 0;i < BossStage1::kArmorPlates;++i) {
        armorAttached_[i] = true;
        peeledFrames_[i] = 0;
    }
    armorStrainPlate_ = -1; armorStrain_ = 0.0f;
    hitCooldown_ = 0;
}



// 【連打専用】左で移動・右スティックをニュートラル→倒す、を1HITとして受付。
// 接近判定をなくしているのは、カメラ演出中の攻撃が届かない事故を防ぐため。
// スタン中のスティック弾き直し等を1回の連打として数え、コアHPを減らす。
bool Boss::RegisterMashHit() {
    if (state_ != BossState::ArmorStun || IsDefeated()) return false;
    ++mashCount_;
    const int bonus = (mashCount_ / BossStage1::kMashBonusEvery);
    hp_ -= battleSettings_.mashDamage + bonus;
    hitstop_ = 0; // 連打の受付を止めない。ボスの外見・画面揺れだけでヒット感を表す。
    ShakeCamera(2.0f + static_cast<float>(mashCount_ > 12 ? 12 : mashCount_) * 0.16f,
        BossStage1::kMashHitShakeFrames);
    if (hp_ <= 0) { hp_ = 0; ChangeState(BossState::Defeated); }
    else if (!coasterStage_ && IsEnraged() && !armorRebuilt_) {
        rebuildOnWake_ = true;
        ChangeState(BossState::WakeBlast);
    }
    return true;
}
// プレス攻撃の内部段階（接近・溜め・落下など）を切り替える。
void Boss::ChangePressPhase(PressPhase next) {
    pressPhase_ = next;
    pressFrame_ = 0;
    if (next == PressPhase::Dive) pressFallSpeed_ = 3.0f;
    if (next == PressPhase::Return) {
        pressReturnStart_ = position_;
        // プレスの着地点から上昇する。高さも毎回変え、一定位置へ帰還しない。
        aiRandomState_ ^= aiRandomState_ << 13;
        aiRandomState_ ^= aiRandomState_ >> 17;
        aiRandomState_ ^= aiRandomState_ << 5;
        const float r = (aiRandomState_ % 1000u) / 999.0f;
        const float side = (flightDirection_ > 0 ? 1.0f : -1.0f);
        pressReturnDestination_ = {
            Clamp(position_.x + side * (85.0f + r * 125.0f), 230.0f, 1050.0f),
            370.0f + r * 150.0f
        };
    }
}
bool Boss::IsWeakPointOpen() const {
    return IsCoreExposed() &&
        (state_ == BossState::ArmorStun || state_ == BossState::Dizzy ||
            (state_ == BossState::Press && pressPhase_ == PressPhase::Stun));
}
// 突進中にワイヤーが使われたか記録し、空振り後の失速判断に使う。
void Boss::NoteWireMotion(bool usingWire) {
    if (coasterStage_ && (state_ != BossState::Rush || !targetPosition_ ||
        targetPosition_->y <= rushTargetY_ + kHitRadius + 24.0f)) return;
    // 【安全な判定】予兆または突進中にワイヤーが接続/自動射出されたか記録。
    // 動かずに普通に避けた場合は専用の長い失速は発生しない。
    if (usingWire && ((state_ == BossState::Warning && nextAttack_ == BossAttack::Rush) ||
        state_ == BossState::Rush)) {
        wireUsedDuringRush_ = true;
    }
}
// 攻撃種と現在の外装・スタン状態を考慮してHPや演出を更新する。
bool Boss::TakeDamage(int amount, bool isGrappleHit, bool isSpinHit, bool fromWireLaunch) {
    if (IsDefeated() || state_ == BossState::ArmorCurl || amount <= 0 || hitCooldown_ > 0) return false;
    if (coasterStage_ && state_ != BossState::Dizzy) return false;
    hitCooldown_ = BossStage1::kNormalHitCooldown;
    // 【通常攻撃】短い間隔で当てると連続ヒット。粒子を増やさずダメージで爽快感。
    if (!isGrappleHit && !isSpinHit) {
        if (normalComboTimer_ == 0) normalComboHits_ = 0;
        if (normalComboHits_ < 8) ++normalComboHits_;
        normalComboTimer_ = BossStage1::kNormalComboWindow;
        amount += (normalComboHits_ - 1) * BossStage1::kNormalComboBonusPerHit;
    }
    // 【ワイヤーからの追撃】射出の勢いを利用して攻撃すると威力が増す。
    if (fromWireLaunch) amount += BossStage1::kWireLaunchBonusDamage;
    // 【空中スピン】相手の攻撃中に当てると、一定間隔でカウンター成功。
    const bool spinCounter = isSpinHit && spinCounterCooldown_ == 0 &&
        (state_ == BossState::Rush || state_ == BossState::Shot ||
            state_ == BossState::Toy || state_ == BossState::Warning);
    const auto applyCounter = [this, spinCounter]() {
        if (spinCounter && !IsDefeated()) {
            spinCounterCooldown_ = BossStage1::kSpinCounterCooldown;
            ChangeState(BossState::Dizzy);
        }
        };
    if (armorPlates_ > 0) {
        // 【攻撃の役割】通常/回転はボスを怯ませたり、スピンでカウンターを取れる。
        // ただし外装を取り外すには、その外装の取っ掛かりを狙ってJで引く必要がある。
        // ボスの胴体を叩くだけで外装が消えると、狙う駆け引きがなくなるため。
        armorDamage_ = (armorDamage_ + 1) % BossStage1::kArmorHitsPerPlate;
        visual_.OnMuzzleFlash(position_);
        // 【外装への打撃感】外装がまだ剥がれない段階でも短く敵が怯む。
        // Bossの移動だけ2～3F止める。描画は増やさず、操作の手応えを返す。
        hitstop_ = isSpinHit ? BossStage1::kArmorHitstopSpin : BossStage1::kArmorHitstopNormal;
        ShakeCamera(isSpinHit ? BossStage1::kArmorShakeSpin : BossStage1::kArmorShakeNormal,
            isSpinHit ? 4 : 2);
        applyCounter();
        return true;
    }
    if (IsWeakPointOpen()) amount = static_cast<int>(amount * BossStage1::kCoreBonusDamage);
    hp_ -= amount;
    visual_.OnMuzzleFlash(position_);
    if (hp_ <= 0) { hp_ = 0; ChangeState(BossState::Defeated); }
    else if (!coasterStage_ && IsEnraged() && !armorRebuilt_) {
        rebuildOnWake_ = true;
        ChangeState(BossState::WakeBlast);
        return true;
    }
    else if (amount >= 14) { hitstop_ = 2; ShakeCamera(2.0f, 4); }
    applyCounter();
    return true;
}
void Boss::UpdatePress() {
    if (coasterStage_) UpdatePressStage2();
    else UpdatePressStage1();
}
void Boss::Shoot(int spreadIndex) {
    Bullet& b = bullets_[nextBulletIndex_];
    nextBulletIndex_ = (nextBulletIndex_ + 1) % kMaxBullets;
    b = {};b.active = true;b.life = 96;
    // 移動している相手を少し先読み。ただし射撃開始後に弾は追尾しない。
    const Vector2 current = targetPosition_ ? *targetPosition_ : Vector2{ position_.x - 260.0f,position_.y };
    const Vector2 target = { Clamp(current.x + targetVelocity_.x * battleSettings_.shotPredict,
                                BossStage1::kArenaLeft,BossStage1::kArenaRight),
                          Clamp(current.y + targetVelocity_.y * battleSettings_.shotPredict,
                                BossStage1::kFloorY + 20.0f,PortalConfig::kWorldCeiling) };
    b.position = { position_.x + BossCombatEditor::values.startOffsetX[int(BossAttack::Shot)],
        (target.y > position_.y ? position_.y + 12.0f : position_.y - 12.0f) + BossCombatEditor::values.startOffsetY[int(BossAttack::Shot)] };
    float dx = target.x - b.position.x, dy = target.y - b.position.y + spreadIndex * 34.0f;
    const float len = std::sqrt(dx * dx + dy * dy);
    const float speed = IsEnraged() ? 15.5f : 13.0f; // 銃撃は数ではなく速度で強く
    if (len > 0.001f) b.velocity = { dx / len * speed,dy / len * speed };
    else b.velocity = { -speed,0.0f };
    visual_.OnMuzzleFlash(b.position);
}
bool Boss::TriggerAttackForEditor(BossAttack attack) {
    // テストボタンは全5技に対応。通常AIのON/OFFは無視する。
    if (IsDefeated() || state_ == BossState::ArmorCurl || state_ == BossState::ArmorFalling ||
        state_ == BossState::ArmorImpact || state_ == BossState::ArmorStun || state_ == BossState::WakeBlast)
        return false;
    for (auto& b : bullets_)b.active = false;
    repairingScrew_ = -1;
    nextAttack_ = attack;
    ChangeState(BossState::Warning); // 始点まで移動→予兆→攻撃、ワープしない
    return true;
}

bool Boss::BeginScrewRepair(int screwIndex) {
    if (!BossCombatEditor::values.repairEnabled || state_ != BossState::Idle || IsDefeated())return false;
    repairingScrew_ = screwIndex;
    ChangeState(BossState::ScrewRepair);
    return true;
}


void Boss::UpdateBullets() {
    for (auto& b : bullets_) if (b.active) {
        b.position.x += b.velocity.x;
        b.position.y += b.velocity.y;
        // 落下ガラクタは横移動+小さな落下。見た目は弾と同じBOX。
        if (b.debris) b.velocity.y -= 0.22f;
        if (--b.life <= 0 || b.position.y < BossStage1::kFloorY - 30.0f) b.active = false;
    }
}
// ボスの攻撃とプレイヤーの距離や重なりを調べ、ダメージ量を返す。
int Boss::TryHitPlayer(Vector2 playerPosition, float playerRadius, Vector2* hitSource) {
    // 墜落から連打のあいだは無防備なボス。攻撃判定を完全に止める。
    if (IsDefeated() || state_ == BossState::ArmorCurl ||
        state_ == BossState::ArmorFalling || state_ == BossState::ArmorImpact ||
        state_ == BossState::ArmorStun) return 0;
    for (auto& b : bullets_) if (b.active) {
        const float r = playerRadius + 10.0f;
        if (Dist2(playerPosition, b.position) <= r * r) {
            b.active = false;
            if (hitSource) *hitSource = b.position;
            return BossStage1::kBulletDamage;
        }
    }
    if (state_ == BossState::Toy) {
        const float toyRadius = nextAttack_ == BossAttack::Top ? battleSettings_.topHitRadius : 48.0f;
        const float r = playerRadius + toyRadius;
        if (Dist2(playerPosition, toyPosition_) <= r * r && !toyHasHitPlayer_) {
            toyHasHitPlayer_ = true;
            if (hitSource) *hitSource = toyPosition_;
            return BossStage1::kToyDamage;
        }
        if (Dist2(playerPosition, toyPosition_) > r * r * 4.0f) toyHasHitPlayer_ = false;
    }
    if (coasterStage_ && state_ == BossState::Rush) {
        const float dx = Abs(playerPosition.x - position_.x);
        if (dx <= BossStage1::kBossWidth * 0.5f + playerRadius &&
            Abs(playerPosition.y - position_.y) <= kHitRadius + playerRadius) {
            rushHitPlayer_ = true;
            if (hitSource) *hitSource = position_;
            return 18;
        }
        return 0;
    }
    const bool dangerous = state_ == BossState::Rush ||
        (state_ == BossState::Press &&
            (pressPhase_ == PressPhase::Dive || pressPhase_ == PressPhase::Impact));
    const float r = playerRadius + kHitRadius;
    if (dangerous && Dist2(playerPosition, position_) <= r * r) {
        if (state_ == BossState::Rush) rushHitPlayer_ = true;
        if (hitSource) *hitSource = position_;
        return state_ == BossState::Rush ? 18 : 24;
    }
    return 0;
}
// 予兆前の短い待機中も、コプターが同じ一点に固定されないよう移動する。
// 目的地はプレイヤーから距離を取りつつ画面内に収め、速度をなめらかに変える。
void Boss::UpdateFlight() {
    if (coasterStage_) UpdateFlightStage2();
    else UpdateFlightStage1();
}

void Boss::Update() {
    if (hitCooldown_ > 0) --hitCooldown_;
    if (spinCounterCooldown_ > 0) --spinCounterCooldown_;
    if (normalComboTimer_ > 0) {
        --normalComboTimer_;
        if (normalComboTimer_ == 0) normalComboHits_ = 0;
    }
    if (hitstop_ > 0) {
        // ボス本体だけ停止。弾・コマ以外の投射物は動かし続ける。
        --hitstop_;
        UpdateBullets();
        visual_.Update(position_);
        return;
    }
    if (IsDefeated()) {
        ++defeatFrame_;
        if (position_.y > BossStage1::kFloorY + 30.0f) position_.y -= 3.0f;
        visual_.Update(position_);
        return;
    }
    ++stateFrame_;
    // 【注目】【状態別の開始処理】新状態になった瞬間に目標固定・弾消去・カメラ演出等を行う。
    switch (state_) {
    case BossState::Idle: {
        // 【直した原因③】射撃で横へノックバックした後、そのまま位置がズレ続けていた。
        // 毎回同じ位置へ戻さず、新しい巡回地点へ向かう。
        UpdateFlight();
        const bool stage2Ready = !coasterStage_ ||
            (position_.x > 210.0f && position_.x < 1070.0f &&
                position_.y > 190.0f && position_.y < 630.0f);
        // 画面外で攻撃の選択を始めない。再進入から一定時間の隙も作る。
        if (stage2Ready && stateFrame_ >= (coasterStage_ ? 65 :
            (IsEnraged() ? 24 : battleSettings_.idleFrames))) {
            const bool anyAttack = BossCombatEditor::AnyAttackEnabled() ||
                (coasterStage_ && (BossCombatEditor::values.missileEnabled || BossCombatEditor::values.cutterEnabled));
            if (anyAttack) {
                ChooseAttack();
                if (state_ == BossState::Idle) ChangeState(BossState::Warning);
            }
        }
        break;
    }
    case BossState::Warning: {
        // 技ごとに予兆時間を変える。種類は今までの5つを維持。
        int durationFrames = BossStage1::kWarningFrames;
        switch (nextAttack_) {
        case BossAttack::Rush: durationFrames = battleSettings_.rushWarning;break;
        case BossAttack::Press:durationFrames = battleSettings_.pressWarning;break;
        case BossAttack::Shot: durationFrames = battleSettings_.shotWarning;break;
        case BossAttack::Top:case BossAttack::YoYo:durationFrames = battleSettings_.toyWarning;break;
        }
        const float duration = static_cast<float>(coasterStage_ && nextAttack_ == BossAttack::Rush ? 60 :
            (IsEnraged() ? durationFrames * 3 / 4 : durationFrames));
        // プレスは予兆前半だけ追尾し、後半は予測位置が固定される。
        // ステージ2の縦突進は予兆開始時にX座標を固定する。
        // 予兆線を見て回避した後で追尾し直すと、表示と実際の着弾位置がずれるため。
        if (!coasterStage_ && nextAttack_ == BossAttack::Press && stateFrame_ < 12 && targetPosition_)
            pressTargetX_ = targetPosition_->x + BossCombatEditor::values.startOffsetX[int(BossAttack::Press)];
        // 突進の予兆中は「開始側の端」へ飛んでいき、床スレスレまで降りる。
        // Approachで最大移動量を制限するため、開始位置へのテレポートは起きない。
        if (nextAttack_ == BossAttack::Rush) {
            const float halfWidth = coasterStage_ ? Stage2VisualHalfWidth() : BossStage1::kBossWidth * 0.5f;
            const float startX = rushDirection_ > 0
                ? (coasterStage_ ? -halfWidth - Stage2Tuning::values.flybyOutsideMargin : BossStage1::kArenaLeft + halfWidth)
                + BossCombatEditor::values.startOffsetX[int(BossAttack::Rush)]
                : (coasterStage_ ? 1280.0f + halfWidth + Stage2Tuning::values.flybyOutsideMargin : BossStage1::kArenaRight - halfWidth)
                + BossCombatEditor::values.startOffsetX[int(BossAttack::Rush)];
            position_.x = Approach(position_.x, startX, 18.0f);
            position_.y = Approach(position_.y, rushTargetY_, 8.0f);
        }
        // 予兆中はターゲットを追い過ぎず、赤い狙いBOXだけで危険を示す。
        // 警告の十字も技に対応した位置へ。全部が床警告になる誤解をなくす。
        const Vector2 targetPos = targetPosition_ ? *targetPosition_ : position_;
        float warningY = targetPos.y;
        if (nextAttack_ == BossAttack::Rush || nextAttack_ == BossAttack::Press)
            warningY = BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f;
        else if (nextAttack_ == BossAttack::Top)
            warningY = BossStage1::kFloorY + 25.0f;
        // VFXの照準も実際のレーンへ合わせる（旧実装はプレイヤーの現在Xを指していた）。
        const Vector2 warningPoint =
            (coasterStage_ && nextAttack_ == BossAttack::Rush) ? Vector2{ 640.0f, rushTargetY_ } :
            (coasterStage_ && nextAttack_ == BossAttack::Press) ? Vector2{ pressTargetX_, 360.0f } :
            Vector2{ nextAttack_ == BossAttack::Press ? pressTargetX_ : targetPos.x, warningY };
        visual_.SetWarning(Clamp(stateFrame_ / duration, 0.0f, 1.0f), warningPoint);
        // 突進だけは、予兆時間が終わっても開始側の端に着くまで攻撃を始めない。
        // そのため、どこから予兆に入っても必ずステージの端から端まで走れる。
        bool readyToRush = true;
        if (nextAttack_ == BossAttack::Rush) {
            const float halfWidth = coasterStage_ ? Stage2VisualHalfWidth() : BossStage1::kBossWidth * 0.5f;
            const float startX = rushDirection_ > 0
                ? (coasterStage_ ? -halfWidth - Stage2Tuning::values.flybyOutsideMargin : BossStage1::kArenaLeft + halfWidth)
                + BossCombatEditor::values.startOffsetX[int(BossAttack::Rush)]
                : (coasterStage_ ? 1280.0f + halfWidth + Stage2Tuning::values.flybyOutsideMargin : BossStage1::kArenaRight - halfWidth)
                + BossCombatEditor::values.startOffsetX[int(BossAttack::Rush)];
            readyToRush = Abs(position_.x - startX) <= 0.5f &&
                Abs(position_.y - rushTargetY_) <= 0.5f;
        }
        if (stateFrame_ >= duration && readyToRush) {
            switch (nextAttack_) {
            case BossAttack::Press: ChangeState(BossState::Press); break;
            case BossAttack::Shot: ChangeState(BossState::Shot); break;
            case BossAttack::Rush: ChangeState(BossState::Rush); break;
            case BossAttack::Top: case BossAttack::YoYo: ChangeState(BossState::Toy); break;
            }
        }
        break;
    }
    case BossState::Stage2Special:
        UpdateStage2Special();
        break;
    case BossState::Rush: {
        if (coasterStage_) UpdateRushStage2();
        else UpdateRushStage1();

        // 機体の幅も含めて端に収まる位置で止める。行き過ぎや画面外への消失を防ぐ。
        const bool reachedEdge = rushDirection_ < 0 ?
            position_.x <= rushTargetX_ : position_.x >= rushTargetX_;
        if (reachedEdge) {
            // 画面外で初めて終了する。第1ステージは従来の端で停止。
            position_.x = rushTargetX_;
            rushVelocity_ = 0.0f;
            rushDodgeStunPending_ = wireUsedDuringRush_ && !rushHitPlayer_;
            ShakeCamera(3.0f, 5);
            ChangeState(rushDodgeStunPending_ ? BossState::Dizzy : BossState::Recovery);
        }
        break;
    }
    case BossState::ScrewRepair:
        // 手を伸ばす描画はStage側。移動・攻撃を停止してネジを締める。
        if (stateFrame_ >= BossCombatEditor::values.repairDuration) { repairingScrew_ = -1;ChangeState(BossState::Idle); }
        break;
    case BossState::Recovery:
        // 突進を終えた場所から滑らかに飛行再開。固定位置には戻らない。
        // 反動で場外に出ないよう、端で速度を落としてから自由飛行へ移る。
        rushVelocity_ *= 0.72f;
        if (stateFrame_ > BossStage1::kRushBrakeFrames) {
            flightTimer_ = 0; // 次のIdle更新で新しい飛行先を選ぶ
            flightVelocity_ = {};
            ChangeState(BossState::Idle);
        }
        break;
    case BossState::ArmorCurl:
        // 【めくれ】既存BOXの回転だけ。完了したらその場で墜落を開始。
        if (stateFrame_ >= BossStage1::kArmorCurlFrames) ChangeState(BossState::ArmorFalling);
        break;
    case BossState::ArmorFalling:
        // 【Y上向き】重力で下向き加速。床Y+機体半径で着地させる。
        armorFallVelocity_ -= BossStage1::kArmorFallGravity;
        if (armorFallVelocity_ < -BossStage1::kArmorFallMaxSpeed)
            armorFallVelocity_ = -BossStage1::kArmorFallMaxSpeed;
        position_.y += armorFallVelocity_;
        if (position_.y <= BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f) {
            position_.y = BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f;
            ChangeState(BossState::ArmorImpact);
        }
        break;
    case BossState::ArmorImpact:
        if (stateFrame_ >= BossStage1::kArmorImpactFrames)ChangeState(BossState::ArmorStun);
        break;
    case BossState::ArmorStun:
        // 【叩き放題】長い反撃時間。HPが0になれば吹き飛ばしを出さず撃破。
        if (stateFrame_ >= BossStage1::kMashTimeFrames) {
            // 時間切れでもプレイヤーを吹き飛ばす。
            rebuildOnWake_ = !coasterStage_ && !armorRebuilt_ && IsEnraged();
            ChangeState(BossState::WakeBlast);
        }
        break;
    case BossState::WakeBlast:
        // 【復帰】ここに入ったフレームだけStageへ衝撃イベントを通知。
        // その後ボスは今の位置から空中を巡回して5技AIを再開する。
        if (stateFrame_ >= BossStage1::kArmorWakeFrames) {
            // 地面から空中へ復帰する動作はIdleの自由飛行に任せる。
            ChangeState(BossState::Idle);
        }
        break;
    case BossState::Dizzy:
        // 【反撃猶予】ワイヤーで突進をかわした直後の隙。
        // コア露出済なら与ダメUP。まだ外装があれば通常攻撃でも剥がしやすい。
        if (stateFrame_ >= (coasterStage_ ? 120 : battleSettings_.rushDizzy))
            ChangeState(BossState::Recovery); // 隙の後で現在位置から飛行再開
        break;
    case BossState::Press: UpdatePress(); break;
    case BossState::Shot:
        if (coasterStage_) {
            // ステージ2専用：画面内で減速し、約1.5秒間に3回の射撃。
            // 画面外へ抜ける突進と交互に出し、反撃できる時間を作る。
            flightVelocity_.x *= 0.85f;
            flightVelocity_.y *= 0.85f;
            position_.x = Approach(position_.x, Clamp(position_.x, 280.0f, 1000.0f), 13.0f);
            position_.y = Approach(position_.y, 425.0f, 9.0f);
            if (stateFrame_ == 12 || stateFrame_ == 36 || stateFrame_ == 60) {
                for (int i = -1; i <= 1; ++i) Shoot(i);
                ShakeCamera(1.8f, 3);
            }
            if (stateFrame_ >= 90) ChangeState(BossState::Recovery);
        }
        else {
            // ステージ1は従来通り、2回の3方向射撃。
            if (stateFrame_ == 9 || stateFrame_ == 28) {
                for (int i = -1; i <= 1; ++i) Shoot(i);
                ShakeCamera(1.8f, 3);
                position_.x -= rushDirection_ * 16.0f;
            }
            else if (stateFrame_ > 9 && stateFrame_ < 42) {
                flightVelocity_.x *= 0.88f;
                position_.x = Clamp(position_.x + flightVelocity_.x, 225.0f, 1055.0f);
            }
            if (stateFrame_ >= 48) ChangeState(BossState::Idle);
        }
        break;
    case BossState::Toy: UpdateToy(); break;
    case BossState::Defeated: break;
    }
    // 剥がした板は小さなBOX1枚が斜めへ飛ぶだけ。消えるまで24フレーム。
    for (int i = 0;i < BossStage1::kArmorPlates;++i)
        if (peeledFrames_[i] > 0) --peeledFrames_[i];
    for (int i = 0; i < kAccessoryGripCount - 1; ++i)
        if (detachedAccessoryFrames_[i] > 0) --detachedAccessoryFrames_[i];
    UpdateBullets();
    visual_.Update(position_);
}
// 現在の状態を画面に表示する。基本的にゲームの座標やHPは変えない。

