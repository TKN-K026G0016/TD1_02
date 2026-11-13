#pragma once
#include "PortalInput.h"
#include "PortalTentacle.h"
#include "PortalConfig.h"

// 衝突した攻撃の種類。Stage.cppがダメージ計算に使う。
enum class PortalAttackKind { None, Normal, Spin, Grapple };

class PortalPlayer {
public:
    void Initialize(Vector2 position);
    // fixedCount個までは移動杭。それ以降はボスの外装取っ掛かり。
    void Update(const PortalCommands& input, const Vector2* anchors, int anchorCount, int fixedCount = 4);
    int ConsumeArmorPeelRequest();
    // 【外装フック】引っ張っている相手と張力を表示・ボスの姿勢変形に使う。
    bool IsPullingArmor() const { return armorPeelFrames_ > 0; }
    // ゲージが緑になったら長押しを終えてよい（タイミング制限なし）。
    bool IsArmorPullReady() const { return armorPeelFrames_ >= PortalConfig::kArmorPullMinFrames; }
    int GetArmorPullTarget() const { return armorPeelTarget_; }
    float GetArmorPullProgress() const; // 0.0～1.0。ゲージ描画に使う
    // 【命中演出】StageがBossへの攻撃成立を通知。毎フレームの移動は止めない。
    void OnAttackHit(PortalAttackKind kind);
    void OnArmorPeeled();
    // ボスが外装スタンから復帰した衝撃。無敵時間とは別に速度だけ吹き飛ばす。
    void ApplyWakeBlast(Vector2 bossPosition, float pushX, float downSpeed); // -1:なし、0..2:剥がしたい外装番号（Stageへ一度だけ通知）
    void Draw() const;
    const Vector2& GetPosition() const { return position_; }
    Vector2 GetVelocity() const { return velocity_; }
    int GetSelectedAnchor() const { return selectedAnchor_; }
    bool IsGripping() const;
    bool IsSpinning() const { return spinFrames_ > 0; }
    bool IsAutoLaunching() const { return launchVisualFrames_ > 0; }
    bool HasWireMomentum() const { return launchMomentumFrames_ > 0; } // ワイヤー射出後の回避中も有効
    bool IsGrounded() const { return grounded_; }

    // 【ボス連携】触手・スピンとボス円の接触判定。1フレームで最強の攻撃を返す。
    PortalAttackKind HitTestEnemy(Vector2 target, float radius) const;

    // 【被ダメージ】無敵時間付き。ダメージ受けたら少し吹き飛ぶ。
    bool TakeDamage(int damage, Vector2 source);
    int GetHp() const { return hp_; }
    static constexpr int kMaxHp = 100;
    bool IsDefeated() const { return hp_ <= 0; }

private:
    void ReleaseAll();
    int FindAnchor(Vector2 direction, const Vector2* anchors, int count, int fixedCount) const;
    void StartSpin();

    Vector2 position_ = {}, velocity_ = {}, aim_ = { 1,0 };
    int facing_ = 1, cooldown_ = 0, selectedAnchor_ = -1, animationFrame_ = 0;
    int coyoteFrames_ = 0, attackBufferFrames_ = 0, jumpBufferFrames_ = 0;
    int attackIntentTarget_ = -1; // キーボードJの場合は押した瞬間のロックを固定
    // 【2スティック操作】右スティックの短い傾き=攻撃 / 杭を狙って保持=グラップル。
    int stickLockTarget_ = -1;
    int stickLockFrames_ = 0;
    bool stickLockConsumed_ = false; // 一度の傾きで同じ杭を再グラップルしない
    int grappleSpinTick_ = 0; // ワイヤー中の小さな回転攻撃（追加パーティクルなし）
    int launchMomentumFrames_ = 0; // 射出の勢いを空中でも保つ猶予
    int lastLaunchedAnchor_ = -1; // 直前の杭だけ再接続を少し抑制
    int activeGripAnchor_ = -1; // 接続した杭の番号（照準が動いても保持）
    float airTilt_ = 0.0f; // 空中の本体の傾き（当たり判定は変更しない）
    int launchVisualFrames_ = 0;  // 自動射出の演出残り時間
    int regripLockFrames_ = 0;    // 接続済みの杭へ即戻らないための入力猶予
    float grappleForwardSign_ = 1.0f; // 旧互換値。シームレス射出の向きは実速度で決まる
    Vector2 grappleTravelDirection_ = { 1.0f, 0.0f }; // 接続時の杭方向。射出直前の微調整には使わない
    int grappleAge_ = 0; // 接続直後の誤解除対策。杭に最接近するまでの経過フレーム
    float grappleLastDistance_ = 0.0f; // 距離が増加し始める前に自動解除するための記録
    // 【外装剥がしの動き】ボス用の触手が伸びる→引く→外装が飛ぶ。プレイヤーは引っ張られない。
    // 【外装引き剥がし】以前の「12Fで自動剥がし」ではなく、
    // J長押し→適切な時に離す「張力のタイミング操作」に変更。
    int armorPeelFrames_ = 0; // フックを引っ張っている経過フレーム（0=未接続）
    int armorPullFailFrames_ = 0; // 失敗したときの視覚フィードバック
    int armorPeelTarget_ = -1; // ボス外装の添字0..2
    int armorPeelRequest_ = -1;
    int armorTetherSlot_ = -1;
    int hitConfirmFrames_ = 0; // 既存BOXの色と傾きでヒットを強調する
    // 【通常攻撃】触手描画インスタンスと独立した攻撃状態。
    // すべてのゲートが使用中でもJ押下が空振り入力にならない。
    int normalAttackFrames_ = 0;
    Vector2 normalAttackDirection_ = { 1.0f, 0.0f };
    int spinFrames_ = 0;          // 0なら回転攻撃していない
    int hurtFrames_ = 0;          // ダメージ後の無敵時間
    int hp_ = kMaxHp;
    bool jumpCutArmed_ = false; // SPACEジャンプの長押し/短押しを反映
    bool grounded_ = false;
    float castPulse_ = 0.0f, landingBounce_ = 0.0f, gripStretch_ = 0.0f, bodyLean_ = 0.0f;
    PortalTentacle gates_[PortalConfig::kGateCount];
};
