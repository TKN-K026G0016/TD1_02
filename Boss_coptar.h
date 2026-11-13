#pragma once
#include "Vector2.h"
#include "BossVisualEffect.h"
#include "BossStage1Tuning.h"
#include "BossBattleSettings.h"

// 第1ステージの行動AI。本体は白BOX、外装/弱点も数個のBOXだけで表現。
// StageがBossの実体を所有し、Player座標への読み取り専用ポインタを渡す。
enum class BossState { Idle, Warning, Rush, Recovery, Dizzy, ArmorCurl, ArmorStun, WakeBlast, Press, Shot, Toy, Defeated };
enum class BossAttack { Press, Shot, Rush, Top, YoYo };

class Boss {
public:
    static constexpr int kMaxHp = BossStage1::kMaxHp;
    static constexpr float kHitRadius = BossStage1::kBossHitRadius; // 大きくした本体に合わせた円判定
    void Initialize(Vector2 spawnPos);
    // ゲーム起動時/F8リロード時にエディターで調整した5技とフック位置を反映。
    void SetBattleSettings(const BossBattleSettings& config) { if (config.Valid())battleSettings_ = config; }
    void SetTarget(const Vector2* playerPosition);
    // 【AI入力】速度・地上/空中・ワイヤーの使用状況を受け取る。
    // PlayerをBossに所有させない（既存のクラス分担を維持）。
    void SetTargetMotion(Vector2 velocity, bool grounded, bool usingWire);
    void Update();
    void Draw() const;

    // グラップルの綱も外装剥がしに使う。第2引数はその判定。
    // 戻り値: このフレームにダメージ/外装ヒットが成立したらtrue。
    // 攻撃種の違い：通常=連続ヒット、スピン=カウンター、グラップル=外装剥がし。
    // 射出直後は攻撃の威力も少し上がる。互換性のため既存の引数も維持。
    bool TakeDamage(int amount, bool isGrappleHit = false,
        bool isSpinHit = false, bool fromWireLaunch = false);
    int TryHitPlayer(Vector2 playerPosition, float playerRadius, Vector2* hitSource);

    int GetHp() const { return hp_; }
    int GetArmorPlates() const { return armorPlates_; }
    int GetMaxArmorPlates() const { return BossStage1::kArmorPlates; }
    bool IsCoreExposed() const { return armorPlates_ == 0; }
    bool IsDefeated() const { return hp_ <= 0; }
    bool IsEnraged() const { return hp_ <= kMaxHp / 2; }
    bool IsWeakPointOpen() const; // プレス硬直・ワイヤー回避後の失速中は弱点ダメージUP
    bool IsDizzy() const { return state_ == BossState::Dizzy || state_ == BossState::ArmorStun; }
    bool IsArmorStun() const { return state_ == BossState::ArmorStun; }
    bool IsArmorCurling() const { return state_ == BossState::ArmorCurl; }
    bool IsWaking() const { return state_ == BossState::WakeBlast; }
    // スタン復帰時の吹き飛ばしをStage側に一度だけ渡す。
    bool ConsumeWakeBlast();
    void NoteWireMotion(bool usingWire); // プレイヤーのワイヤー使用をAIへ報告
    Vector2 GetPosition() const { return position_; }
    Vector2 GetCoreHitPoint() const { return { position_.x + battleSettings_.coreOffset.x,position_.y + battleSettings_.coreOffset.y }; }
    float GetCoreHitRadius() const { return battleSettings_.coreRadius; }
    // 外装用のボス本体グリップポイント。プレイヤーが直接ワイヤーで剥がせる。
    // 【外装の取っ掛かり】3枚にそれぞれ照準ポイントを持たせる。
    // 通常の杭とは違い、Jで引っかけてもプレイヤーを引き寄せず外装だけ剥がす。
    bool IsArmorAttached(int plate) const;
    Vector2 GetArmorGripPoint(int plate) const;
    // プレイヤーがJでフックを引いている最中、板をたわませる（描画追加なし）。
    void SetArmorStrain(int plate, float progress);
    bool TryPeelArmor(int plate); // ポイントを狙って触手で引いた時だけ外装を剥がす
    Vector2 GetArmorGripPoint() const { return GetArmorGripPoint(1); } // 旧API互換
    BossState GetState() const { return state_; }
    BossAttack GetSelectedAttack() const { return nextAttack_; }
    int GetAttackCount() const { return attackCount_; }
    int GetNormalCombo() const { return normalComboHits_; }

private:
    enum class PressPhase { Align, Charge, Dive, Impact, Stun, Return };
    struct Bullet {
        Vector2 position = {}, velocity = {};
        int life = 0;
        bool active = false;
        bool debris = false; // プレスで落ちた部品だけ重力をかける
    };
    static constexpr int kMaxBullets = 12;

    BossBattleSettings battleSettings_ = {};
    Vector2 homePosition_ = {}, position_ = {}, rushStart_ = {};
    const Vector2* targetPosition_ = nullptr;
    BossState state_ = BossState::Idle;
    BossAttack nextAttack_ = BossAttack::Shot;
    BossAttack lastAttack_ = BossAttack::Shot;
    PressPhase pressPhase_ = PressPhase::Align;
    BossVisualEffect visual_ = {};
    Bullet bullets_[kMaxBullets] = {};

    int hp_ = kMaxHp;
    bool wakeBlastPending_ = false; // 1フレームのみtrue
    int armorPlates_ = BossStage1::kArmorPlates;
    int armorStrainPlate_ = -1; // 張力がかかっている板
    float armorStrain_ = 0.0f;   // 0～1。既存BOXの角度だけ変える
    int armorDamage_ = 0;  // 通常攻撃による揺れ/ダメージ演出用（外装は剥がれない）
    bool armorAttached_[BossStage1::kArmorPlates] = { true, true, true, true };
    Vector2 peeledPosition_[BossStage1::kArmorPlates] = {};
    int peeledFrames_[BossStage1::kArmorPlates] = {}; // 外装破壊時だけBOX一枚が飛ぶ
    int hitCooldown_ = 0;
    int hitstop_ = 0; // ボスだけ停止。描画物は増やさない。
    int stateFrame_ = 0;
    int pressFrame_ = 0;
    int attackCount_ = 0;
    int defeatFrame_ = 0;
    int nextBulletIndex_ = 0;
    // 【AI選択】5つの既存攻撃だけで変化を作るための記憶。
    Vector2 targetVelocity_ = {};
    bool targetGrounded_ = true;
    bool targetUsingWire_ = false;
    int targetStillFrames_ = 0;
    BossAttack previousAttack_ = BossAttack::Press;
    unsigned int aiRandomState_ = 0x9E3779B9u;
    // 【プレイヤー攻撃への反応】追加の描画は使わない。
    int normalComboHits_ = 0;
    int normalComboTimer_ = 0;
    int spinCounterCooldown_ = 0;
    bool wireUsedDuringRush_ = false;  // 突進中/予兆中にワイヤーで回避行動を取ったか
    bool rushHitPlayer_ = false;       // 突進がプレイヤーに当たったか
    bool rushDodgeStunPending_ = false;// 回避成功したら帰還後に失速する
    int rushDirection_ = -1;
    float rushVelocity_ = 0.0f;
    float rushTargetY_ = BossStage1::kRushGroundY; // 地上突進専用：高さ固定
    float rushTargetX_ = 0.0f; // 【突進の狙い】予兆終了時に固定。追尾し続けない
    float pressTargetX_ = 0.0f, pressFallSpeed_ = 0.0f;
    Vector2 pressReturnStart_ = {};

    // コマ・ヨーヨーを共通の物理オブジェクト一個で表現する。
    Vector2 toyPosition_ = {}, toyVelocity_ = {};
    int toyDirection_ = -1;
    bool toyReturning_ = false;
    bool toyLanded_ = false;
    float toyOriginX_ = 0.0f; // コマが一往復したら帰還する判定に使う
    float toyAimY_ = 150.0f; // ヨーヨーが狙う高さ（攻撃開始時に固定）
    bool toyHasHitPlayer_ = false;

    void ChangeState(BossState next);
    void ChangePressPhase(PressPhase next);
    void ChooseAttack();
    void UpdatePress();
    void UpdateToy();
    void Shoot(int spreadIndex = 0);
    void UpdateBullets();
};
