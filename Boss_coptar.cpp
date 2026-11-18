#include "Boss_coptar.h"
#include "Camera.h"
#include "PortalDraw.h"
#include "PortalConfig.h"
#include <cmath>
#include <Novice.h>

namespace {
    float Clamp(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
    float Abs(float v) { return v < 0.0f ? -v : v; }
    float Dist2(Vector2 a, Vector2 b) {
        const float x = a.x - b.x, y = a.y - b.y;
        return x * x + y * y;
    }
    float Approach(float a, float b, float step) {
        return a < b ? (a + step > b ? b : a + step) : (a - step < b ? b : a - step);
    }
}
namespace {
    struct BossPartArt { const char* path; float left; float top; int width; int height; };
    constexpr float kArtScale = 0.5f;
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
        const Vector2 offset = ImageOffset(art);
        const Vector2 center = { base.x + offset.x, base.y + offset.y };
        if (handle >= 0) {
            const Vector2i screen = PortalDraw::ToScreen(center);
            const float drawScale = GetCameraZoom() * kArtScale;
            const int w = static_cast<int>(art.width * drawScale);
            const int h = static_cast<int>(art.height * drawScale);
            Novice::DrawSprite(screen.x - w / 2, screen.y - h / 2,
                handle, drawScale, drawScale, -angle, 0xFFFFFFFF);
        }
        else {
            // 画像を見つけられない場合でもゲームを止めず、目印のBOXを表示する。
            PortalDraw::Box(center, 22.0f, 22.0f, 0xA9ACC6FF);
        }
    }
}

// オブジェクトを使い始めるときに値を初期状態へ戻す。リトライ時にも重要。
void Boss::Initialize(Vector2 spawnPos) {
    homePosition_ = position_ = spawnPos;
    flightVelocity_ = {}; flightTimer_ = 0; flightDirection_ = 1;
    targetPosition_ = nullptr; // Stage::InitStageがあとからSetTargetで接続する
    hp_ = kMaxHp;
    armorRebuilt_ = false; rebuildOnWake_ = false; mashCount_ = 0; armorFallVelocity_ = 0.0f;
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
    normalComboHits_ = normalComboTimer_ = spinCounterCooldown_ = 0;
    rushDirection_ = -1;
    wireUsedDuringRush_ = rushHitPlayer_ = rushDodgeStunPending_ = false;
    rushVelocity_ = 0.0f;
    rushTargetY_ = BossStage1::kRushGroundY;
    rushTargetX_ = spawnPos.x;
    toyAimY_ = BossStage1::kFloorY + 90.0f;
    toyPosition_ = toyVelocity_ = {};
    toyReturning_ = toyLanded_ = toyHasHitPlayer_ = false;
    nextBulletIndex_ = 0;
    for (auto& b : bullets_) b = {};
    visual_.Initialize(spawnPos, BossStage1::kFloorY);
    ChangeState(BossState::Idle);
}
// ボスが狙うプレイヤー位置の参照先を保存する。ポインタ先の寿命に注意。
void Boss::SetTarget(const Vector2* playerPosition) { targetPosition_ = playerPosition; }
// 【調整ポイント】外装の各取っ掛かりは本体に追従する。
// ボスのX移動と一緒に動く。0=左/1=中央/2=右。
// 既存のコア円とは別の「狙って掴む専用ポイント」。
bool Boss::IsArmorAttached(int plate) const {
    if (plate < 0 || plate >= BossStage1::kArmorPlates || IsDefeated()) return false;
    // まずジェットとパイプを外す。次に3本のプロペラが狙える。
    if (!AllAccessoriesRemoved()) return false;
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
    ++attackCount_;
}
// ボスの行動状態を切り替え、状態ごとの開始演出・タイマーを初期化する。
void Boss::ChangeState(BossState next) {
    state_ = next;
    stateFrame_ = 0;
    // 【注目】【状態別の開始処理】新状態になった瞬間に目標固定・弾消去・カメラ演出等を行う。
    switch (state_) {
    case BossState::Idle:
        flightVelocity_ = {};
        visual_.SetMode(BossVfxMode::Idle);
        break;
    case BossState::Warning:
        // プレス予兆は最初に標的を決め、攻撃直前には追尾しない。
        pressTargetX_ = targetPosition_ ? targetPosition_->x : position_.x;
        // 【床上の突進】プレイヤーが空中にいても、高さを追尾しない。
        // 予兆から本番まで床スレスレの一定高度。ジャンプ/ワイヤーで飛び越せる。
        rushTargetY_ = BossStage1::kRushGroundY;
        if (nextAttack_ == BossAttack::Rush) {
            wireUsedDuringRush_ = false;
            rushHitPlayer_ = false;
            rushDodgeStunPending_ = false;
        }
        visual_.SetMode(BossVfxMode::Warning);
        break;
    case BossState::Rush:
        // 床上の高さを固定。プレイヤーを追尾せず、予兆で決めたXを通過する。
        position_.y = BossStage1::kRushGroundY;
        rushStart_ = position_;
        rushVelocity_ = 0.0f;
        visual_.SetMode(BossVfxMode::Rush);
        break;
    case BossState::Recovery: visual_.SetMode(BossVfxMode::Recovery); break;
    case BossState::ArmorCurl:
        // ネジ全解除で専用イベントへ移る。飛んでいた弾が連打を邪魔しないよう消す。
        for (auto& b : bullets_)b.active = false;
        toyReturning_ = true;
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
        toyPosition_ = { position_.x + toyDirection_ * 47.0f,position_.y - 20.0f };
        toyVelocity_ = { toyDirection_ * (nextAttack_ == BossAttack::Top ? battleSettings_.topInitialSpeed : 14.5f),3.0f };
        toyOriginX_ = toyPosition_.x;
        toyReturning_ = toyLanded_ = toyHasHitPlayer_ = false;
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

// 【第2段階】HP50%以下になったタイミングで一度だけ外装とネジを復元。
// 続けて衝撃を発生させるのは ChangeState(WakeBlast) の既存経路。
// HP50%以下で1度だけ外装・ネジを復活させる。
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
    hp_ -= BossStage1::kMashDamage + bonus;
    hitstop_ = 0; // 連打の受付を止めない。ボスの外見・画面揺れだけでヒット感を表す。
    ShakeCamera(2.0f + static_cast<float>(mashCount_ > 12 ? 12 : mashCount_) * 0.16f,
        BossStage1::kMashHitShakeFrames);
    if (hp_ <= 0) { hp_ = 0; ChangeState(BossState::Defeated); }
    else if (hp_ <= kMaxHp / 2 && !armorRebuilt_) {
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
    if (next == PressPhase::Return) pressReturnStart_ = position_;
}
bool Boss::IsWeakPointOpen() const {
    return IsCoreExposed() &&
        (state_ == BossState::ArmorStun || state_ == BossState::Dizzy ||
            (state_ == BossState::Press && pressPhase_ == PressPhase::Stun));
}
// 突進中にワイヤーが使われたか記録し、空振り後の失速判断に使う。
void Boss::NoteWireMotion(bool usingWire) {
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
    if (hp_ <= 0) { hp_ = 0;ChangeState(BossState::Defeated); }
    else if (hp_ <= kMaxHp / 2 && !armorRebuilt_) {
        // 通常攻撃でHP半分を跨いだときも第2段階へ（連打以外のルートを塞がない）。
        rebuildOnWake_ = true;
        ChangeState(BossState::WakeBlast);
    }
    else if (amount >= 14) { hitstop_ = 2;ShakeCamera(2.0f, 4); }
    applyCounter();
    return true;
}
void Boss::UpdatePress() {
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
        position_.x = pressReturnStart_.x + (homePosition_.x - pressReturnStart_.x) * ease;
        position_.y = pressReturnStart_.y + (homePosition_.y - pressReturnStart_.y) * ease;
        if (pressFrame_ >= 34) { ChangeState(BossState::Idle); }
        break;
    }
    }
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
    b.position = { position_.x,target.y > position_.y ? position_.y + 12.0f : position_.y - 12.0f };
    float dx = target.x - b.position.x, dy = target.y - b.position.y + spreadIndex * 34.0f;
    const float len = std::sqrt(dx * dx + dy * dy);
    const float speed = IsEnraged() ? 15.5f : 13.0f; // 銃撃は数ではなく速度で強く
    if (len > 0.001f) b.velocity = { dx / len * speed,dy / len * speed };
    else b.velocity = { -speed,0.0f };
    visual_.OnMuzzleFlash(b.position);
}
void Boss::UpdateToy() {
    if (nextAttack_ == BossAttack::Top) {
        // 【コマ】低空を回転しながら滑り、壁で跳ね返る。
        // 当たり判定は円1つ。描画は四角形2〜3枚のみ。
        toyPosition_.x += toyVelocity_.x;
        if (!toyLanded_) {
            // ボスが放り投げたガラクタが重力で落ちてから回転する。
            toyVelocity_.y -= 1.1f;
            toyPosition_.y += toyVelocity_.y;
            if (toyPosition_.y <= BossStage1::kFloorY + 25.0f) {
                toyPosition_.y = BossStage1::kFloorY + 25.0f;
                toyLanded_ = true;
                ShakeCamera(3.0f, 4);
            }
        }
        else {
            toyPosition_.y = BossStage1::kFloorY + 25.0f +
                11.0f * Abs(std::sin(stateFrame_ * 0.18f));
        }
        if (toyPosition_.x<BossStage1::kArenaLeft || toyPosition_.x>BossStage1::kArenaRight) {
            toyVelocity_.x *= -0.93f;
            toyPosition_.x = Clamp(toyPosition_.x, BossStage1::kArenaLeft, BossStage1::kArenaRight);
            toyReturning_ = true;
            ShakeCamera(3.0f, 4);
        }
        // ステージ端で反射したら、投げた位置へ戻った時点で退場。
        const bool hasReturned = toyReturning_ &&
            ((toyDirection_ < 0 && toyPosition_.x >= toyOriginX_) ||
                (toyDirection_ > 0 && toyPosition_.x <= toyOriginX_));
        if (hasReturned) { ChangeState(BossState::Idle); return; }
    }
    else {
        // 【ヨーヨー】伸びる→一瞬停止→縮む。実体を作りすぎず位置計算だけ。
        const float t = Clamp(stateFrame_ / static_cast<float>(battleSettings_.yoyoDuration), 0.0f, 1.0f);
        const float pulse = std::sin(t * 3.14159265f);
        toyPosition_.x = position_.x + toyDirection_ * pulse * battleSettings_.yoyoRange;
        toyPosition_.y = toyAimY_ + BossStage1::kYoYoHeightWave * std::sin(t * 6.2831853f);
    }
    const int duration = nextAttack_ == BossAttack::YoYo ?
        battleSettings_.yoyoDuration : battleSettings_.topDuration;
    if (stateFrame_ >= duration) ChangeState(BossState::Idle);
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
        const float r = playerRadius + BossStage1::kToyRadius;
        if (Dist2(playerPosition, toyPosition_) <= r * r && !toyHasHitPlayer_) {
            toyHasHitPlayer_ = true;
            if (hitSource) *hitSource = toyPosition_;
            return BossStage1::kToyDamage;
        }
        if (Dist2(playerPosition, toyPosition_) > r * r * 4.0f) toyHasHitPlayer_ = false;
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
    if (--flightTimer_ <= 0) {
        flightTimer_ = IsEnraged() ? battleSettings_.flightIntervalPhase2 : battleSettings_.flightInterval;
        flightDirection_ = -flightDirection_;
        // プレイヤーのすぐ上で静止しないよう左右に散らして位置を決める。
        const float playerX = targetPosition_ ? targetPosition_->x : 420.0f;
        aiRandomState_ ^= aiRandomState_ << 13;
        aiRandomState_ ^= aiRandomState_ >> 17;
        aiRandomState_ ^= aiRandomState_ << 5;
        const float random = (aiRandomState_ % 1000u) / 999.0f;
        const float offset = 320.0f + random * 230.0f;
        homePosition_.x = Clamp(playerX + flightDirection_ * offset, 280.0f, 1010.0f);
        homePosition_.y = 400.0f + random * 135.0f;
    }
    // 速度ベクトルを目標方向へ少しずつ近づける（急な瞬間移動を防ぐ）。
    const float maxSpeed = IsEnraged() ? battleSettings_.flightSpeedPhase2 : battleSettings_.flightSpeed;
    const float wantedX = Clamp((homePosition_.x - position_.x) * 0.065f, -maxSpeed, maxSpeed);
    const float wantedY = Clamp((homePosition_.y - position_.y) * 0.050f, -maxSpeed * 0.65f, maxSpeed * 0.65f);
    flightVelocity_.x += (wantedX - flightVelocity_.x) * 0.14f;
    flightVelocity_.y += (wantedY - flightVelocity_.y) * 0.12f;
    position_.x = Clamp(position_.x + flightVelocity_.x, 225.0f, 1055.0f);
    position_.y = Clamp(position_.y + flightVelocity_.y, 345.0f, 555.0f);
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
    case BossState::Idle:
        // 【直した原因③】射撃で横へノックバックした後、そのまま位置がズレ続けていた。
        // 次の技までに必ず戻す。ボスが画面外へ逃げたりフックが射程外にならない。
        UpdateFlight();
        if (stateFrame_ >= (IsEnraged() ? 24 : battleSettings_.idleFrames)) {
            ChooseAttack();
            ChangeState(BossState::Warning);
        }
        break;
    case BossState::Warning: {
        // 技ごとに予兆時間を変える。種類は今までの5つを維持。
        int durationFrames = BossStage1::kWarningFrames;
        switch (nextAttack_) {
        case BossAttack::Rush: durationFrames = battleSettings_.rushWarning;break;
        case BossAttack::Press:durationFrames = battleSettings_.pressWarning;break;
        case BossAttack::Shot: durationFrames = battleSettings_.shotWarning;break;
        case BossAttack::Top:case BossAttack::YoYo:durationFrames = battleSettings_.toyWarning;break;
        }
        const float duration = static_cast<float>(IsEnraged() ?
            (durationFrames * 3 / 4) : durationFrames);
        // プレスは予兆前半だけ追尾し、後半は予測位置が固定される。
        if (nextAttack_ == BossAttack::Press && stateFrame_ < 12 && targetPosition_)
            pressTargetX_ = targetPosition_->x;
        // 突進は床すれすれへ降下してから横へ飛ぶ。ワイヤー/ジャンプで避けられる。
        if (nextAttack_ == BossAttack::Rush)
            position_.y += (rushTargetY_ - position_.y) * 0.13f;
        // 予兆中はターゲットを追い過ぎず、赤い狙いBOXだけで危険を示す。
        // 警告の十字も技に対応した位置へ。全部が床警告になる誤解をなくす。
        const Vector2 targetPos = targetPosition_ ? *targetPosition_ : position_;
        float warningY = targetPos.y;
        if (nextAttack_ == BossAttack::Rush || nextAttack_ == BossAttack::Press)
            warningY = BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f;
        else if (nextAttack_ == BossAttack::Top)
            warningY = BossStage1::kFloorY + 25.0f;
        visual_.SetWarning(stateFrame_ / duration,
            { nextAttack_ == BossAttack::Press ? pressTargetX_ : targetPos.x,warningY });
        if (stateFrame_ >= duration) {
            if (targetPosition_) rushDirection_ = (targetPosition_->x < position_.x) ? -1 : 1;
            // 【突進の狙い固定】床の高さだけでなく、走り抜ける位置もここで確定。
            // 狙った地点を通過したら止まり、回避成功時すぐ反撃できる距離を残す。
            rushTargetX_ = Clamp((targetPosition_ ? targetPosition_->x : position_.x) +
                rushDirection_ * BossStage1::kRushOvershoot,
                BossStage1::kArenaLeft + BossStage1::kBossWidth * 0.5f,
                BossStage1::kArenaRight - BossStage1::kBossWidth * 0.5f);
            switch (nextAttack_) {
            case BossAttack::Press: ChangeState(BossState::Press); break;
            case BossAttack::Shot: ChangeState(BossState::Shot); break;
            case BossAttack::Rush: ChangeState(BossState::Rush); break;
            case BossAttack::Top: case BossAttack::YoYo: ChangeState(BossState::Toy); break;
            }
        }
        break;
    }
    case BossState::Rush: {
        // 【地上突進】Y座標は一切追跡せず、床のすぐ上を左右に突き抜ける。
        position_.y = Approach(position_.y, BossStage1::kRushGroundY, 7.0f);
        // 一瞬逆側へ溜めてから、速度を上げる。線を大量に描かなくても迫力。
        if (stateFrame_ <= 2) position_.x -= rushDirection_ * 3.0f;
        else {
            rushVelocity_ = Clamp(rushVelocity_ + battleSettings_.rushAccel, 0.0f,
                battleSettings_.rushSpeed * (IsEnraged() ? 1.25f : 1.0f));
            position_.x += rushDirection_ * rushVelocity_;
        }
        const bool passedTarget = rushDirection_ < 0 ?
            position_.x <= rushTargetX_ : position_.x >= rushTargetX_;
        if (stateFrame_ >= BossStage1::kRushFrames || passedTarget ||
            position_.x<BossStage1::kArenaLeft || position_.x>BossStage1::kArenaRight) {
            // 逃げに成功したら「画面端まで戻ってから失速」ではなく、ここで失速する。
            // 回避→反撃のタイミングが近くなり、ワイヤーの使い道が生まれる。
            rushDodgeStunPending_ = wireUsedDuringRush_ && !rushHitPlayer_;
            ShakeCamera(3.0f, 5);
            ChangeState(rushDodgeStunPending_ ? BossState::Dizzy : BossState::Recovery);
        }
        break;
    }
    case BossState::Recovery:
        // 一度減速/反動を見せてからホームへ戻る。
        if (stateFrame_ < BossStage1::kRushBrakeFrames) {
            position_.x += rushDirection_ * rushVelocity_ * (1.0f - stateFrame_ / static_cast<float>(BossStage1::kRushBrakeFrames)) * 0.28f;
        }
        else {
            const float dx = homePosition_.x - position_.x;
            position_.x = Approach(position_.x, homePosition_.x, IsEnraged() ? 11.0f : 8.0f);
            position_.y = Approach(position_.y, homePosition_.y, 4.0f);
            if (Abs(dx) < 9.0f && Abs(homePosition_.y - position_.y) < 5.0f) {
                ChangeState(rushDodgeStunPending_ ? BossState::Dizzy : BossState::Idle);
            }
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
            // 時間切れでもプレイヤーを吹き飛ばす。HP半分は一度しか復活しない。
            rebuildOnWake_ = !armorRebuilt_ && hp_ <= kMaxHp / 2;
            ChangeState(BossState::WakeBlast);
        }
        break;
    case BossState::WakeBlast:
        // 【復帰】ここに入ったフレームだけStageへ衝撃イベントを通知。
        // その後ボスは高い定位置に戻って元の5技AIを再開する。
        if (stateFrame_ >= BossStage1::kArmorWakeFrames) {
            // 地面から上へ戻してから通常AI復帰。以後の上昇もIdleの既存処理に任せる。
            ChangeState(BossState::Idle);
        }
        break;
    case BossState::Dizzy:
        // 【反撃猶予】ワイヤーで突進をかわした直後の隙。
        // コア露出済なら与ダメUP。まだ外装があれば通常攻撃でも剥がしやすい。
        if (stateFrame_ >= battleSettings_.rushDizzy)
            ChangeState(BossState::Recovery); // 隙の後で帰還し、次の予兆へ
        break;
    case BossState::Press: UpdatePress(); break;
    case BossState::Shot:
        // 2回の3方向射撃（計6発）。弾数が少なくても回避を考えさせる。
        if (stateFrame_ == 9 || stateFrame_ == 28) {
            for (int i = -1;i <= 1;++i) Shoot(i);
            ShakeCamera(1.8f, 3);
            position_.x -= rushDirection_ * 16.0f; // 反動。Idleでホームへ復帰する
        }
        else if (stateFrame_ > 9 && stateFrame_ < 42) {
            // 急なノックバックの後にじわっと戻る。追加描画なしの重さ。
            position_.x = Approach(position_.x, homePosition_.x, BossStage1::kShotReturnSpeed);
        }
        if (stateFrame_ >= 48) ChangeState(BossState::Idle);
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
void Boss::Draw() const {
    // 【技が見て分かる予兆】種類を増やさず、各攻撃で異なる警告図形を1～2個だけ表示。
    // 大量パーティクルは使わない。攻撃開始後は予兆を消して判定と対応させる。
    if (state_ == BossState::Warning) {
        const unsigned int color = (stateFrame_ / 5) % 2 ? 0xF15D58AA : 0xF15D5844;
        const Vector2 target = targetPosition_ ? *targetPosition_ : position_;
        switch (nextAttack_) {
        case BossAttack::Rush:
            PortalDraw::Box({ 640.0f,rushTargetY_ }, 1120.0f, 7.0f, color);
            break;
        case BossAttack::Press:
            // どこへ落ちてくるか床に予測エリア。移動して回避できる。
            PortalDraw::Box({ pressTargetX_,BossStage1::kFloorY + 5 }, BossStage1::kBossWidth + 35.0f, 7.0f, color);
            break;
        case BossAttack::Shot:
            // 射線を1本だけ示す。発射後の弾の位置は固定される。
            PortalDraw::Line(position_, target, color);
            break;
        case BossAttack::Top:
            // 地面を走るコマ：床が危険になることを線で示す。
            PortalDraw::Box({ 640.0f,BossStage1::kFloorY + 24.0f }, 1080.0f, 3.0f, color);
            break;
        case BossAttack::YoYo:
            // プレイヤーの高度で往復するヨーヨー。
            PortalDraw::Line(position_, { target.x,target.y }, color);
            PortalDraw::Box(target, 28.0f, 3.0f, color);
            break;
        }
    }
    visual_.DrawBehind(position_);
    float bodyLean = 0.0f;
    if (state_ != BossState::ArmorFalling && state_ != BossState::ArmorImpact &&
        state_ != BossState::ArmorStun && state_ != BossState::WakeBlast) {
        if (!armorAttached_[1]) bodyLean -= 0.055f;
        if (!armorAttached_[2]) bodyLean += 0.045f;
        if (!armorAttached_[3]) bodyLean += 0.065f;
    }
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
    // ボス側のグリップポイントを画像より手前に表示する。
    for (int i = 0; i < kAccessoryGripCount; ++i) {
        if (!IsAccessoryGripEnabled(i)) continue;
        const Vector2 point = GetAccessoryGripPoint(i);
        PortalDraw::Box(point, 11.0f, 3.0f, 0xC6ECFFFF);
        PortalDraw::Box(point, 3.0f, 11.0f, 0xC6ECFFFF);
    }
    // 【連打演出】本体の白BOXを再利用。色を重ねるだけで追加描画は最大1BOX。
    // 256x256の元サイズは変えず、連打で一瞬赤く点滅する。
    if (state_ == BossState::ArmorStun && mashCount_ > 0 && stateFrame_ % 6 < 2) {
        PortalDraw::Box(position_, 256.0f, 256.0f, 0xFF45664B);
    }
    // 【外装シート】256x256の本体1倍はそのまま。上に薄い外装BOXを1枚重ねる。
    // ネジ4つを外すと、板が下端を支点に回転してペロッとめくれる。
    if (state_ == BossState::ArmorCurl) {
        const float t = state_ == BossState::ArmorCurl ?
            Clamp(stateFrame_ / static_cast<float>(BossStage1::kArmorCurlFrames), 0.0f, 1.0f) : 0.0f;
        const float angle = t * t * 1.45f;
        const Vector2 shellCenter = { position_.x + std::sin(angle) * 70.0f,
            position_.y - 85.0f + std::cos(angle) * 84.0f };
        PortalDraw::RotatedBox(shellCenter, 226.0f, 22.0f, angle, 0xB7C4D1CC);
    }
    // プロペラ3本が破壊対象。本体はグリップ可能でも剥がれない。
    // 各板の上に小さな黄色の十字を描き、そこへ触手を当てて引き剥がす。
    // 表示1枚あたりBOX3個。板が減れば描画も減る。
    for (int i = 0;i < BossStage1::kArmorPlates;++i) {
        if (IsArmorAttached(i)) {
            const Vector2 p = GetArmorGripPoint(i);
            // 本体のポイントは移動用。プロペラだけが外れる。
            const float tension = i == armorStrainPlate_ ? armorStrain_ : 0.0f;
            PortalDraw::RotatedBox({ p.x,p.y - 6.0f + tension * 4.0f },
                34.0f + tension * 12.0f, 16.0f - tension * 4.0f,
                tension * (i == 0 ? -0.24f : 0.24f), tension > 0.55f ? 0xFFF0B0FF : 0xA7B1C1FF);
            PortalDraw::Box(p, 13.0f, 3.0f, 0xFFE16AFF);
            PortalDraw::Box(p, 3.0f, 13.0f, 0xFFE16AFF);
        }

    }
    if (state_ == BossState::Dizzy || state_ == BossState::ArmorStun || state_ == BossState::ArmorImpact) {
        // 回避成功の合図：本体の上に黄色BOXを1枚。新規パーティクル不要。
        PortalDraw::Box({ position_.x,position_.y + 53.0f }, 49.0f, 7.0f, 0xFFEA73FF);
    }
    if (IsCoreExposed() && !IsDefeated() && state_ != BossState::ArmorCurl) {
        const float pulse = 1.0f + 0.17f * std::sin(stateFrame_ * 0.21f);
        PortalDraw::Box(GetCoreHitPoint(), 20.0f * pulse, 22.0f * pulse,
            IsWeakPointOpen() ? 0xFF3030FF : 0xC73A48FF);
    }
    // 「コマ」は回転BOX2枚、「ヨーヨー」は本体と1本の線＋BOX2枚。
    if (state_ == BossState::WakeBlast && stateFrame_ < 12) {
        // 起き上がり：描画BOX1枚で衝撃を表す。Player側へ同じ瞬間に速度を与える。
        PortalDraw::Box(position_, 300.0f + stateFrame_ * 17.0f, 8.0f, 0xFFE9BCCC);
    }
    if (state_ == BossState::Toy) {
        if (nextAttack_ == BossAttack::YoYo) PortalDraw::Line(position_, toyPosition_, 0xF0D3FFFF);
        PortalDraw::RotatedBox(toyPosition_, 44.0f, 24.0f, stateFrame_ * 0.43f, 0xFFE6A3FF);
        PortalDraw::RotatedBox(toyPosition_, 13.0f, 42.0f, -stateFrame_ * 0.56f, 0xC77AFFFF);
    }
    for (const auto& b : bullets_) if (b.active) visual_.DrawBullet(b.position, b.velocity);
    visual_.DrawFront();
    if (IsDefeated()) PortalDraw::Box(position_, 72.0f, 13.0f, 0x777777FF);
}
