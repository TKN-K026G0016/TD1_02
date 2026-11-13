#include "PortalPlayer.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include <cmath>
using namespace PortalMath;

namespace {
    float MaxF(float a, float b) { return a > b ? a : b; }
    float Approach(float current, float target, float step) {
        if (current < target) return current + step < target ? current + step : target;
        return current - step > target ? current - step : target;
    }
}

void PortalPlayer::Initialize(Vector2 position) {
    position_ = position;
    velocity_ = {}; aim_ = { 1,0 }; facing_ = 1;
    cooldown_ = 0; grounded_ = (position.y <= PortalConfig::kFloor + 30.01f); selectedAnchor_ = -1; animationFrame_ = 0;
    coyoteFrames_ = 0; attackBufferFrames_ = 0; jumpBufferFrames_ = 0; attackIntentTarget_ = -1;
    stickLockTarget_ = -1; stickLockFrames_ = 0; stickLockConsumed_ = false; grappleSpinTick_ = 0;
    launchMomentumFrames_ = 0; lastLaunchedAnchor_ = -1; activeGripAnchor_ = -1; airTilt_ = 0.0f;
    launchVisualFrames_ = 0; regripLockFrames_ = 0; grappleForwardSign_ = 1.0f; grappleTravelDirection_ = { 1.0f,0.0f };
    grappleAge_ = 0; grappleLastDistance_ = 0.0f;
    armorPeelFrames_ = 0; armorPeelTarget_ = -1; armorPeelRequest_ = -1; armorTetherSlot_ = -1;
    armorPullFailFrames_ = 0; normalAttackFrames_ = 0; normalAttackDirection_ = { 1.0f,0.0f };
    spinFrames_ = 0; hurtFrames_ = 0; hitConfirmFrames_ = 0; hp_ = kMaxHp;
    jumpCutArmed_ = false;
    castPulse_ = landingBounce_ = gripStretch_ = bodyLean_ = 0.0f;
    for (auto& gate : gates_) gate = PortalTentacle{};
}

float PortalPlayer::GetArmorPullProgress() const {
    if (armorPeelFrames_ <= 0) return 0.0f;
    return Clamp(armorPeelFrames_ / static_cast<float>(PortalConfig::kArmorPullMaxFrames), 0.0f, 1.0f);
}

int PortalPlayer::ConsumeArmorPeelRequest() {
    const int result = armorPeelRequest_;
    armorPeelRequest_ = -1;
    return result;
}

bool PortalPlayer::IsGripping() const {
    for (const auto& gate : gates_) if (gate.IsGripping()) return true;
    return false;
}
void PortalPlayer::ReleaseAll() {
    for (auto& gate : gates_) gate.Release();
}

// ================================================================
// 【ロック対象の選択】照準方向から一定の角度・横ずれに収まる杭だけ選ぶ。
// 右スティックを短く倒すと攻撃、同じロックへ9F保持するとグラップル。
// 見た目もロック中のみ緑/白の表示になるので、Jを押す前に結果が分かる。
// A/D移動だけならUpdate側でFindAnchorを呼ばない（右スティックで狙う）。
// ================================================================
int PortalPlayer::FindAnchor(Vector2 direction, const Vector2* anchors, int count, int fixedCount) const {
    if (anchors == nullptr || count <= 0) return -1;
    const Vector2 aimDir = Unit(direction);
    int best = -1;
    float bestScore = 100000.0f;
    for (int i = 0;i < count;++i) {
        if (regripLockFrames_ > 0 && i == lastLaunchedAnchor_) continue;
        const Vector2 delta = Sub(anchors[i], position_);
        const float distance = Length(delta);
        if (distance<30.0f || distance>PortalConfig::kReach) continue;
        const float forward = PortalMath::Dot(delta, aimDir);
        if (forward <= 0.0f) continue;
        const float alignment = forward / distance;
        // 横ずれ距離：方向ベクトルに垂直な方向への投影量。
        const float sideways = std::fabs(delta.x * aimDir.y - delta.y * aimDir.x);
        const float tolerance = Clamp(distance * PortalConfig::kAnchorLockSideRatio,
            PortalConfig::kAnchorLockSideMin, PortalConfig::kAnchorLockSideMax);
        // ボス外装のフックは、固定杭より意図的に照準しないとロックしない。
        // 通常攻撃がボスの黄色い3点へ勝手に吸われるのを防ぐ。
        const bool isArmor = i >= fixedCount;
        const float requiredDot = isArmor ? PortalConfig::kArmorAimDot : PortalConfig::kAnchorAimDot;
        // ボス外装だけは少し広い補正。固定杭の誤グリップ判定は据え置く。
        const float allowedSide = isArmor ? PortalConfig::kArmorAimSideMax : tolerance;
        if (alignment<requiredDot || sideways>allowedSide) continue;
        // ピッタリ狙えたポイントを優先。同一直線上なら近い方を選ぶ。
        // 【四隅ネジ】遠いネジをぴったり狙ったのに近い別のネジに吸われないようにする。
        // ネジは角度を優先、移動用杭は距離も考慮する。
        float score = isArmor ? (sideways * 7.0f + distance * 0.02f)
            : (sideways * 2.1f + distance * 0.16f);
        if (i == selectedAnchor_) score -= 12.0f; // ロック対象が毎フレーム振れないように
        if (score < bestScore) { bestScore = score;best = i; }
    }
    return best;
}

// 空中で右スティックを倒すと回転攻撃。キーボードのJも従来通り。
// 攻撃の当たり判定はHitTestEnemy()、見た目はDraw()で管理する。
// 【ヒット確認】Boss::TakeDamageがtrueのときだけ呼ばれる。
// 手触りは速度を急停止させるのではなく、既存BOXの色変化＋軽い反動＋
// 攻撃用触手のポーズ保持で出す。ワイヤーは一切止めない。
void PortalPlayer::OnAttackHit(PortalAttackKind kind) {
    if (kind == PortalAttackKind::None) return;
    hitConfirmFrames_ = PortalConfig::kHitConfirmFrames;
    // ここでパーティクルも追加BOXも生成しない。既存のプレイヤー色だけを変更。
    if (kind == PortalAttackKind::Normal) {
        for (auto& gate : gates_) if (gate.IsActive() && !gate.IsGrapple())
            gate.HoldAttackPose(PortalConfig::kHitPoseHoldFrames);
        // 【反動】射程を短く感じないよう0.8px/Fだけ。グラップル/射出を止めない。
        if (!IsGripping()) velocity_.x -= aim_.x * PortalConfig::kNormalHitRecoil;
    }
    else if (kind == PortalAttackKind::Spin) {
        // 大技は少し上へ押し返される。上限を小さくして連打浮遊を防ぐ。
        if (!IsGripping()) velocity_.y = Clamp(velocity_.y + PortalConfig::kSpinHitLift, -18.0f, 20.0f);
    }
}

void PortalPlayer::OnArmorPeeled() {
    // 【外装破壊】ボスの破片はBoss側で描画。Player側は既存BOXの発色だけを変える。
    hitConfirmFrames_ = PortalConfig::kArmorPeelConfirmFrames;
}

void PortalPlayer::StartSpin() {
    spinFrames_ = PortalConfig::kSpinFrames;
    // 落下しているときだけ少しブレーキ。完全な空中停止にはしない。
    if (velocity_.y < -6.0f) velocity_.y = -6.0f;
    velocity_.y += PortalConfig::kSpinLift;
    castPulse_ = 1.0f;
}

void PortalPlayer::Update(const PortalCommands& input, const Vector2* anchors, int anchorCount, int fixedCount) {
    // ================================================================
    // 【旧版互換】現在はネジ接続時に即座に取り外すので通常は通らない。
    // 【外装剥がしの詰み防止】
    // 以前は「26～39Fで離す」「ボスが510+80px離れたら失敗」の2条件があり、
    // 素早く移動するボスでは、Jを押しても一度も外装を剥がせなかった。
    // 現在は、装甲への接続が成功したらボスに触手が追従し続ける。
    // 22F以上引いてからJを離すと剥がれる。緑の間はずっと成功受付。
    // ただし早離しは失敗するので、狙って引っ張る操作自体は維持する。
    // ================================================================
    if (armorPeelFrames_ > 0) {
        const int targetIndex = fixedCount + armorPeelTarget_;
        const bool valid = anchors && armorTetherSlot_ >= 0 && armorPeelTarget_ >= 0 &&
            targetIndex >= fixedCount && targetIndex < anchorCount &&
            anchors[targetIndex].x < 10000.0f; // 外装がまだ存在するかだけ確認
        if (valid) {
            // ボスが左右に動いても触手先端が外装についていく。
            gates_[armorTetherSlot_].SetArmorTarget(anchors[targetIndex]);
        }
        const bool released = !input.attackHeld;
        const bool ready = armorPeelFrames_ >= PortalConfig::kArmorPullMinFrames;
        if (!valid || released) {
            if (valid && released && ready) armorPeelRequest_ = armorPeelTarget_;
            else armorPullFailFrames_ = 12;
            gates_[armorTetherSlot_ >= 0 ? armorTetherSlot_ : 0].Release();
            armorPeelFrames_ = 0;
            armorPeelTarget_ = -1;
            armorTetherSlot_ = -1;
            // 長押し中の連続攻撃パルスが残っても、剥がした直後に誤攻撃しない。
            attackBufferFrames_ = 0;
            attackIntentTarget_ = -1;
            if (valid && released && ready) castPulse_ = 1.0f;
            else cooldown_ = PortalConfig::kArmorPullFailCooldown;
        }
        else if (armorPeelFrames_ < PortalConfig::kArmorPullMaxFrames) {
            ++armorPeelFrames_;
        }
    }
    ++animationFrame_;
    castPulse_ *= 0.78f;
    landingBounce_ *= 0.77f;
    if (cooldown_ > 0) --cooldown_;
    if (hurtFrames_ > 0) --hurtFrames_;
    if (hitConfirmFrames_ > 0) --hitConfirmFrames_;
    if (armorPullFailFrames_ > 0) --armorPullFailFrames_;
    if (normalAttackFrames_ > 0) --normalAttackFrames_;
    if (spinFrames_ > 0) --spinFrames_;
    if (coyoteFrames_ > 0) --coyoteFrames_;
    if (attackBufferFrames_ > 0) --attackBufferFrames_;
    if (jumpBufferFrames_ > 0) --jumpBufferFrames_;
    if (launchMomentumFrames_ > 0) --launchMomentumFrames_;
    if (launchVisualFrames_ > 0) --launchVisualFrames_;
    if (regripLockFrames_ > 0) --regripLockFrames_;
    if (grounded_) coyoteFrames_ = PortalConfig::kCoyoteFrames;
    if (input.jump) jumpBufferFrames_ = PortalConfig::kJumpBufferFrames;

    if (std::fabs(input.move) > 0.1f) facing_ = input.move > 0 ? 1 : -1;
    aim_ = Length(input.aim) > 0.05f ? Unit(input.aim) : Vector2{ static_cast<float>(facing_),0 };
    // 【攻撃優先】右スティックの最初の入力は、ロックがあっても必ず触手攻撃。
    // これにより「全部グラップルになって攻撃できない」問題を防ぐ。
    selectedAnchor_ = input.aimExplicit ? FindAnchor(aim_, anchors, anchorCount, fixedCount) : -1;
    if (input.attack) {
        attackBufferFrames_ = PortalConfig::kAttackBufferFrames;
        attackIntentTarget_ = input.stickAction ? -1 : selectedAnchor_;
    }

    // ================================================================
    // 【操作の役割分担】パッドは左スティック左右移動・上ジャンプ、
    // 右スティックで8方向攻撃 / ロック保持でワイヤー接続。
    // キーボードは従来通りSPACEとJ。解除キーは不要。
    // ================================================================
    // 格納中の触手はグラップル扱いにしない（射出後にJが無効になる不具合を防ぐ）。
    bool grappleRunning = IsGripping();

    // SPACEを押した時にジャンプ可能なら実行（コヨーテタイム＋先行入力）。
    // グラップル中のSPACEは任意の早期飛び出し。押さなくても自動で射出する。
    if (jumpBufferFrames_ > 0 && grappleRunning) {
        lastLaunchedAnchor_ = activeGripAnchor_;
        activeGripAnchor_ = -1;
        ReleaseAll();
        grappleRunning = false;
        // 【SPACE早期離脱】ワイヤーで得た横速度を維持。A/Dでは射出方向を決めない。
        // 上方向だけジャンプ速度を与えるので、移動方向が突然反転しない。
        // x方向は既存速度のまま。ジャンプで突然逆へ飛ぶのを防ぐ。
        velocity_.x = Clamp(velocity_.x, -PortalConfig::kLaunchAirMaxX, PortalConfig::kLaunchAirMaxX);
        velocity_.y = MaxF(velocity_.y, PortalConfig::kJumpSpeed * 0.90f);
        jumpCutArmed_ = true;
        launchMomentumFrames_ = PortalConfig::kLaunchMomentumFrames;
        launchVisualFrames_ = PortalConfig::kLaunchVisualFrames;
        regripLockFrames_ = PortalConfig::kLaunchRegripLockFrames;
        jumpBufferFrames_ = 0;
        attackBufferFrames_ = 0;
        grounded_ = false;
        coyoteFrames_ = 0;
    }
    else if (jumpBufferFrames_ > 0 && (grounded_ || coyoteFrames_ > 0)) {
        velocity_.y = PortalConfig::kJumpSpeed;
        grounded_ = false;
        coyoteFrames_ = 0;
        jumpCutArmed_ = true;
        jumpBufferFrames_ = 0;
        landingBounce_ = 0.0f;
    }
    // ================================================================
    // 【ロック保持でグラップル】短く倒しただけでは絶対にグラップルしない。
    // 一度攻撃を出したあと、ロックの方向を9F保つと自動接続。
    // ボスの外装も同様。接続後は右スティックを保持して引っ張り、
    // ゲージが緑になったらニュートラルへ戻すと外装が剥がれる。
    // ================================================================
    if (!input.stickAction) {
        stickLockTarget_ = -1;
        stickLockFrames_ = 0;
        stickLockConsumed_ = false;
    }
    else if (!grappleRunning && armorPeelFrames_ == 0 && !stickLockConsumed_ &&
        selectedAnchor_ >= 0) {
        if (stickLockTarget_ == selectedAnchor_) ++stickLockFrames_;
        else { stickLockTarget_ = selectedAnchor_; stickLockFrames_ = 1; }
        if (stickLockFrames_ >= PortalConfig::kStickGrappleHoldFrames) {
            // 右スティックにボタンはないため、一定時間の方向保持を接続指示に変換。
            attackIntentTarget_ = selectedAnchor_;
            attackBufferFrames_ = PortalConfig::kAttackBufferFrames;
            stickLockConsumed_ = true;
            cooldown_ = 0;
            spinFrames_ = 0; // 空中スピン中でも確定したグラップルを優先する
            normalAttackFrames_ = 0;
        }
    }
    else if (selectedAnchor_ < 0) {
        stickLockTarget_ = -1;
        stickLockFrames_ = 0;
    }

    // グラップル中は自動で軽い回転攻撃。スティックの追加入力は不要。
    // 既存の太いワイヤー判定に近距離の円判定を追加するだけ（描画増ゼロ）。
    grappleSpinTick_ = grappleRunning ? grappleSpinTick_ + 1 : 0;
    // 右スティックの連打パルスで重い空中スピンへ切り替わらないようにする。
    if (grappleRunning && input.stickAction) attackBufferFrames_ = 0;
    else if (grappleRunning && attackBufferFrames_ > 0) attackIntentTarget_ = -1;
    if (attackBufferFrames_ > 0 && cooldown_ == 0 && spinFrames_ == 0 && armorPeelFrames_ == 0) {
        // グリップの判定はプレイヤーの狙いに対して自動補助する。
        const bool canGrip = !grappleRunning && attackIntentTarget_ >= 0 && anchors != nullptr &&
            attackIntentTarget_ < anchorCount &&
            Length(Sub(anchors[attackIntentTarget_], position_)) <= PortalConfig::kReach;
        const bool willSpin = !canGrip && !grounded_ && coyoteFrames_ == 0;
        // ボスから離れていても、Jを押した時点で攻撃が開始する。
        // 触手の空きスロット不足は「攻撃不可」の理由にしない。
        const bool normalAttack = !canGrip && !willSpin;
        if (normalAttack) {
            normalAttackFrames_ = PortalConfig::kNormalAttackFrames;
            normalAttackDirection_ = aim_; // 入力した瞬間の方向で攻撃する
            cooldown_ = PortalConfig::kAttackCooldownFrames;
            attackBufferFrames_ = 0; attackIntentTarget_ = -1;
            castPulse_ = 1.0f;
        }
        if (normalAttack) {
            // 判定は独立して開始済み。以下の触手描画は空きがあれば使う。
            int freeSlot = -1;
            for (int i = 0;i < PortalConfig::kGateCount;++i) {
                if (!gates_[i].IsActive() || gates_[i].IsRetracting()) { freeSlot = i;break; }
            }
            if (freeSlot < 0) {
                // 描画枠がすべて使用中なら、非グリップの古い触手を再利用。
                // 固定杭へのグリップと装甲フックは絶対に切断しない。
                for (int i = 0;i < PortalConfig::kGateCount;++i)
                    if (!gates_[i].IsGrapple() && !gates_[i].IsArmorTether()) { freeSlot = i;break; }
            }
            if (freeSlot >= 0) {
                const Vector2 origin = Sub(position_, Mul(aim_, 66.0f));
                gates_[freeSlot].Start(origin, aim_, nullptr);
            }
        }
        else if (willSpin) {
            StartSpin();
            cooldown_ = PortalConfig::kSpinFrames + 5;
            attackBufferFrames_ = 0;attackIntentTarget_ = -1;
        }
        else {
            int slot = -1;
            for (int i = 0;i < PortalConfig::kGateCount;++i) {
                if (!gates_[i].IsActive() || gates_[i].IsRetracting()) { slot = i;break; }
            }
            if (slot >= 0) {
                const Vector2* anchor = canGrip ? &anchors[attackIntentTarget_] : nullptr;
                const Vector2 shotDirection = anchor ? Unit(Sub(*anchor, position_)) : aim_;
                const Vector2 origin = Sub(position_, Mul(shotDirection, 66.0f));
                // 【J自動振り分け】同じボタンで3種類の行動。
                // 明確に外装ポイントを狙う→外装剥がし（プレイヤーは移動しない）。
                // 明確に固定杭を狙う→ワイヤー移動。それ以外→通常/空中攻撃。
                const bool armorTarget = canGrip && attackIntentTarget_ >= fixedCount;
                if (armorTarget) {
                    // 【新しい四隅ネジ】狙ってフックに届いた瞬間、1本のネジが外れる。
                    // J長押し・緑ゲージは廃止。触手は短い攻撃アニメーションだけ残す。
                    gates_[slot].StartArmorTether(origin, *anchor);
                    gates_[slot].Release();
                    armorPeelRequest_ = attackIntentTarget_ - fixedCount;
                    armorPeelFrames_ = 0;
                    armorPeelTarget_ = -1;
                    armorTetherSlot_ = -1;
                    // 引っかけた勢いをプレイヤーへ返さない。
                    // 攻撃アニメーションだけを継続し、移動速度には触れない。
                }
                else {
                    gates_[slot].Start(origin, shotDirection, anchor);
                }
                if (anchor && !armorTarget) {
                    // 【射出方向の固定】Jを押してつないだ瞬間の杭方向を保存。
                    // グラップル中のA/D入力で、射出方向を変更する処理は一切行わない。
                    grappleTravelDirection_ = shotDirection;
                    grappleForwardSign_ = std::fabs(shotDirection.x) > 0.12f
                        ? (shotDirection.x > 0.0f ? 1.0f : -1.0f)
                        : static_cast<float>(facing_);
                    activeGripAnchor_ = attackIntentTarget_;
                    grappleAge_ = 0;
                    grappleLastDistance_ = Length(Sub(*anchor, position_));
                }
                castPulse_ = 1.0f;
                cooldown_ = PortalConfig::kAttackCooldownFrames;
                attackBufferFrames_ = 0;attackIntentTarget_ = -1;
                if (!anchor) {
                    // 杭がないなら8方向攻撃。上+Jでもジャンプせず攻撃する。
                    if (aim_.y < -0.5f) {
                        velocity_.y = MaxF(velocity_.y, PortalConfig::kDownAttackBoost);
                        grounded_ = false;
                    }
                    else {
                        velocity_ = Add(velocity_, Mul(aim_, -2.2f));
                    }
                }
            }
        }
    }

    // ジャンプはSPACEの短押し/長押しで高さを調整する。
    if (jumpCutArmed_ && !input.jumpHeld && velocity_.y > 0.0f) {
        velocity_.y *= PortalConfig::kJumpReleaseMultiplier;
        jumpCutArmed_ = false;
    }
    if (velocity_.y <= 0.0f) jumpCutArmed_ = false;

    for (auto& gate : gates_) gate.Update();
    bool gripping = IsGripping();
    bool justLaunched = false;
    gripStretch_ += ((gripping ? 1.0f : 0.0f) - gripStretch_) * 0.24f;

    if (gripping) {
        // ================================================================
        // 【シームレスグラップル】毎フレーム「杭へ引く」だけを行い、
        // 杭でぴったり停止したり、杭を越えて逆向きに引き戻したりしない。
        // A/Dは巻き取り方向・射出方向に一切使わない。
        // ================================================================
        for (auto& gate : gates_) {
            if (!gate.IsGripping()) continue;
            ++grappleAge_;
            const Vector2 toAnchor = Sub(gate.GetAnchor(), position_);
            const float distance = Length(toAnchor);
            const Vector2 direction = distance > 0.001f ? Mul(toAnchor, 1.0f / distance) : grappleTravelDirection_;

            // 近くで力を弱めない。引き寄せの速度は連続して変化する。
            Vector2 nextVelocity = Add(Mul(velocity_, PortalConfig::kPullMomentumKeep),
                Mul(direction, PortalConfig::kPullAcceleration));
            nextVelocity.y -= PortalConfig::kGravity * 0.16f;
            nextVelocity = Limit(nextVelocity, PortalConfig::kMaxPullSpeed);

            // 【停止/引っかかり防止】次の1Fで杭にどれだけ近づくかを予測。
            // 速度を反転してから解除するのでは遅いので、少し手前でロープを外す。
            const float approach = PortalMath::Dot(nextVelocity, direction);
            const float forward = PortalMath::Dot(toAnchor, nextVelocity);
            const float releaseRadius = PortalConfig::kAutoLaunchDistance +
                Clamp(approach * PortalConfig::kAutoLaunchLeadFrames,
                    0.0f, PortalConfig::kAutoLaunchMaxLead);
            // 点と1フレームの移動線分の最短距離（跨ぐケースも検知する）。
            const float sweep = Clamp(forward / (PortalMath::Dot(nextVelocity, nextVelocity) + 0.0001f), 0.0f, 1.0f);
            const float sweptDistance = Length(Sub(toAnchor, Mul(nextVelocity, sweep)));
            const bool passedNearest = grappleAge_ > 2 &&
                distance > grappleLastDistance_ + 0.5f;
            const bool ready = grappleAge_ >= PortalConfig::kGrappleMinFrames;
            const bool nearPoint = (distance <= releaseRadius && approach > PortalConfig::kGrappleReleaseForwardDot) ||
                (forward > 0.0f && sweptDistance <= PortalConfig::kAutoLaunchDistance);
            const bool arriving = ready && (nearPoint || passedNearest);
            grappleLastDistance_ = distance;

            if (arriving) {
                // 【最重要】射出方向を新たに設定しない。
                // 接近直前の速度をそのまま採用し、速さを1px/Fだけ足す。
                // 旧実装の「固定x速度・上向き最低速度・接続方向への加算」を廃止。
                Vector2 exitVelocity = nextVelocity;
                const float exitSpeed = Length(exitVelocity);
                if (exitSpeed > 0.001f) {
                    exitVelocity = Mul(exitVelocity,
                        1.0f + PortalConfig::kGrappleExitSpeedBonus / exitSpeed);
                }
                if (exitSpeed < PortalConfig::kGrappleStallSpeed) {
                    // 予想外に低速になった場合のみ、小さい補助を同方向へ。
                    exitVelocity = Add(exitVelocity, Mul(grappleTravelDirection_,
                        PortalConfig::kGrappleStallImpulse));
                }
                velocity_ = Limit(exitVelocity, PortalConfig::kGrappleExitMaxSpeed);
                lastLaunchedAnchor_ = activeGripAnchor_;
                activeGripAnchor_ = -1;
                grappleAge_ = 0;
                ReleaseAll();
                attackBufferFrames_ = 0;
                launchMomentumFrames_ = PortalConfig::kLaunchMomentumFrames;
                launchVisualFrames_ = PortalConfig::kLaunchVisualFrames;
                regripLockFrames_ = PortalConfig::kLaunchRegripLockFrames;
                jumpCutArmed_ = false;
                castPulse_ = 1.0f;
                gripping = false;
                justLaunched = true;
            }
            else {
                velocity_ = nextVelocity;
            }
            break;
        }
    }
    if (!gripping && !justLaunched) {
        const bool moving = std::fabs(input.move) > 0.08f;
        if (!grounded_ && launchMomentumFrames_ > 0) {
            // 【大事】従来は射出速度25→通常速度6.4へすぐ減速していた。
            // この期間は横速度を保持し、A/Dで空中軌道を調整できる。
            if (moving) {
                // 【方向転換】逆入力は先に強いブレーキ。通常の加速より早く切り返せる。
                // 高速射出後の長い逆走を防ぐ。速度の上限は従来通り制限。
                const float turnAcceleration = velocity_.x * input.move < 0.0f
                    ? PortalConfig::kLaunchAirReverseBrake : PortalConfig::kLaunchAirSteer;
                velocity_.x = Clamp(velocity_.x + input.move * turnAcceleration,
                    -PortalConfig::kLaunchAirMaxX, PortalConfig::kLaunchAirMaxX);
            }
            else {
                velocity_.x *= PortalConfig::kLaunchMomentumDrag;
            }
        }
        else {
            const float desired = input.move * PortalConfig::kMoveSpeed;
            const float accel = grounded_ ?
                (moving ? PortalConfig::kGroundAcceleration : PortalConfig::kGroundBrake) :
                (moving ? PortalConfig::kAirAcceleration : PortalConfig::kAirBrake);
            velocity_.x = Approach(velocity_.x, desired, accel);
        }
        velocity_.y = Clamp(velocity_.y - PortalConfig::kGravity, -18.0f, 27.0f);
    }

    const bool wasGrounded = grounded_;
    const float impactVelocity = velocity_.y;
    position_ = Add(position_, velocity_);
    grounded_ = false;
    if (position_.y < PortalConfig::kFloor + 30.0f) {
        position_.y = PortalConfig::kFloor + 30.0f;
        velocity_.y = 0.0f;
        grounded_ = true;
    }
    if (position_.y > PortalConfig::kWorldCeiling) { position_.y = PortalConfig::kWorldCeiling; if (velocity_.y > 0) velocity_.y = 0.0f; }
    if (position_.x < 18.0f) { position_.x = 18.0f; velocity_.x = 0.0f; }
    if (position_.x > 1262.0f) { position_.x = 1262.0f; velocity_.x = 0.0f; }
    if (!wasGrounded && grounded_ && impactVelocity < -4.0f)
        landingBounce_ = Clamp(-impactVelocity / 18.0f, 0.0f, 1.0f);
    if (grounded_) launchMomentumFrames_ = 0;
    bodyLean_ += (Clamp(velocity_.x * 0.9f, -9.0f, 9.0f) - bodyLean_) * 0.2f;
    // 【低コスト演出】BOXの角度だけ変えて飛行方向を表す（追加描画ゼロ）。
    const float desiredTilt = grounded_ ? 0.0f : Clamp(-velocity_.x * 0.013f, -0.32f, 0.32f);
    airTilt_ += (desiredTilt - airTilt_) * 0.16f;
    for (auto& gate : gates_) gate.FollowOwner(position_);
}

PortalAttackKind PortalPlayer::HitTestEnemy(Vector2 target, float radius) const {
    // 一度の空中回転中は半径で当たり判定。敵に近づくほどヒットしやすい。
    const int elapsed = PortalConfig::kSpinFrames - spinFrames_;
    if (spinFrames_ > 0 && elapsed >= PortalConfig::kSpinActiveStart &&
        elapsed <= PortalConfig::kSpinActiveEnd) {
        const Vector2 d = Sub(target, position_);
        const float hitRadius = radius + PortalConfig::kSpinRadius;
        if (PortalMath::Dot(d, d) <= hitRadius * hitRadius) return PortalAttackKind::Spin;
    }
    // 【通常攻撃の保証】触手表示のスロットが埋まっていてもJ入力から判定する。
    // ボスが大きくなっても攻撃しやすいよう、太い線分×円で判定。
    const int attackElapsed = PortalConfig::kNormalAttackFrames - normalAttackFrames_;
    if (normalAttackFrames_ > 0 &&
        attackElapsed >= PortalConfig::kNormalAttackActiveStart &&
        attackElapsed <= PortalConfig::kNormalAttackActiveEnd) {
        const Vector2 segment = Mul(normalAttackDirection_, PortalConfig::kAttackReach);
        const Vector2 a = position_;
        const Vector2 delta = Sub(target, a);
        const float t = Clamp(PortalMath::Dot(delta, segment) / (PortalMath::Dot(segment, segment) + 0.0001f), 0.0f, 1.0f);
        // Windowsのヘッダーではnearがマクロ定義されることがある。
        // そのため変数名をclosestOffsetにする（攻撃判定の計算は同じ）。
        const Vector2 closestOffset = Sub(delta, Mul(segment, t));
        const float r = radius + PortalConfig::kNormalAttackThickness;
        if (PortalMath::Dot(closestOffset, closestOffset) <= r * r) return PortalAttackKind::Normal;
    }
    // 【ボタン不要の軽い回転攻撃】グラップル中は近くの敵へ触手が当たる。
    // 強い空中スピンとは違い、低ダメージのGrapple判定。ボスの無敵時間も適用。
    if (IsGripping()) {
        const Vector2 d = Sub(target, position_);
        const float hitRadius = radius + PortalConfig::kGrappleSpinRadius;
        if (PortalMath::Dot(d, d) <= hitRadius * hitRadius) return PortalAttackKind::Grapple;
    }
    // グラップル中は「移動用の綱＝攻撃中」の判定にする。
    for (const auto& gate : gates_) {
        if (gate.IsGrapple() && gate.HitsCircle(target, radius)) return PortalAttackKind::Grapple;
    }
    for (const auto& gate : gates_) {
        if (!gate.IsGrapple() && !gate.IsArmorTether() && normalAttackFrames_ > 0 &&
            gate.HitsCircle(target, radius)) return PortalAttackKind::Normal;
    }
    return PortalAttackKind::None;
}

bool PortalPlayer::TakeDamage(int damage, Vector2 source) {
    if (damage <= 0 || hurtFrames_ > 0 || hp_ <= 0) return false;
    hp_ -= damage;
    if (hp_ < 0) hp_ = 0;
    hurtFrames_ = 48;  // 同じ弾が連続ヒットしないよう無敵時間を設ける。
    velocity_.x = (position_.x < source.x) ? -7.0f : 7.0f;
    velocity_.y = 7.0f;
    ReleaseAll();
    armorPeelFrames_ = 0; armorPeelTarget_ = -1; armorTetherSlot_ = -1;
    armorPeelRequest_ = -1;
    normalAttackFrames_ = 0;
    return true;
}

void PortalPlayer::Draw() const {
    for (const auto& gate : gates_) gate.Draw(position_);
    const float sway = std::sin(animationFrame_ * 0.19f);
    const float runCycle = std::sin(animationFrame_ * 0.28f);
    const float speed = std::fabs(velocity_.x);
    const float bob = grounded_ ? std::fabs(runCycle) * Clamp(speed * 0.33f, 0.0f, 3.0f) : sway * 1.8f;
    const Vector2 drawCenter = Add(position_, { bodyLean_ * 0.55f,bob + landingBounce_ * 3.0f });

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

// 【起き上がり衝撃】追加の描画なし。プレイヤーを床側へ落とし、横にも押す。
void PortalPlayer::ApplyWakeBlast(Vector2 bossPosition, float pushX, float downSpeed) {
    ReleaseAll();
    activeGripAnchor_ = -1;
    launchMomentumFrames_ = 0;
    const float sign = position_.x < bossPosition.x ? -1.0f : 1.0f;
    velocity_.x = sign * pushX;
    velocity_.y = -downSpeed;
    grounded_ = false;
    coyoteFrames_ = 0;
    hitConfirmFrames_ = 12;
}
