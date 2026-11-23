#pragma once
#include "PortalPlayer.h"
#include "BossBattleSettings.h"
#include "Stage2Gimmicks.h"

// 第1ステージ：画面固定の回避アリーナ。
// ワイヤーで「高い出口に登る」のではなく「突進の攻撃ラインから脱出」する。
// 杭を増やしたい場合はエディターのボス戦タブから変更。
class PortalArena {
public:
    void Initialize();
    // エディターが配置した杭をゲームに反映する。リロード時も使用。
    void SetBattleSettings(const BossBattleSettings& settings);
    void SetCoasterStage(bool active) { coasterStage_ = active; }
    void UpdateStage2(Vector2 bossPosition, int bossHp, int bossMaxHp);
    int Stage2HitPlayer(Vector2 player, Vector2* source);
    bool Stage2CounterReady() const { return stage2_.CanCounter() && stage2_.Loosen() >= 100.0f; }
    bool ConsumeStage2Counter() { return stage2_.ConsumeLoosen(); }
    bool Update(); // Rでリスタートするとtrue
    void Draw() const; // 従来の描画（他の画面で使用する場合の互換用）
    void DrawWorld() const; // 背景・床・グリップポイントだけ
    void DrawPlayer() const; // プレイヤーと触手だけ
    void DrawHUD() const;
    const Vector2& GetPlayerPosition() const { return player_.GetPosition(); }
    PortalPlayer& GetPlayer() { return player_; }
    // 【連打】右スティックの弾き直し/キーボードJの押し直しをそのまま流用。
    bool ConsumeMashPulse() { const bool b = mashPulse_; mashPulse_ = false; return b; }

    const PortalPlayer& GetPlayer() const { return player_; }
    // 【外装の取っ掛かり】ボスに付けた3点。通常の杭とは別のターゲット。
    // enabled=falseの板は入力候補に入れず、描画もしない。
    void SetBossArmorTargets(const Vector2 points[BossBattleSettings::kArmorHooks], const bool enabled[BossBattleSettings::kArmorHooks]);
    // ジェット4・パイプ・目。ボスが動くたび位置を更新する。
    static constexpr int kExtraBossPoints = 6;
    void SetBossExtraTargets(const Vector2 points[kExtraBossPoints], const bool enabled[kExtraBossPoints]);
private:
    // 【混雑解消】4個→3個。両端＋中央上で、それぞれ離れた回避ルートを作る。
    static constexpr int kMaxAnchors = BossBattleSettings::kMaxAnchors;
    int anchorCount_ = 6;
    PortalInput input_;
    PortalPlayer player_;
    // 【回避用杭】上方向に登るのではなく、突進から逃げる複数の選択肢。
    // 地上から届きやすい左寄りと、連続グラップルしやすい右寄りに配置。
    Vector2 anchors_[kMaxAnchors] = { {155,320},{450,530},{780,360},{1090,530},{270,590},{1180,300} };
    bool coasterStage_ = false;
    bool connected_ = false;
    bool mashPulse_ = false;
    bool armorPointEnabled_[BossBattleSettings::kArmorHooks] = {};
    Vector2 armorPoints_[BossBattleSettings::kArmorHooks] = {};
    bool extraPointEnabled_[kExtraBossPoints] = {};
    Vector2 extraPoints_[kExtraBossPoints] = {};
    int frame_ = 0;
    Stage2Gimmicks stage2_;
};
