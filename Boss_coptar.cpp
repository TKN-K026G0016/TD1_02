#include "AssetEditor.h"
// 【このファイルの役割】Boss_coptar.cpp — コプターの行動・外装・戦闘AI
// 5種類の攻撃、4ネジ解除、墜落、連打スタン、第2段階、被弾、撃破を管理する。

#include "Boss_coptar.h"
#include "Camera.h"
#include "PortalDraw.h"
#include "PortalConfig.h"
#include <cmath>
#include <cstring>
#include <Novice.h>

namespace {
    constexpr float kTopRadius = 128.0f;
    // PortalArena::DrawWorld で描く床の左右端。
    constexpr float kTopFloorLeft = 5.0f;
    constexpr float kTopFloorRight = 1275.0f;
    constexpr float kTopVisibleHalfWidth = 78.0f; // gyro.png の不透明部分は X=50〜205

    float TopWidthFactor(int frame) {
        return 0.91f + 0.09f * std::cos(frame * 0.28f);
    }

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
    // コマ・ヨーヨーは既存の専用PNGを原寸表示する。
    // 画像はキャッシュし、毎フレームLoadTextureを呼ばない。
    void DrawToySprite(const char* path, Vector2 center, int pixelSize, float angle, int& handle) {
        if (handle == -2) handle = Novice::LoadTexture(path);
        if (handle < 0) {
            PortalDraw::RotatedBox(center, static_cast<float>(pixelSize) * 0.5f,
                static_cast<float>(pixelSize) * 0.5f, angle, 0xC9C4DAFF);
            return;
        }
        const Vector2i screen = PortalDraw::ToScreen(center);
        const float scale = GetCameraZoom(); // 1.0倍が基準。256px/128pxのまま描画。
        const int size = static_cast<int>(pixelSize * scale);
        Novice::DrawSprite(screen.x - size / 2, screen.y - size / 2,
            handle, scale, scale, -angle, 0xFFFFFFFF);
    }
    void DrawSpinningTop(Vector2 center, int frame) {
        constexpr int kSize = 256;
        static const int handle = Novice::LoadTexture("./resources/texture/boss/form1/gyro.png");
        // 先端を下に向けたまま、横幅の変化で軸回転を表現する。
        const float widthFactor = TopWidthFactor(frame);
        if (handle < 0) {
            PortalDraw::Box(center, kSize * widthFactor * 0.5f, kSize * 0.5f, 0xC9C4DAFF);
            return;
        }
        const float scale = GetCameraZoom();
        const float scaleX = scale * widthFactor;
        const Vector2i screen = PortalDraw::ToScreen(center);
        const int width = static_cast<int>(kSize * scaleX);
        const int height = static_cast<int>(kSize * scale);
        Novice::DrawSprite(screen.x - width / 2, screen.y - height / 2,
            handle, scaleX, scale, 0.0f, 0xFFFFFFFF);
    }
}

// オブジェクトを使い始めるときに値を初期状態へ戻す。リトライ時にも重要。
void Boss::Initialize(Vector2 spawnPos) {
    position_ = flightDestination_ = spawnPos;
    flightVelocity_ = {}; flightTimer_ = 0; flightDirection_ = 1;
    targetPosition_ = nullptr; // Stage::InitStageがあとからSetTargetで接続する
    hp_ = GetMaxHp();
    coasterStage_ = false; armorRebuilt_ = rebuildOnWake_ = false; mashCount_ = 0; armorFallVelocity_ = 0.0f;
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
    if (coasterStage_) {
        // Stage 2: horizontal charge, then vertical charge.
        // Stage 1's attack selection is not modified.
        nextAttack_ = (attackCount_ % 2 == 0) ? BossAttack::Rush : BossAttack::Press;
        ++attackCount_;
        return;
    }
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
        // 地上にいるプレイヤー相手でもヨーヨーが出なくならないようにする。
        // 長期間使っていない技ほど少し優先する（5種類すべてが出る）。
        for (int i = 0; i < 5; ++i) {
            score[i] += Clamp(static_cast<float>(attackStaleness_[i]) * 11.0f, 0.0f, 70.0f);
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
        pressTargetX_ = targetPosition_ ? targetPosition_->x : position_.x;
        // 【床上の突進】プレイヤーが空中にいても、高さを追尾しない。
        // 予兆から本番まで床スレスレの一定高度。ジャンプ/ワイヤーで飛び越せる。
        rushTargetY_ = BossStage1::kRushGroundY;
        if (nextAttack_ == BossAttack::Rush) {
            wireUsedDuringRush_ = false;
            rushHitPlayer_ = false;
            rushDodgeStunPending_ = false;
            // 右側にいるプレイヤーには左端から、左側にいるプレイヤーには右端から突進。
            // 予兆開始時に方向を固定し、途中でプレイヤーが動いても反転させない。
            const float middleX = (BossStage1::kArenaLeft + BossStage1::kArenaRight) * 0.5f;
            rushDirection_ = (!targetPosition_ || targetPosition_->x >= middleX) ? 1 : -1;
            const float halfWidth = BossStage1::kBossWidth * 0.5f;
            rushTargetX_ = (rushDirection_ > 0)
                ? BossStage1::kArenaRight - halfWidth
                : BossStage1::kArenaLeft + halfWidth;
        }
        visual_.SetMode(BossVfxMode::Warning);
        break;
    case BossState::Rush:
        // 予兆中に端へ移動済み。ここでは位置を瞬間移動させない。
        rushStart_ = position_;
        rushVelocity_ = 0.0f;
        visual_.SetMode(BossVfxMode::Rush);
        break;
    case BossState::Recovery: visual_.SetMode(BossVfxMode::Recovery); break;
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
        toyPosition_ = { position_.x + toyDirection_ * 47.0f,position_.y - 20.0f };
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

void Boss::StartCoasterStage() {
    coasterStage_ = true;
    armorPlates_ = 0;
    for (int i = 1; i < BossStage1::kArmorPlates; ++i) armorAttached_[i] = false;
    for (int i = 0; i < kAccessoryGripCount - 1; ++i) accessoryAttached_[i] = false;
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
    // Stage 2 vertical charge: prepare high above the target, then descend
    // rapidly in a straight line. Stage 1 retains its original press motion.
    if (coasterStage_) {
        ++pressFrame_;
        switch (pressPhase_) {
        case PressPhase::Align:
            position_.x = Approach(position_.x, pressTargetX_, 15.0f);
            position_.y = Approach(position_.y, 600.0f, 13.0f);
            if (Abs(position_.x - pressTargetX_) < 1.0f &&
                Abs(position_.y - 600.0f) < 1.0f)
                ChangePressPhase(PressPhase::Charge);
            break;
        case PressPhase::Charge:
            // A visible pause makes the vertical lane readable.
            if (pressFrame_ >= 32) ChangePressPhase(PressPhase::Dive);
            break;
        case PressPhase::Dive:
            pressFallSpeed_ = Clamp(pressFallSpeed_ + 1.9f, 0.0f, 31.0f);
            position_.y -= pressFallSpeed_;
            if (position_.y <= BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f) {
                position_.y = BossStage1::kFloorY + BossStage1::kBossHeight * 0.5f;
                ChangePressPhase(PressPhase::Impact);
                ShakeCamera(9.0f, 13);
                visual_.OnImpact({ position_.x, BossStage1::kFloorY + 5.0f });
            }
            break;
        case PressPhase::Impact:
            if (pressFrame_ >= 8) ChangePressPhase(PressPhase::Stun);
            break;
        case PressPhase::Stun:
            if (pressFrame_ >= 38) ChangePressPhase(PressPhase::Return);
            break;
        case PressPhase::Return:
            position_.y = Approach(position_.y, 370.0f, 9.0f);
            if (Abs(position_.y - 370.0f) < 1.0f) ChangeState(BossState::Idle);
            break;
        }
        return;
    }
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
void Boss::TriggerAttackForEditor(BossAttack attack) {
    if (IsDefeated() || state_ == BossState::ArmorCurl ||
        state_ == BossState::ArmorFalling || state_ == BossState::ArmorImpact ||
        state_ == BossState::ArmorStun || state_ == BossState::WakeBlast) return;
    if (attack != BossAttack::Top && attack != BossAttack::YoYo) return;
    nextAttack_ = attack;
    ChangeState(BossState::Toy);
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
            playerPosition.y <= rushTargetY_ + kHitRadius + playerRadius) {
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
    if (coasterStage_) {
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
        position_.x = Clamp(position_.x + flightVelocity_.x, 150.0f, 1130.0f);
        position_.y = Clamp(position_.y + flightVelocity_.y, 210.0f, 590.0f);
        return;
    }
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
        // 毎回同じ位置へ戻さず、新しい巡回地点へ向かう。
        UpdateFlight();
        if (stateFrame_ >= ((coasterStage_ || IsEnraged()) ? 24 : battleSettings_.idleFrames)) {
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
        const float duration = static_cast<float>(coasterStage_ && nextAttack_ == BossAttack::Rush ? 60 :
            (IsEnraged() ? durationFrames * 3 / 4 : durationFrames));
        // プレスは予兆前半だけ追尾し、後半は予測位置が固定される。
        if (nextAttack_ == BossAttack::Press && stateFrame_ < 12 && targetPosition_)
            pressTargetX_ = targetPosition_->x;
        // 突進の予兆中は「開始側の端」へ飛んでいき、床スレスレまで降りる。
        // Approachで最大移動量を制限するため、開始位置へのテレポートは起きない。
        if (nextAttack_ == BossAttack::Rush) {
            const float halfWidth = BossStage1::kBossWidth * 0.5f;
            const float startX = rushDirection_ > 0
                ? BossStage1::kArenaLeft + halfWidth
                : BossStage1::kArenaRight - halfWidth;
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
        visual_.SetWarning(Clamp(stateFrame_ / duration, 0.0f, 1.0f),
            { nextAttack_ == BossAttack::Press ? pressTargetX_ : targetPos.x,warningY });
        // 突進だけは、予兆時間が終わっても開始側の端に着くまで攻撃を始めない。
        // そのため、どこから予兆に入っても必ずステージの端から端まで走れる。
        bool readyToRush = true;
        if (nextAttack_ == BossAttack::Rush) {
            const float halfWidth = BossStage1::kBossWidth * 0.5f;
            const float startX = rushDirection_ > 0
                ? BossStage1::kArenaLeft + halfWidth
                : BossStage1::kArenaRight - halfWidth;
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
    case BossState::Rush: {
        // ステージの端から反対側の端まで、プレイヤーの現在位置に左右されず突進する。
        position_.y = rushTargetY_;
        const float maxSpeed = (battleSettings_.rushSpeed > 1.0f ? battleSettings_.rushSpeed : 1.0f) *
            (coasterStage_ ? 1.75f : (IsEnraged() ? 1.25f : 1.0f));
        const float accel = battleSettings_.rushAccel > 0.1f ? battleSettings_.rushAccel : 0.1f;
        rushVelocity_ = Clamp(rushVelocity_ + accel, 0.0f, maxSpeed);
        position_.x += rushDirection_ * rushVelocity_;

        // 機体の幅も含めて端に収まる位置で止める。行き過ぎや画面外への消失を防ぐ。
        const bool reachedEdge = rushDirection_ < 0 ?
            position_.x <= rushTargetX_ : position_.x >= rushTargetX_;
        if (reachedEdge) {
            position_.x = rushTargetX_;
            rushVelocity_ = 0.0f;
            rushDodgeStunPending_ = wireUsedDuringRush_ && !rushHitPlayer_;
            ShakeCamera(3.0f, 5);
            ChangeState(rushDodgeStunPending_ ? BossState::Dizzy : BossState::Recovery);
        }
        break;
    }
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
        // 2回の3方向射撃（計6発）。弾数が少なくても回避を考えさせる。
        if (stateFrame_ == 9 || stateFrame_ == 28) {
            for (int i = -1;i <= 1;++i) Shoot(i);
            ShakeCamera(1.8f, 3);
            position_.x -= rushDirection_ * 16.0f; // 反動。次は今の位置から飛行する
        }
        else if (stateFrame_ > 9 && stateFrame_ < 42) {
            // 射撃中は反動の余韻だけを残す。目標位置へ引き戻さない。
            flightVelocity_.x *= 0.88f;
            position_.x = Clamp(position_.x + flightVelocity_.x, 225.0f, 1055.0f);
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
            PortalDraw::Box({ (BossStage1::kArenaLeft + BossStage1::kArenaRight) * 0.5f, rushTargetY_ },
                BossStage1::kArenaRight - BossStage1::kArenaLeft, 7.0f, color);
            if (coasterStage_) {
                const float ceiling = rushTargetY_ + kHitRadius;
                PortalDraw::Box({ 640.0f, (ceiling + BossStage1::kFloorY) * 0.5f },
                    1270.0f, ceiling - BossStage1::kFloorY, 0xFF644334);
            }
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
    // コマ256px・ヨーヨー128pxの画像を使い、攻撃の差が一目で分かるようにする。
    if (state_ == BossState::WakeBlast && stateFrame_ < 12) {
        // 起き上がり：描画BOX1枚で衝撃を表す。Player側へ同じ瞬間に速度を与える。
        PortalDraw::Box(position_, 300.0f + stateFrame_ * 17.0f, 8.0f, 0xFFE9BCCC);
    }
    if (state_ == BossState::Toy) {
        static int yoyoHandle = -2;
        if (nextAttack_ == BossAttack::Top) {
            DrawSpinningTop(toyPosition_, stateFrame_);
        }
        else {
            PortalDraw::Line(position_, toyPosition_, 0xF0D3FFFF);
            DrawToySprite("./resources/texture/boss/form1/yo-yo.png",
                toyPosition_, 128, stateFrame_ * 0.17f, yoyoHandle);
        }
    }
    for (const auto& b : bullets_) if (b.active) visual_.DrawBullet(b.position, b.velocity);
    visual_.DrawFront();
    if (IsDefeated()) PortalDraw::Box(position_, 72.0f, 13.0f, 0x777777FF);
}
