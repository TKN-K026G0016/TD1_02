#pragma once
#include "PortalPlayer.h"
#include "BossBattleSettings.h"

// 第1ステージ：画面固定の回避アリーナ。
// ワイヤーで「高い出口に登る」のではなく「突進の攻撃ラインから脱出」する。
// 杭を増やしたい場合はエディターのボス戦タブから変更。
class PortalArena {
public:
    void Initialize();
    // エディターが配置した杭をゲームに反映する。リロード時も使用。
    void SetBattleSettings(const BossBattleSettings& settings);
    bool Update(); // Rでリスタートするとtrue
    void Draw() const;
    void DrawHUD() const;
    const Vector2& GetPlayerPosition() const { return player_.GetPosition(); }
    PortalPlayer& GetPlayer() { return player_; }
    const PortalPlayer& GetPlayer() const { return player_; }
    // 【外装の取っ掛かり】ボスに付けた3点。通常の杭とは別のターゲット。
    // enabled=falseの板は入力候補に入れず、描画もしない。
    void SetBossArmorTargets(const Vector2 points[BossBattleSettings::kArmorHooks], const bool enabled[BossBattleSettings::kArmorHooks]);
private:
    // 【混雑解消】4個→3個。両端＋中央上で、それぞれ離れた回避ルートを作る。
    static constexpr int kMaxAnchors = BossBattleSettings::kMaxAnchors;
    int anchorCount_ = 6;
    PortalInput input_;
    PortalPlayer player_;
    // 【回避用杭】上方向に登るのではなく、突進から逃げる複数の選択肢。
    // 地上から届きやすい左寄りと、連続グラップルしやすい右寄りに配置。
    Vector2 anchors_[kMaxAnchors] = { {155,320},{450,530},{780,360},{1090,530},{270,590},{1180,300} };
    bool connected_ = false;
    bool armorPointEnabled_[BossBattleSettings::kArmorHooks] = {};
    Vector2 armorPoints_[BossBattleSettings::kArmorHooks] = {};
    int frame_ = 0;
};
