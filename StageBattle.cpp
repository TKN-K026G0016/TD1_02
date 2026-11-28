// ============================================================================
// 【このファイルの役割】StageBattle.cpp — ステージ1・2共通のボス戦処理
// PortalArena(プレイヤー)とBoss(敵)をつなぐ。ネジ解除、ダメージ、カメラ、クリア判定、描画順を決める。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

#include "StageBattle.h"
#include "StageBattleContext.h"
#include "Stage1.h"
#include "Stage2.h"
#include "Camera.h"
#include <Novice.h>
#include <dinput.h>
#include <cstring>
#include <string>
#include <algorithm>
#include "BossBattleSettings.h"
#include "PlayerTuning.h"
#include "AssetEditor.h"
#include "BossPartsEditor.h"
#include "Stage2Tuning.h"
#include "BossCombatEditorSettings.h"
#include "MiniUnityEditor.h"
#include "StageCollision.h"
#include "CameraBackgroundTuning.h"
#include "PortalMath.h"
#include "CreatorGameRuntime.h"
#include "GameHudEditor.h"

#include "Effect.h"

// ================================================================
// 【第1ステージ】「突進の予兆→ワイヤー回避→失速時に反撃→外装/コアを破壊」。
// 現在は左スティックで移動、右スティックで8方向触手攻撃。Jはキーボード用。AIはBoss、プレイヤーはPortalArenaが管理。
// 画面の派手さは運動/停止/画面揺れを使い、オブジェクト描画は最小限。
// ================================================================
// ステージ1・2の戦闘状態は1箇所のみ。
StageBattleInternal::BattleContext StageBattleInternal::battleContext;
using namespace StageBattleInternal;

void StageBattle::Reset(StageId stage) {
    GameHudEditor::SelectAndReset(int(stage));
    CameraBackgroundTuning::SelectStage(int(stage));
    battleContext.arena.Initialize();
    battleContext.arena.GetPlayer().SetMaxHp(battleContext.liveBattleConfig.playerMaxHp);
    battleContext.arena.SetBattleSettings(battleContext.liveBattleConfig);
    battleContext.boss.SetBattleSettings(battleContext.liveBattleConfig);
    battleContext.boss.Initialize({ battleContext.liveBattleConfig.bossSpawn.x, battleContext.liveBattleConfig.bossSpawn.y });
    battleContext.boss.SetTarget(&battleContext.arena.GetPlayerPosition());
    const bool coaster = stage == StageId::Coaster;
    if (coaster) Stage2::SetupBoss(battleContext.boss);
    battleContext.arena.SetCoasterStage(coaster);
    battleContext.stageCleared = false;
    battleContext.tutorialFrame = 0;
    MiniUnityEditor::ResetRuntime();
    battleContext.clearDelayFrames = 0;
    battleContext.screwRepairCooldownFrames = BossCombatEditor::values.repairCooldown;
    battleContext.repairWasActive = false;
    SetBossMashCamera(false, 640.0f, 360.0f);
    SetCameraFollowY(360.0f);
}

// IMGUI無効ビルドでもステージ2の保存値を読めるよう、共通ヘルパーは外側に置く。



// ------------------------------------------------------------------------
// 【関数】InitStage()
// 何をする？ ボス戦の設定読込、プレイヤー・ボス初期化、カメラ位置を整える。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void StageBattle::LoadSettings() {
    // ボス1～4のパーツ配置を起動時に復元。画像未登録なら従来の描画を使用。
    AssetEditor::FixSettingsLocation();
    BossPartsEditor::Initialize(AssetEditor::resourceRoot + "\\boss_parts_layout.txt");
    // 設定ファイルが無い場合は前回正常に読めた値／初期値で続行。
    // 【注目】【設定読込】エディターの設定が無い場合は既定値を使う。
    battleContext.liveBattleConfig.Load(AssetEditor::BossSettingsPath().c_str());
    Stage2Tuning::Load(Stage2FilePath().c_str());
    BossCombatEditor::Load(BossCombatFilePath().c_str());
    PlayerTuning::Load(PlayerTuningFilePath().c_str());
    MiniUnityEditor::Load();
    TileMapEditor::Load();
    CameraBackgroundTuning::Load(CameraBGFilePath().c_str());
    GameHudEditor::Load(GameHudFilePath().c_str());
    battleContext.creatorRuntime.Reload();
}

// ------------------------------------------------------------------------
// 【関数】UpdateStage()
// 何をする？ プレイヤーとボスを更新し、ネジ解除・連打・ダメージ・クリアを接続する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------


bool StageBattle::IsCleared() { return battleContext.stageCleared; }

// ボスGPの情報を一度に取得。可視・退場判定は呼び出し側から指定する。
static void SyncBossGripTargets(bool bossOffscreen) {
    Vector2 armorPoints[BossStage1::kArmorPlates];
    bool armorExists[BossStage1::kArmorPlates];
    // 【繰り返し】対象（触手・ネジ・弾など）を順に確認する。
    for (int i = 0;i < BossStage1::kArmorPlates;++i) {
        armorPoints[i] = battleContext.boss.GetArmorGripPoint(i);
        // 本体0番は移動用GP（破壊不可）。破壊対象としては表示しない。
        armorExists[i] = !bossOffscreen && battleContext.boss.IsArmorAttached(i);
    }
    battleContext.arena.SetBossArmorTargets(armorPoints, armorExists);
    Vector2 extraBossPoints[Boss::kAccessoryGripCount];
    bool extraBossEnabled[Boss::kAccessoryGripCount];
    for (int i = 0; i < Boss::kAccessoryGripCount; ++i) {
        extraBossPoints[i] = battleContext.boss.GetAccessoryGripPoint(i);
        extraBossEnabled[i] = !bossOffscreen && battleContext.boss.IsAccessoryGripEnabled(i);
    }
    battleContext.arena.SetBossExtraTargets(extraBossPoints, extraBossEnabled);
}

void StageBattle::Update(const char* liveKeys) {
    if (GetIsHitStop()) return;

    const bool f8Now = liveKeys[DIK_F8] != 0;
    if (f8Now && !battleContext.f8Prev) {
        PlayerTuning::Load(PlayerTuningFilePath().c_str());
        GameHudEditor::Load(GameHudFilePath().c_str());
        BossCombatEditor::Load(BossCombatFilePath().c_str());
        BossBattleSettings next;
        if (next.Load(AssetEditor::BossSettingsPath().c_str())) { battleContext.liveBattleConfig = next; battleContext.creatorRuntime.Reload(); ResetStageBattle(GetStageId()); battleContext.reloadIndicatorFrames = 120; }
        else battleContext.reloadIndicatorFrames = -120;
    }
    battleContext.f8Prev = f8Now;
    if (battleContext.reloadIndicatorFrames > 0)--battleContext.reloadIndicatorFrames;
    if (battleContext.reloadIndicatorFrames < 0)++battleContext.reloadIndicatorFrames;
    battleContext.creatorRuntime.Update();
    const bool bossOffscreen = GetStageId() == StageId::Coaster && Stage2::IsBossOffscreen(battleContext.boss);
    // 【重要】ボス外装の四隅ネジ4点を移動杭とは別の目標として渡す。
    // 各ネジは右スティックで狙って接続すると外せる。プレイヤー自身はボスへ引き寄せられない。
    SyncBossGripTargets(bossOffscreen);
    // プレイヤー更新より前に退場を通知する（退場時のボスGP接続解除）。
    battleContext.arena.SetBossOutsideScreen(bossOffscreen);
    // Update前の位置を衝突判定に渡し、ワイヤー移動中の床・壁へのめり込みを防ぐ。
    const Vector2 beforeMove = battleContext.arena.GetPlayer().GetPosition();
    if (battleContext.arena.Update()) {
        ResetStageBattle(GetStageId());
        return;
    }
    {
        StageCollision::Rect worldColliders[StageCollision::kMaxRects]{};
        int used = MiniUnityEditor::CollectSolidColliders(int(GetStageId()), worldColliders, StageCollision::kMaxRects);
        if (used < StageCollision::kMaxRects) {
            used += TileMapEditor::CollectColliders(int(GetStageId()), worldColliders + used, StageCollision::kMaxRects - used);
        }
        // ボスパーツ側も同じ物理判定を共有する（外れた部位は判定も消す）。
        const int bossForm = GetStageId() == StageId::Copter ? 0 : 1;
        const int rest = StageCollision::kMaxRects - used;
        const int additional = BossPartsEditor::CollectColliders(bossForm, battleContext.boss.GetPosition(),
            worldColliders + used, rest, [&](int link)->bool {
                if (link >= 0 && link < BossStage1::kArmorPlates) return battleContext.boss.IsArmorAttached(link);
                if (link >= 10 && link < 10 + Boss::kAccessoryGripCount) return battleContext.boss.IsAccessoryGripEnabled(link - 10);
                return true;
            });
        used += additional;
        battleContext.arena.GetPlayer().ResolveStageColliders(beforeMove, worldColliders, used);
    }
    // 既存のプレイヤー基本物理はそのまま。時間制だけHP敗北を使わない。
    if (!GameHudEditor::UsesHp())
        battleContext.arena.GetPlayer().SetCurrentHpForEditor(battleContext.arena.GetPlayer().GetMaxHp());
    bool timerActive = !battleContext.stageCleared && !battleContext.boss.IsDefeated() && !battleContext.arena.GetPlayer().IsDefeated();
#ifdef USE_IMGUI
    timerActive = timerActive && !StageBattleEditor::IsOpen(); // 編集中は制限時間を消費しない
#endif
    GameHudEditor::Tick(timerActive);
    ++battleContext.tutorialFrame;
    CameraBackgroundTuning::Tick();
    MiniUnityEditor::TickRuntime();
    PortalPlayer& player = battleContext.arena.GetPlayer();
    CameraBackgroundTuning::SetTarget(player.GetPosition().x, player.GetPosition().y);
    // シーン編集で配置した敵・危険領域との接触をゲームへ反映。
    // 既存のプレイヤー物理・ジャンプ・触手処理は一切変更しない。
    if (GameHudEditor::UsesHp() && battleContext.tutorialFrame % 30 == 0 && !player.IsDefeated()) {
        int damage = 0;
        Vector2 source{};
        if (MiniUnityEditor::CheckPlayerHit(int(GetStageId()), player.GetPosition(),
            24.0f, &damage, &source)) player.TakeDamage(damage, source);
    }
    // ボス編集パーツの接触ダメージ（画像の有効/破壊状態と連動）。
    if (GameHudEditor::UsesHp() && battleContext.tutorialFrame % 30 == 0 && !player.IsDefeated()) {
        StageCollision::Rect partRects[BossPartsEditor::kMaxParts]{};
        const int bossForm = GetStageId() == StageId::Copter ? 0 : 1;
        const int n = BossPartsEditor::CollectColliders(bossForm, battleContext.boss.GetPosition(), partRects,
            BossPartsEditor::kMaxParts, [&](int link)->bool {
                if (link >= 0 && link < BossStage1::kArmorPlates) return battleContext.boss.IsArmorAttached(link);
                if (link >= 10 && link < 10 + Boss::kAccessoryGripCount) return battleContext.boss.IsAccessoryGripEnabled(link - 10);
                return true;
            });
        const Vector2 pos = player.GetPosition();
        for (int i = 0;i < n;++i) {
            const auto& r = partRects[i];
            if (r.type != StageCollision::Type::Damage || r.damage <= 0) continue;
            if (StageCollision::Intersect(pos.x - 18.f, pos.x + 18.f, r.left, r.right) &&
                StageCollision::Intersect(pos.y - 30.f, pos.y + 30.f, r.bottom, r.top)) {
                player.TakeDamage(r.damage, battleContext.boss.GetPosition()); break;
            }
        }
    }
    if (GameHudEditor::UsesHp() && battleContext.creatorRuntime.showCollision && battleContext.creatorRuntime.PlayerInHazard(player.GetPosition()) && (battleContext.tutorialFrame % 45 == 0))
        player.TakeDamage(6, player.GetPosition());
    // 【連打】攻撃が届かない距離でもスタン中は右スティック弾き直しでコアを殴れる。
    const bool mashPulse = battleContext.arena.ConsumeMashPulse();
    if (battleContext.boss.IsMashActive() && mashPulse) battleContext.boss.RegisterMashHit();
    SetBossMashCamera(battleContext.boss.IsMashActive(), battleContext.boss.GetPosition().x, battleContext.boss.GetPosition().y);

    // 四隅ネジは接続時に解除される。長押し張力ゲージは使わない。
    // 四隅のネジを1個ずつ外す。最後の1個で外装めくれ演出開始。
    const int peelRequest = player.ConsumeArmorPeelRequest();
    if (peelRequest >= 0 && !battleContext.boss.IsDefeated()) {
        // 【外装ヒット確認】本当に剥がれた場合だけプレイヤーのポーズを強調。
        bool removed = false;
        if (peelRequest < BossStage1::kArmorPlates)
            removed = battleContext.boss.TryPeelArmor(peelRequest);
        else
            removed = battleContext.boss.TryPeelAccessory(peelRequest - BossStage1::kArmorPlates);
        if (removed) {

            //ヒットストップ発生
            TriggerHitStopEffect(0);

            battleContext.arena.NotifyArmorPeeled(peelRequest);
            player.OnArmorPeeled();
        }
    }
    SetCameraFollowY(360.0f); // プレイヤーのジャンプ/ワイヤーでカメラが上に行かない
    if (GameHudEditor::IsTimeUp() || player.IsDefeated() || battleContext.stageCleared) return;
    if (battleContext.boss.IsDefeated()) {
        battleContext.boss.Update(); // コプターの撃破時落下は最後まで見せる
        if (++battleContext.clearDelayFrames >= 45) battleContext.stageCleared = true;
        return;
    }
    {
        // 【突進回避判定】ワイヤー移動していたことをボスに伝える。
        // ワイヤー使用＋突進空振りなら、復帰後にボスが長く失速する。
        const bool usingWire = player.IsGripping() || player.HasWireMomentum();
        // 【判断AIへ情報を渡す】5つの技を状況に応じて選び分ける。
        battleContext.boss.SetTargetMotion(player.GetVelocity(), player.IsGrounded(), usingWire);
        battleContext.boss.NoteWireMotion(usingWire);
        battleContext.boss.Update();
        // 更新前のボスGP座標を描画に使わない。ボス本体移動後の座標へ同期。
        // プレイヤーの接続・巻き取り物理やボスAIの計算は変更しない。
        SyncBossGripTargets(false);
        // 巻き直しAI：最も緩んだネジを優先。ボスはこの間攻撃しない。
        if (battleContext.screwRepairCooldownFrames > 0)--battleContext.screwRepairCooldownFrames;
        if (battleContext.repairWasActive && !battleContext.boss.IsRepairingScrew()) {
            battleContext.screwRepairCooldownFrames = BossCombatEditor::values.repairCooldown;
            battleContext.repairWasActive = false;
        }
        if (!battleContext.boss.IsRepairingScrew() && battleContext.screwRepairCooldownFrames == 0 && battleContext.boss.GetState() == BossState::Idle) {
            float mostLoose = 0.0f;
            const int targetScrew = battleContext.arena.MostLooseScrew(&mostLoose);
            if (targetScrew >= 0 && mostLoose >= BossCombatEditor::values.repairThreshold)
                battleContext.repairWasActive = battleContext.boss.BeginScrewRepair(targetScrew);
        }
        if (battleContext.boss.IsRepairingScrew()) {
            const int targetScrew = battleContext.boss.RepairScrewIndex();
            if (battleContext.arena.ScrewExists(targetScrew))
                battleContext.arena.TightenScrew(targetScrew, BossCombatEditor::values.repairPerFrame);
        }
        if (battleContext.boss.IsCoasterStage()) {
            Stage2::UpdateGimmicks(battleContext.arena, battleContext.boss);
            // No automatic counter damage: stage 2 now shares the
            // same loosen -> two-arm peel progression as stage 1.
        }
        // Update中に墜落→連打へ切り替わるので、カメラ状態を毎フレーム更新。
        SetBossMashCamera(battleContext.boss.IsMashActive(), battleContext.boss.GetPosition().x, battleContext.boss.GetPosition().y);
        // 【スタン復帰の吹き飛ばし】1回だけ起きるイベントを消費。
        // プレイヤーは下へ落下。ワイヤーを使えば着地後に即復帰できる。
        if (battleContext.boss.ConsumeWakeBlast()) {
            const Vector2 d = PortalMath::Sub(player.GetPosition(), battleContext.boss.GetPosition());
            if (PortalMath::Length(d) < BossStage1::kWakeBlastRadius) {
                player.ApplyWakeBlast(battleContext.boss.GetPosition(), BossStage1::kWakeBlastPushX,
                    BossStage1::kWakeBlastPushDown);
            }
        }
        // 【攻撃の区別】ボス外装は専用ポイントを狙って引いた時だけ剥がれる。
        // 普通の攻撃は外装への衝撃、スピンはカウンター、剥がした後はコアHPへダメージ。
        // 装甲剥離前は機体全体、剥離後はエディターで調整した赤いコアの円判定。
        const PortalAttackKind attack = battleContext.boss.IsMashActive() ? PortalAttackKind::None : player.HitTestEnemy(
            battleContext.boss.IsCoreExposed() ? battleContext.boss.GetCoreHitPoint() : battleContext.boss.GetPosition(),
            battleContext.boss.IsCoreExposed() ? battleContext.boss.GetCoreHitRadius() : Boss::kHitRadius);
        int damage = 0;
        // 【分岐表】現在の状態/攻撃種類に応じて、実行する処理を分ける。
        switch (attack) {
        case PortalAttackKind::Normal:damage = battleContext.liveBattleConfig.normalDamage;break;
        case PortalAttackKind::Spin:damage = battleContext.liveBattleConfig.spinDamage;break;
        case PortalAttackKind::Grapple:damage = battleContext.liveBattleConfig.grappleDamage;break;
        case PortalAttackKind::None:break;
        }
        if (damage > 0 && battleContext.boss.TakeDamage(damage,
            attack == PortalAttackKind::Grapple,
            attack == PortalAttackKind::Spin,
            player.HasWireMomentum())) {
            // 【触手が当たった感触】攻撃成功時だけ反動/色/ポーズを短く変える。
            // ワイヤー移動は止めないので、回避→反撃が1つの動きになる。
            player.OnAttackHit(attack);
        }
        // 突進・プレス・ガラクタ・コマ・ヨーヨーにもそれぞれ被弾判定がある。
        if (GameHudEditor::UsesHp() && !battleContext.boss.IsDefeated()) {
            Vector2 source = {};
            int hit = battleContext.boss.TryHitPlayer(player.GetPosition(), 24.0f, &source);
            if (hit <= 0 && battleContext.boss.IsCoasterStage()) hit = Stage2::HitPlayer(battleContext.arena, player.GetPosition(), &source);
            if (hit > 0) {
                const int adjustedDamage = (hit * battleContext.liveBattleConfig.incomingDamagePercent + 50) / 100;
                if (adjustedDamage > 0) player.TakeDamage(adjustedDamage, source);
            }
        }
    }
    // 時間制のみでは残りHPをゲームオーバー条件に使用しない。
    if (!GameHudEditor::UsesHp())
        player.SetCurrentHpForEditor(player.GetMaxHp());
}
