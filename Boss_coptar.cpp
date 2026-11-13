#include "Boss_coptar.h"
#include "Camera.h"
#include "PortalDraw.h"
#include "PortalConfig.h"
#include <cmath>

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

void Boss::Initialize(Vector2 spawnPos) {
    homePosition_ = position_ = spawnPos;
    targetPosition_ = nullptr; // Stage::InitStageがあとからSetTargetで接続する
    hp_ = kMaxHp;
    armorPlates_ = BossStage1::kArmorPlates;
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

void Boss::SetTarget(const Vector2* playerPosition) { targetPosition_ = playerPosition; }
// 【調整ポイント】外装の各取っ掛かりは本体に追従する。
// ボスのX移動と一緒に動く。0=左/1=中央/2=右。
// 既存のコア円とは別の「狙って掴む専用ポイント」。
bool Boss::IsArmorAttached(int plate) const {
    return plate >= 0 && plate < BossStage1::kArmorPlates && armorAttached_[plate] && !IsDefeated();
}
Vector2 Boss::GetArmorGripPoint(int plate) const {
    if (plate < 0 || plate >= BossStage1::kArmorPlates) return position_;
    return { position_.x + battleSettings_.armorHooks[plate].x,
        position_.y + battleSettings_.armorHooks[plate].y };
}
void Boss::SetArmorStrain(int plate, float progress) {
    // ボスの外装そのものがねじれて張力を表す。新しい描画物は作らない。
    armorStrainPlate_ = IsArmorAttached(plate) ? plate : -1;
    armorStrain_ = Clamp(progress, 0.0f, 1.0f);
}

// 【外装ネジ】四隅の1点を触手で掴めた瞬間にネジが外れる。
// ネジを1つ外してもコアは出ない。4点すべて外してから外装がペロっとめくれる。
bool Boss::TryPeelArmor(int plate) {
    if (!IsArmorAttached(plate) || state_ == BossState::ArmorCurl || state_ == BossState::ArmorStun) return false;
    armorAttached_[plate] = false;
    --armorPlates_;
    peeledPosition_[plate] = GetArmorGripPoint(plate);
    peeledFrames_[plate] = 20;
    hitstop_ = 3;
    ShakeCamera(2.5f, 4);
    if (armorPlates_ == 0) {
        // 全ネジ解除！専用の外装めくれ→スタンへ進む。ここでHPはまだ減らさない。
        ChangeState(BossState::ArmorCurl);
        hitstop_ = 0;
        PunchCamera(0.11f, 10);
    }
    return true;
}

bool Boss::ConsumeWakeBlast() {
    const bool result = wakeBlastPending_;
    wakeBlastPending_ = false;
    return result;
}

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

void Boss::ChooseAttack() {
    // 【重要】攻撃種類は従来の5種類のまま。
    // 毎回固定の並びにせず、プレイヤーの位置・動き・ワイヤー使用を採点する。
    // 直前と2回前の攻撃にはペナルティを付けて単調な繰り返しを防ぐ。
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

void Boss::ChangeState(BossState next) {
    state_ = next;
    stateFrame_ = 0;
    switch (state_) {
    case BossState::Idle: visual_.SetMode(BossVfxMode::Idle); break;
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
        visual_.SetMode(BossVfxMode::Recovery);
        ShakeCamera(6.0f, 8);
        break;
    case BossState::ArmorStun:
        visual_.SetMode(BossVfxMode::Recovery);
        break;
    case BossState::WakeBlast:
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

void Boss::NoteWireMotion(bool usingWire) {
    // 【安全な判定】予兆または突進中にワイヤーが接続/自動射出されたか記録。
    // 動かずに普通に避けた場合は専用の長い失速は発生しない。
    if (usingWire && ((state_ == BossState::Warning && nextAttack_ == BossAttack::Rush) ||
        state_ == BossState::Rush)) {
        wireUsedDuringRush_ = true;
    }
}

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
        if (pressFrame_ >= 34) { position_ = homePosition_;ChangeState(BossState::Idle); }
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

int Boss::TryHitPlayer(Vector2 playerPosition, float playerRadius, Vector2* hitSource) {
    if (IsDefeated()) return 0;
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
    switch (state_) {
    case BossState::Idle:
        // 【直した原因③】射撃で横へノックバックした後、そのまま位置がズレ続けていた。
        // 次の技までに必ず戻す。ボスが画面外へ逃げたりフックが射程外にならない。
        position_.x = Approach(position_.x, homePosition_.x, BossStage1::kShotReturnSpeed);
        position_.y += (homePosition_.y - position_.y) * 0.18f;
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
        position_.y = BossStage1::kRushGroundY;
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
            if (Abs(dx) < 9.0f) {
                position_ = homePosition_;
                ChangeState(rushDodgeStunPending_ ? BossState::Dizzy : BossState::Idle);
            }
        }
        break;
    case BossState::ArmorCurl:
        // 【めくれ】新しい粒子を出さず、外装BOXの回転で「ペロッ」を表現。
        if (stateFrame_ >= BossStage1::kArmorCurlFrames) ChangeState(BossState::ArmorStun);
        break;
    case BossState::ArmorStun:
        // 【叩き放題】長い反撃時間。HPが0になれば吹き飛ばしを出さず撃破。
        if (stateFrame_ >= BossStage1::kArmorStunFrames) ChangeState(BossState::WakeBlast);
        break;
    case BossState::WakeBlast:
        // 【復帰】ここに入ったフレームだけStageへ衝撃イベントを通知。
        // その後ボスは高い定位置に戻って元の5技AIを再開する。
        if (stateFrame_ >= BossStage1::kArmorWakeFrames) ChangeState(BossState::Idle);
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
    UpdateBullets();
    visual_.Update(position_);
}

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
    visual_.DrawBody(position_);
    // 【外装シート】256x256の本体1倍はそのまま。上に薄い外装BOXを1枚重ねる。
    // ネジ4つを外すと、板が下端を支点に回転してペロッとめくれる。
    if (armorPlates_ > 0 || state_ == BossState::ArmorCurl) {
        const float t = state_ == BossState::ArmorCurl ?
            Clamp(stateFrame_ / static_cast<float>(BossStage1::kArmorCurlFrames), 0.0f, 1.0f) : 0.0f;
        const float angle = t * t * 1.45f;
        const Vector2 shellCenter = { position_.x + std::sin(angle) * 70.0f,
            position_.y - 85.0f + std::cos(angle) * 84.0f };
        PortalDraw::RotatedBox(shellCenter, 226.0f, 212.0f, angle, 0xB7C4D1D9);
    }
    // 【外装と取っ掛かり】外装3枚はそれぞれ独立した目標。
    // 各板の上に小さな黄色の十字を描き、そこへ触手を当てて引き剥がす。
    // 表示1枚あたりBOX3個。板が減れば描画も減る。
    for (int i = 0;i < BossStage1::kArmorPlates;++i) {
        if (armorAttached_[i]) {
            const Vector2 p = GetArmorGripPoint(i);
            // 【フックを引いている最中】同じ板を回転/伸縮するだけで負荷ゼロ増加。
            const float tension = i == armorStrainPlate_ ? armorStrain_ : 0.0f;
            PortalDraw::RotatedBox({ p.x,p.y - 6.0f + tension * 4.0f },
                34.0f + tension * 12.0f, 16.0f - tension * 4.0f,
                tension * (i == 0 ? -0.24f : 0.24f), tension > 0.55f ? 0xFFF0B0FF : 0xA7B1C1FF);
            PortalDraw::Box(p, 13.0f, 3.0f, 0xFFE16AFF);
            PortalDraw::Box(p, 3.0f, 13.0f, 0xFFE16AFF);
        }
        else if (peeledFrames_[i] > 0) {
            const float t = 22.0f - peeledFrames_[i];
            // あえて物理計算だけで派手さを出す：一枚のBOXが回転しながら飛ぶ。
            const Vector2 p = { peeledPosition_[i].x + (i - 1) * t * 4.0f,
                peeledPosition_[i].y + t * 5.0f - t * t * 0.25f };
            PortalDraw::RotatedBox(p, 24.0f, 10.0f, t * 0.22f + (i - 1) * 0.3f, 0xE7E5DDFF);
        }
    }
    if (state_ == BossState::Dizzy || state_ == BossState::ArmorStun) {
        // 回避成功の合図：本体の上に黄色BOXを1枚。新規パーティクル不要。
        PortalDraw::Box({ position_.x,position_.y + 53.0f }, 49.0f, 7.0f, 0xFFEA73FF);
    }
    if (armorPlates_ == 0 && !IsDefeated() && state_ != BossState::ArmorCurl) {
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
