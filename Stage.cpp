// ============================================================================
// 【このファイルの役割】Stage.cpp — ボス戦の司令塔
// PortalArena(プレイヤー)とBoss(敵)をつなぐ。ネジ解除、ダメージ、カメラ、クリア判定、描画順を決める。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

#include "Stage.h"
#include "Boss_coptar.h"
#include "PortalArena.h"
#include "PortalConfig.h"
#include "Camera.h"
#include <Novice.h>
#include <dinput.h>
#include <cstring>
#include "BossBattleSettings.h"
#include "FinalTuningEditor.h"
#include "AssetEditor.h"
#include "PortalMath.h"
#include "CreatorGameRuntime.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ================================================================
// 【第1ステージ】「突進の予兆→ワイヤー回避→失速時に反撃→外装/コアを破壊」。
// 現在は左スティックで移動、右スティックで8方向触手攻撃。Jはキーボード用。AIはBoss、プレイヤーはPortalArenaが管理。
// 画面の派手さは運動/停止/画面揺れを使い、オブジェクト描画は最小限。
// ================================================================
static CreatorGameRuntime::Runtime creatorRuntime;
static Boss boss;
static PortalArena arena;
enum class StageId { Copter = 1, Coaster = 2 };
static StageId currentStage = StageId::Copter;
static bool stageCleared = false;
static BossBattleSettings liveBattleConfig;
static int reloadIndicatorFrames = 0;
static bool f8Prev = false;
static bool f7Prev = false;
static bool spacePrev = false;
static int tutorialFrame = 0;
static int clearDelayFrames = 0; // ボス撃破後、短い倒れる演出のあとクリア
static constexpr Vector2 kGravityScale = { 0.0f,-1.2f };
static void ResetStageBattle(StageId stage) {
    currentStage = stage;
    arena.Initialize();
    arena.GetPlayer().SetMaxHp(liveBattleConfig.playerMaxHp);
    arena.SetBattleSettings(liveBattleConfig);
    boss.SetBattleSettings(liveBattleConfig);
    boss.Initialize({ liveBattleConfig.bossSpawn.x, liveBattleConfig.bossSpawn.y });
    boss.SetTarget(&arena.GetPlayerPosition());
    const bool coaster = stage == StageId::Coaster;
    if (coaster) boss.StartCoasterStage();
    arena.SetCoasterStage(coaster);
    stageCleared = false;
    tutorialFrame = 0;
    clearDelayFrames = 0;
    SetBossMashCamera(false, 640.0f, 360.0f);
    SetCameraFollowY(360.0f);
}

#ifdef USE_IMGUI
// 旧F5/F6/F9を1つのウィンドウへ統合する。
static bool showGameEditor = false;
static int editorRequestedTab = 0; // 0:調整 1:画像 2:詳細 3:デバッグ
static bool f5Prev = false;
static bool f6Prev = false;
static bool f9Prev = false;
static char gameEditorStatus[256] = "保存ボタンで画像とボス設定をまとめて記録できます。";
static bool battleEditorDirty = false;
static char battleEditorMessage[128] = "Change values, then Save";
static bool ResetBossBattleFromEditor() {
    if (!liveBattleConfig.Valid()) return false;
    ResetStageBattle(currentStage);
    return true;
}

// すべてワールド座標(+Yが上)。UIの入力値はBossBattleSettingsで検証してから反映。
static void DrawBossBattleEditorContents() {
    ImGui::TextUnformatted("Edit while playing / F8: reload saved settings");
    if (ImGui::CollapsingHeader("Creator editor integration")) {
        ImGui::Checkbox("Show stage tiles", &creatorRuntime.showStage);
        ImGui::Checkbox("Show collision outlines", &creatorRuntime.showCollision);
        ImGui::Checkbox("Show UI widgets", &creatorRuntime.showCreatorUI);
        ImGui::Checkbox("Show effect preview", &creatorRuntime.showEffects);
        ImGui::Checkbox("Show animation preview", &creatorRuntime.showAnimation);
        ImGui::Checkbox("Show gimmick preview", &creatorRuntime.showGimmicks);
        if (ImGui::Button("Reload creator stage + hitboxes")) creatorRuntime.Reload();
        ImGui::TextWrapped("F8 reloads TXT and stage data. Animation/effect/UI/gimmick preview uses generated CreatorRuntimeData.h (rebuild after export).");
    }
    ImGui::Separator();
    bool changed = false;
    auto valueF = [&](const char* label, float* value, float min, float max) {
        if (ImGui::SliderFloat(label, value, min, max, "%.2f")) changed = true;
        };
    auto valueI = [&](const char* label, int* value, int min, int max) {
        if (ImGui::SliderInt(label, value, min, max)) changed = true;
        };
    if (ImGui::CollapsingHeader("0. HP and damage", ImGuiTreeNodeFlags_DefaultOpen)) {
        valueI("Boss maximum HP", &liveBattleConfig.bossMaxHp, 1, 10000);
        valueI("Player maximum HP", &liveBattleConfig.playerMaxHp, 1, 1000);
        valueI("Stage 1 armor rebuild HP (%)", &liveBattleConfig.phase2HpPercent, 1, 99);
        ImGui::Separator();
        valueI("Normal attack damage", &liveBattleConfig.normalDamage, 0, 500);
        valueI("Spin attack damage", &liveBattleConfig.spinDamage, 0, 500);
        valueI("Grapple damage", &liveBattleConfig.grappleDamage, 0, 500);
        valueI("Mash damage / hit", &liveBattleConfig.mashDamage, 0, 500);
        valueI("Player received damage (%)", &liveBattleConfig.incomingDamagePercent, 0, 500);
        ImGui::TextWrapped("Max HP is fully applied on Restart battle. Attack damage updates immediately.");
    }
    if (ImGui::CollapsingHeader("8. Test controls")) {
        int bossHpEditor = boss.GetHp();
        int playerHpEditor = arena.GetPlayer().GetHp();
        ImGui::Text("Boss: %d / %d", bossHpEditor, boss.GetMaxHp());
        ImGui::Text("Player: %d / %d", playerHpEditor, arena.GetPlayer().GetMaxHp());
        if (ImGui::SliderInt("Set boss current HP", &bossHpEditor, 1, boss.GetMaxHp()))
            boss.SetCurrentHpForEditor(bossHpEditor);
        if (ImGui::SliderInt("Set player current HP", &playerHpEditor, 1, arena.GetPlayer().GetMaxHp()))
            arena.GetPlayer().SetCurrentHpForEditor(playerHpEditor);
        if (ImGui::Button("Set boss HP near armor rebuild")) {
            const int threshold = (boss.GetMaxHp() * liveBattleConfig.phase2HpPercent) / 100;
            boss.SetCurrentHpForEditor(threshold < 1 ? 1 : threshold);
        }
        ImGui::TextWrapped("Current HP test only. In Stage 1, hit the boss after setting HP to test armor rebuild. Saved TXT stores maximum HP and damage.");
    }
    if (ImGui::CollapsingHeader("1. Boss movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        valueF("Fly speed", &liveBattleConfig.flightSpeed, 0.1f, 15.0f);
        valueF("Fly speed (phase 2)", &liveBattleConfig.flightSpeedPhase2, 0.1f, 20.0f);
        valueI("Change destination frames", &liveBattleConfig.flightInterval, 10, 240);
        valueI("Change destination (phase 2)", &liveBattleConfig.flightIntervalPhase2, 10, 240);
        if (ImGui::DragFloat2("Spawn position", &liveBattleConfig.bossSpawn.x, 1.0f, 30.0f, 1250.0f)) changed = true;
        valueI("Wait before attack", &liveBattleConfig.idleFrames, 8, 180);
    }
    if (ImGui::CollapsingHeader("2. Rush attack")) {
        valueF("Rush max speed", &liveBattleConfig.rushSpeed, 5.0f, 50.0f);
        valueF("Rush acceleration", &liveBattleConfig.rushAccel, 0.1f, 10.0f);
        valueI("Rush warning (frames)", &liveBattleConfig.rushWarning, 8, 180);
        valueI("Rush counter time", &liveBattleConfig.rushDizzy, 0, 240);
    }
    if (ImGui::CollapsingHeader("3. Shots and press")) {
        valueF("Shot prediction", &liveBattleConfig.shotPredict, 0, 40);
        valueI("Shot warning", &liveBattleConfig.shotWarning, 8, 180);
        valueF("Press max speed", &liveBattleConfig.pressSpeed, 5, 80);
        valueF("Press acceleration", &liveBattleConfig.pressAccel, 0.1f, 10);
        valueI("Press warning", &liveBattleConfig.pressWarning, 8, 180);
        valueI("Press recovery", &liveBattleConfig.pressStun, 0, 180);
    }
    if (ImGui::CollapsingHeader("4. Top and yoyo")) {
        valueF("Top speed", &liveBattleConfig.topInitialSpeed, 3, 45);
        valueI("Top duration (legacy)", &liveBattleConfig.topDuration, 30, 300);
        valueF("Top gravity", &liveBattleConfig.topGravity, 0.1f, 8.0f);
        valueF("Top floor bounce", &liveBattleConfig.topBounceHeight, 0.0f, 30.0f);
        valueF("Top acceleration after bounce", &liveBattleConfig.topReturnAccel, 0.005f, 0.5f);
        valueF("Top hit radius", &liveBattleConfig.topHitRadius, 20.0f, 128.0f);
        valueI("Top failsafe lifetime", &liveBattleConfig.topMaxLifetime, 180, 1800);
        ImGui::TextWrapped("Top: DROP -> FLOOR ROLL -> ONE WALL BOUNCE -> RETURN. Old duration no longer cuts it short.");
        if (ImGui::Button("Test Top NOW")) boss.TriggerAttackForEditor(BossAttack::Top);
        ImGui::SameLine();
        if (ImGui::Button("Test YoYo NOW")) boss.TriggerAttackForEditor(BossAttack::YoYo);
        valueF("Yoyo range", &liveBattleConfig.yoyoRange, 100, 900);
        valueI("Yoyo duration", &liveBattleConfig.yoyoDuration, 30, 300);
        valueI("Toy warning", &liveBattleConfig.toyWarning, 8, 180);
    }
    if (ImGui::CollapsingHeader("5. Background grips")) {
        valueI("Background grip count", &liveBattleConfig.anchorCount, 1, BossBattleSettings::kMaxAnchors);
        for (int i = 0; i < liveBattleConfig.anchorCount; ++i) {
            ImGui::PushID(i);
            if (ImGui::DragFloat2("Grip XY", &liveBattleConfig.anchors[i].x, 1.0f, 30.0f, 1250.0f)) changed = true;
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("6. Boss parts - grip offsets")) {
        const char* partNames[] = { "Body", "Propeller L", "Propeller Center", "Propeller R" };
        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(100 + i);
            if (ImGui::DragFloat2(partNames[i], &liveBattleConfig.partGrips[i].x, 1.0f, -320.0f, 320.0f)) changed = true;
            ImGui::PopID();
        }
        const char* extraNames[] = { "Jet 1", "Jet 2", "Jet 3", "Jet 4", "Pipe", "Eye" };
        for (int i = 0; i < 6; ++i) {
            ImGui::PushID(200 + i);
            if (ImGui::DragFloat2(extraNames[i], &liveBattleConfig.accessoryGrips[i].x, 1.0f, -320.0f, 320.0f)) changed = true;
            ImGui::PopID();
        }
        ImGui::TextWrapped("Offsets are relative to boss center; visual PNG positions stay unchanged.");
    }
    if (ImGui::CollapsingHeader("7. Core and original armor hooks")) {
        if (ImGui::DragFloat2("Core offset XY", &liveBattleConfig.coreOffset.x, 1.0f, -100.0f, 100.0f)) changed = true;
        valueF("Core hit radius", &liveBattleConfig.coreRadius, 24, 120);
        for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) {
            ImGui::PushID(300 + i);
            if (ImGui::DragFloat2("Legacy hook", &liveBattleConfig.armorHooks[i].x, 1.0f, -128.0f, 128.0f)) changed = true;
            ImGui::PopID();
        }
        ImGui::TextWrapped("Legacy hooks are kept for older editor data; active boss grips are in section 6.");
    }
    if (changed) {
        battleEditorDirty = true;
        if (liveBattleConfig.Valid()) {
            boss.SetBattleSettings(liveBattleConfig);
            arena.SetBattleSettings(liveBattleConfig);
            arena.GetPlayer().SetMaxHp(liveBattleConfig.playerMaxHp);
        }
    }
    if (!liveBattleConfig.Valid()) ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "Invalid setting; change it back before saving");
    if (ImGui::Button("Save TXT")) {
        if (liveBattleConfig.Save(AssetEditor::BossSettingsPath().c_str())) { battleEditorDirty = false; strcpy_s(battleEditorMessage, "Saved bossbattle_stage1.txt"); }
        else strcpy_s(battleEditorMessage, "Save failed: invalid values/path");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load TXT")) {
        BossBattleSettings loaded;
        if (loaded.Load(AssetEditor::BossSettingsPath().c_str())) { liveBattleConfig = loaded; ResetBossBattleFromEditor(); battleEditorDirty = false; strcpy_s(battleEditorMessage, "Loaded and restarted"); }
        else strcpy_s(battleEditorMessage, "Load failed; current values kept");
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart battle")) {
        if (ResetBossBattleFromEditor()) strcpy_s(battleEditorMessage, "Battle restarted");
        else strcpy_s(battleEditorMessage, "Fix invalid values first");
    }
    if (ImGui::Button("Restore defaults")) {
        liveBattleConfig = BossBattleSettings{};
        ResetBossBattleFromEditor();
        battleEditorDirty = true;
        strcpy_s(battleEditorMessage, "Defaults applied; Save TXT to persist");
    }
    ImGui::SameLine(); ImGui::TextUnformatted(battleEditorDirty ? "* Unsaved" : "Saved / unchanged");
    ImGui::TextWrapped("%s", battleEditorMessage);
}
// 既存のゲーム設定と画像設定を一つの画面から扱う。
// 操作はF5:調整 / F6:画像 / F9:詳細 / F8:バランス再読込。
static void DrawGameEditor() {
    AssetEditor::opened = false; // プレビューは画像タブを開いている時だけ描画する
    if (!showGameEditor) return;
    ImGui::SetNextWindowPos(ImVec2(12, 18), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(610, 685), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ゲーム調整エディター###UnifiedGameEditor", &showGameEditor)) {
        ImGui::End(); return;
    }
    ImGui::TextUnformatted("F5 最終調整   /   F6 画像・アニメ   /   F9 詳細設定");
    ImGui::Text("ステージ%d  ボスHP %d / %d  プレイヤーHP %d / %d",
        int(currentStage), boss.GetHp(), boss.GetMaxHp(),
        arena.GetPlayer().GetHp(), arena.GetPlayer().GetMaxHp());

    // 「まとめて保存」は画像・アニメとボス設定の両方を記録する。
    // 失敗時は失敗を明示し、保存できたと偽らない。
    if (ImGui::Button("まとめて保存")) {
        const bool assetOk = AssetEditor::Save();
        const bool bossOk = liveBattleConfig.Valid() &&
            liveBattleConfig.Save(AssetEditor::BossSettingsPath().c_str());
        std::snprintf(gameEditorStatus, sizeof(gameEditorStatus),
            "保存: 画像=%s / ボス=%s", assetOk ? "OK" : "失敗", bossOk ? "OK" : "失敗");
        if (bossOk) { battleEditorDirty = false; FinalTuningEditor::dirty = false; }
    }
    ImGui::SameLine();
    if (ImGui::Button("保存設定を再読込")) {
        AssetEditor::Initialize();
        BossBattleSettings loaded;
        const bool bossOk = loaded.Load(AssetEditor::BossSettingsPath().c_str());
        if (bossOk) { liveBattleConfig = loaded; ResetStageBattle(currentStage); }
        std::snprintf(gameEditorStatus, sizeof(gameEditorStatus),
            "再読込: 画像 %s / ボス %s", AssetEditor::status,
            bossOk ? "OK（戦闘を再開始）" : "保存設定なし／失敗");
    }
    ImGui::SameLine();
    if (ImGui::Button("戦闘をやり直す")) {
        if (liveBattleConfig.Valid()) ResetStageBattle(currentStage);
    }
    ImGui::TextWrapped("%s", gameEditorStatus);
    if (!liveBattleConfig.Valid())
        ImGui::TextColored(ImVec4(1, .4f, .4f, 1), "ボスの設定値が無効です。保存前に修正してください。");
    ImGui::Separator();

    if (ImGui::BeginTabBar("GameEditTabs")) {
        const ImGuiTabItemFlags tab0 = editorRequestedTab == 0 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab1 = editorRequestedTab == 1 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab2 = editorRequestedTab == 2 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab3 = editorRequestedTab == 3 ? ImGuiTabItemFlags_SetSelected : 0;
        // フォーカスしたいタブは一回だけ要求する（次フレームのタブ戻りを防ぐ）。
        editorRequestedTab = -1;
        if (ImGui::BeginTabItem("最終調整", nullptr, tab0)) {
            ImGui::BeginChild("##TuningArea", ImVec2(0, 0), false);
            const FinalTuningEditor::Actions action = FinalTuningEditor::DrawContents(
                liveBattleConfig, boss.GetHp(), arena.GetPlayer().GetHp(), currentStage == StageId::Coaster);
            if (action.reset) { liveBattleConfig = BossBattleSettings{}; FinalTuningEditor::dirty = true; }
            if (action.reload) {
                BossBattleSettings loaded;
                if (loaded.Load(AssetEditor::BossSettingsPath().c_str())) {
                    liveBattleConfig = loaded; ResetStageBattle(currentStage);
                    FinalTuningEditor::dirty = false;
                }
            }
            if (liveBattleConfig.Valid()) {
                if (action.apply || action.save || action.restart) {
                    boss.SetBattleSettings(liveBattleConfig);
                    arena.SetBattleSettings(liveBattleConfig);
                    arena.GetPlayer().SetMaxHp(liveBattleConfig.playerMaxHp);
                }
                if (action.save) {
                    const bool ok = liveBattleConfig.Save(AssetEditor::BossSettingsPath().c_str());
                    if (ok) FinalTuningEditor::dirty = false;
                    std::snprintf(gameEditorStatus, sizeof(gameEditorStatus), "ボス設定の保存: %s", ok ? "OK" : "失敗");
                }
                if (action.restart) ResetStageBattle(currentStage);
                if (action.stage1) ResetStageBattle(StageId::Copter);
                if (action.stage2) ResetStageBattle(StageId::Coaster);
                if (action.testAttack >= 0 && action.testAttack < 5) {
                    const BossAttack attack[] = { BossAttack::Rush, BossAttack::Press,
                        BossAttack::Shot, BossAttack::Top, BossAttack::YoYo };
                    boss.TriggerAttackForEditor(attack[action.testAttack]);
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("画像・アニメ", nullptr, tab1)) {
            AssetEditor::opened = true;
            ImGui::BeginChild("##ImageArea", ImVec2(0, 0), false);
            AssetEditor::DrawPanel();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("詳細ボス設定", nullptr, tab2)) {
            ImGui::BeginChild("##BossAdvanced", ImVec2(0, 0), false);
            DrawBossBattleEditorContents();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("デバッグ・保存先", nullptr, tab3)) {
            ImGui::TextUnformatted("リソースを再起動後にも維持するには「まとめて保存」を押します。");
            ImGui::TextWrapped("画像: %s", AssetEditor::settingsFile.c_str());
            ImGui::TextWrapped("ボス: %s", AssetEditor::BossSettingsPath().c_str());
            ImGui::TextWrapped("直近の読み込み: %s", AssetEditor::status);
            ImGui::Separator();
            ImGui::Checkbox("ステージ表示", &creatorRuntime.showStage);
            ImGui::Checkbox("当たり判定表示", &creatorRuntime.showCollision);
            ImGui::Checkbox("UI表示", &creatorRuntime.showCreatorUI);
            ImGui::Checkbox("エフェクト表示", &creatorRuntime.showEffects);
            ImGui::Checkbox("アニメ確認", &creatorRuntime.showAnimation);
            ImGui::Checkbox("ギミック表示", &creatorRuntime.showGimmicks);
            if (ImGui::Button("ステージデータを再読込")) creatorRuntime.Reload();
            ImGui::TextUnformatted("※ F8はボス設定とステージデータを再読込します。");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
#endif


// ------------------------------------------------------------------------
// 【関数】InitStage()
// 何をする？ ボス戦の設定読込、プレイヤー・ボス初期化、カメラ位置を整える。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void InitStage(void) {
    // 設定ファイルが無い場合は前回正常に読めた値／初期値で続行。
    // 【注目】【設定読込】エディターの設定が無い場合は既定値を使う。
    liveBattleConfig.Load(AssetEditor::BossSettingsPath().c_str());
    creatorRuntime.Reload();
    ResetStageBattle(StageId::Copter);
}

// ------------------------------------------------------------------------
// 【関数】UpdateStage()
// 何をする？ プレイヤーとボスを更新し、ネジ解除・連打・ダメージ・クリアを接続する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void UpdateStage(void) {
    // 【F8】エディターで保存したtxtをゲーム中に読み直し、同じ配置から再テスト。
    // キー押しっぱなしによる連続リロードを防ぐためエッジ判定。
    char liveKeys[256] = {};Novice::GetHitKeyStateAll(liveKeys);
#ifdef USE_IMGUI
    const bool f5Now = liveKeys[DIK_F5] != 0;
    if (f5Now && !f5Prev) { showGameEditor = !showGameEditor; editorRequestedTab = 0; }
    f5Prev = f5Now;
    const bool f6Now = liveKeys[DIK_F6] != 0;
    if (f6Now && !f6Prev) { showGameEditor = true; editorRequestedTab = 1; }
    f6Prev = f6Now;
    const bool f9Now = liveKeys[DIK_F9] != 0;
    if (f9Now && !f9Prev) { showGameEditor = true; editorRequestedTab = 2; }
    f9Prev = f9Now;
#endif
    const bool f7Now = liveKeys[DIK_F7] != 0;
    const bool spaceNow = liveKeys[DIK_SPACE] != 0;
    const bool enterCoaster = (f7Now && !f7Prev && currentStage == StageId::Copter) ||
        (spaceNow && !spacePrev && stageCleared && currentStage == StageId::Copter);
    f7Prev = f7Now;
    spacePrev = spaceNow;
    if (enterCoaster) {
        ResetStageBattle(StageId::Coaster);
        return;
    }
    const bool f8Now = liveKeys[DIK_F8] != 0;
    if (f8Now && !f8Prev) {
        BossBattleSettings next;
        if (next.Load(AssetEditor::BossSettingsPath().c_str())) { liveBattleConfig = next; creatorRuntime.Reload(); ResetStageBattle(currentStage); reloadIndicatorFrames = 120; }
        else reloadIndicatorFrames = -120;
    }
    f8Prev = f8Now;
    if (reloadIndicatorFrames > 0)--reloadIndicatorFrames;
    if (reloadIndicatorFrames < 0)++reloadIndicatorFrames;
    creatorRuntime.Update();
    // 【重要】ボス外装の四隅ネジ4点を移動杭とは別の目標として渡す。
    // 各ネジは右スティックで狙って接続すると外せる。プレイヤー自身はボスへ引き寄せられない。
    Vector2 armorPoints[BossStage1::kArmorPlates];
    bool armorExists[BossStage1::kArmorPlates];
    // 【繰り返し】対象（触手・ネジ・弾など）を順に確認する。
    for (int i = 0;i < BossStage1::kArmorPlates;++i) {
        armorPoints[i] = boss.GetArmorGripPoint(i);
        armorExists[i] = boss.IsArmorAttached(i);
    }
    arena.SetBossArmorTargets(armorPoints, armorExists);
    Vector2 extraBossPoints[Boss::kAccessoryGripCount];
    bool extraBossEnabled[Boss::kAccessoryGripCount];
    for (int i = 0; i < Boss::kAccessoryGripCount; ++i) {
        extraBossPoints[i] = boss.GetAccessoryGripPoint(i);
        extraBossEnabled[i] = boss.IsAccessoryGripEnabled(i);
    }
    arena.SetBossExtraTargets(extraBossPoints, extraBossEnabled);
    if (arena.Update()) {
        ResetStageBattle(currentStage);
        return;
    }
    ++tutorialFrame;
    PortalPlayer& player = arena.GetPlayer();
    if (creatorRuntime.showCollision && creatorRuntime.PlayerInHazard(player.GetPosition()) && (tutorialFrame % 45 == 0))
        player.TakeDamage(6, player.GetPosition());
    // 【連打】攻撃が届かない距離でもスタン中は右スティック弾き直しでコアを殴れる。
    const bool mashPulse = arena.ConsumeMashPulse();
    if (boss.IsMashActive() && mashPulse) boss.RegisterMashHit();
    SetBossMashCamera(boss.IsMashActive(), boss.GetPosition().x, boss.GetPosition().y);

    // 四隅ネジは接続時に解除される。長押し張力ゲージは使わない。
    // 四隅のネジを1個ずつ外す。最後の1個で外装めくれ演出開始。
    const int peelRequest = player.ConsumeArmorPeelRequest();
    if (peelRequest >= 0 && !boss.IsDefeated()) {
        // 【外装ヒット確認】本当に剥がれた場合だけプレイヤーのポーズを強調。
        bool removed = false;
        if (peelRequest < BossStage1::kArmorPlates)
            removed = boss.TryPeelArmor(peelRequest);
        else
            removed = boss.TryPeelAccessory(peelRequest - BossStage1::kArmorPlates);
        if (removed) {
            arena.NotifyArmorPeeled(peelRequest);
            player.OnArmorPeeled();
        }
    }
    SetCameraFollowY(360.0f); // プレイヤーのジャンプ/ワイヤーでカメラが上に行かない
    if (player.IsDefeated() || stageCleared) return;
    if (boss.IsDefeated()) {
        boss.Update(); // コプターの撃破時落下は最後まで見せる
        if (++clearDelayFrames >= 45) stageCleared = true;
        return;
    }
    {
        // 【突進回避判定】ワイヤー移動していたことをボスに伝える。
        // ワイヤー使用＋突進空振りなら、復帰後にボスが長く失速する。
        const bool usingWire = player.IsGripping() || player.HasWireMomentum();
        // 【判断AIへ情報を渡す】5つの技を状況に応じて選び分ける。
        boss.SetTargetMotion(player.GetVelocity(), player.IsGrounded(), usingWire);
        boss.NoteWireMotion(usingWire);
        boss.Update();
        if (boss.IsCoasterStage()) {
            arena.UpdateStage2(boss.GetPosition(), boss.GetHp(), boss.GetMaxHp());
            // No automatic counter damage: stage 2 now shares the
            // same loosen -> two-arm peel progression as stage 1.
        }
        // Update中に墜落→連打へ切り替わるので、カメラ状態を毎フレーム更新。
        SetBossMashCamera(boss.IsMashActive(), boss.GetPosition().x, boss.GetPosition().y);
        // 【スタン復帰の吹き飛ばし】1回だけ起きるイベントを消費。
        // プレイヤーは下へ落下。ワイヤーを使えば着地後に即復帰できる。
        if (boss.ConsumeWakeBlast()) {
            const Vector2 d = PortalMath::Sub(player.GetPosition(), boss.GetPosition());
            if (PortalMath::Length(d) < BossStage1::kWakeBlastRadius) {
                player.ApplyWakeBlast(boss.GetPosition(), BossStage1::kWakeBlastPushX,
                    BossStage1::kWakeBlastPushDown);
            }
        }
        // 【攻撃の区別】ボス外装は専用ポイントを狙って引いた時だけ剥がれる。
        // 普通の攻撃は外装への衝撃、スピンはカウンター、剥がした後はコアHPへダメージ。
        // 装甲剥離前は機体全体、剥離後はエディターで調整した赤いコアの円判定。
        const PortalAttackKind attack = boss.IsMashActive() ? PortalAttackKind::None : player.HitTestEnemy(
            boss.IsCoreExposed() ? boss.GetCoreHitPoint() : boss.GetPosition(),
            boss.IsCoreExposed() ? boss.GetCoreHitRadius() : Boss::kHitRadius);
        int damage = 0;
        // 【分岐表】現在の状態/攻撃種類に応じて、実行する処理を分ける。
        switch (attack) {
        case PortalAttackKind::Normal:damage = liveBattleConfig.normalDamage;break;
        case PortalAttackKind::Spin:damage = liveBattleConfig.spinDamage;break;
        case PortalAttackKind::Grapple:damage = liveBattleConfig.grappleDamage;break;
        case PortalAttackKind::None:break;
        }
        if (damage > 0 && boss.TakeDamage(damage,
            attack == PortalAttackKind::Grapple,
            attack == PortalAttackKind::Spin,
            player.HasWireMomentum())) {
            // 【触手が当たった感触】攻撃成功時だけ反動/色/ポーズを短く変える。
            // ワイヤー移動は止めないので、回避→反撃が1つの動きになる。
            player.OnAttackHit(attack);
        }
        // 突進・プレス・ガラクタ・コマ・ヨーヨーにもそれぞれ被弾判定がある。
        if (!boss.IsDefeated()) {
            Vector2 source = {};
            int hit = boss.TryHitPlayer(player.GetPosition(), 24.0f, &source);
            if (hit <= 0 && boss.IsCoasterStage()) hit = arena.Stage2HitPlayer(player.GetPosition(), &source);
            if (hit > 0) {
                const int adjustedDamage = (hit * liveBattleConfig.incomingDamagePercent + 50) / 100;
                if (adjustedDamage > 0) player.TakeDamage(adjustedDamage, source);
            }
        }
    }
}

// ------------------------------------------------------------------------
// 【関数】DrawStage()
// 何をする？ 背景→ボス→プレイヤー→UIの順で画面を描く。後から描くものが前に出る。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void DrawStage(void) {
    // 描画順：背景・杭 → ボス → プレイヤー・触手 → HUD。
    // ボスの後に背景を描くとボスが隠れるため、この順序を守る。
    // 【注目】【描画順①】床・背景・杭を先に描く（最も奥）。
    arena.DrawWorld();
    creatorRuntime.DrawWorld();
    // 【注目】【描画順②】ボスはプレイヤーより後ろに描く。
    boss.Draw();
    // 【注目】【描画順③】プレイヤーと触手は最後に描いて手前へ。
    arena.DrawPlayer();
    arena.DrawHUD();
    creatorRuntime.DrawHUD();
    if (reloadIndicatorFrames > 0)Novice::ScreenPrintf(18, 210, "BOSS EDITOR DATA RELOADED (F8)");
    else if (reloadIndicatorFrames < 0)Novice::ScreenPrintf(18, 210, "NO VALID bossbattle_stage1.txt");
    // HPと外装は画面固定のBOX。文字もスクリーン座標なので描画数が増えない。
    const int playerHp = arena.GetPlayer().GetHp();
    const int bossHp = boss.GetHp();
    Novice::DrawBox(17, 92, 284, 15, 0, 0x222735FF, kFillModeSolid);
    Novice::DrawBox(18, 93, static_cast<int>(282.0f * playerHp / arena.GetPlayer().GetMaxHp()), 13, 0,
        0x46DBADFF, kFillModeSolid);
    Novice::DrawBox(974, 92, 286, 15, 0, 0x222735FF, kFillModeSolid);
    Novice::DrawBox(975, 93, static_cast<int>(284.0f * bossHp / boss.GetMaxHp()), 13, 0,
        boss.IsEnraged() ? 0xFF9A54FF : 0xD65B67FF, kFillModeSolid);
    Novice::ScreenPrintf(18, 114, "PLAYER HP %d/%d", playerHp, arena.GetPlayer().GetMaxHp());
    Novice::ScreenPrintf(970, 114, "COPTER CORE HP %d/%d", bossHp, boss.GetMaxHp());
    Novice::ScreenPrintf(970, 134, "ARMOR %d/%d", boss.GetArmorPlates(), boss.GetMaxArmorPlates());

    // 【ゲームの流れ】操作を覚えるため、目的を画面上に常時短文で表示する。
    // 攻略順：ジェット・パイプ → プロペラ3本 → 本体と目は残して弱点攻撃。
    bool hasAccessory = false;
    for (int i = 0; i < Boss::kAccessoryGripCount - 1; ++i) {
        if (boss.IsAccessoryGripEnabled(i)) { hasAccessory = true; break; }
    }
    if (boss.IsCoasterStage() && !boss.IsDefeated()) {
        Novice::ScreenPrintf(18, 145, "STAGE 2: COASTER / GRAPPLE ABOVE THE RUSH!");
        if (boss.GetState() == BossState::Dizzy)
            Novice::ScreenPrintf(18, 168, "COUNTER NOW! AIM FOR THE EYE!");
    }
    else if (hasAccessory) {
        Novice::ScreenPrintf(18, 145, "STEP 1: REMOVE JETS AND PIPES!");
    }
    else if (boss.GetArmorPlates() > 0) {
        Novice::ScreenPrintf(18, 145, "STEP 2: REMOVE THREE PROPELLERS!");
    }
    else if (!boss.IsDefeated()) {
        Novice::ScreenPrintf(18, 145, "STEP 3: BODY AND EYE REMAIN! ATTACK THE EYE!");
    }
    else {
        Novice::ScreenPrintf(18, 145, "COPTER DOWN! STAGE CLEAR IN A MOMENT!");
    }
    // 【読みやすさ優先】技ごとに避け方を教える。予兆だけ1行表示、追加BOXゼロ。
    // 単調な攻撃ではなく「予兆を読む→対処を選ぶ」遊びにする。
    if (boss.GetState() == BossState::Warning) {
        const char* guide = "DODGE!";
        // 【分岐表】現在の状態/攻撃種類に応じて、実行する処理を分ける。
        switch (boss.GetSelectedAttack()) {
        case BossAttack::Rush: guide = boss.IsCoasterStage() ? "RUSH: GRAPPLE UP NOW!" : "RUSH: FLOOR LANE - JUMP / WIRE!";break;
        case BossAttack::Shot: guide = "SHOT: TWO TRIPLE VOLLEYS - KEEP MOVING!";break;
        case BossAttack::Press:guide = "PRESS: LEAVE THE RED FLOOR MARK!";break;
        case BossAttack::Top:  guide = "TOP: JUMP OVER THE SPINNING TOP!";break;
        case BossAttack::YoYo: guide = "YOYO: OUT AND BACK - CHANGE HEIGHT!";break;
        }
        Novice::ScreenPrintf(18, 168, "%s", guide);
    }
    // 【連打タイム】既存ScreenPrintfとBOX2枚だけのUI。追加パーティクルなし。
    if (boss.IsMashActive()) {
        const int hits = boss.GetMashCount();
        const int remaining = boss.GetMashTimeLeft();
        const int bar = remaining * 320 / BossStage1::kMashTimeFrames;
        Novice::DrawBox(430, 145, 420, 106, 0, 0x131526D9, kFillModeSolid);
        Novice::ScreenPrintf(492, 160, "RENDA!! RIGHT STICK / J !!");
        Novice::ScreenPrintf(515, 188, "COMBO %d HITS", hits);
        Novice::DrawBox(480, 215, 320, 12, 0, 0x292E44FF, kFillModeSolid);
        Novice::DrawBox(480, 215, bar, 12, 0, 0xFFD35CFF, kFillModeSolid);
    }
    else if (boss.IsArmorFalling()) {
        Novice::ScreenPrintf(490, 157, "ARMOR BROKEN! COPTER FALLING!");
    }
    if (boss.IsArmorStun()) Novice::ScreenPrintf(975, 155, "ARMOR OPEN! MASH NOW!");
    else if (boss.IsWaking()) Novice::ScreenPrintf(975, 155, "WAKE BLAST! FALL BACK!");
    else if (boss.IsDizzy())
        Novice::ScreenPrintf(975, 155, "WIRE DODGE SUCCESS! COUNTER CHANCE!");
    else if (boss.IsWeakPointOpen())
        Novice::ScreenPrintf(975, 155, "CORE BONUS DAMAGE!");
    if (stageCleared) {
        Novice::DrawBox(370, 258, 545, 140, 0, 0x161B27DD, kFillModeSolid);
        Novice::ScreenPrintf(555, 300, "STAGE %d CLEAR!", static_cast<int>(currentStage));
        if (currentStage == StageId::Copter)
            Novice::ScreenPrintf(475, 330, "SPACE: STAGE 2    R: RETRY STAGE 1");
        else
            Novice::ScreenPrintf(518, 330, "R: RETRY STAGE 2");
    }
    else if (arena.GetPlayer().IsDefeated()) {
        Novice::DrawBox(370, 258, 545, 140, 0, 0x161B27DD, kFillModeSolid);
        Novice::ScreenPrintf(567, 300, "GAME OVER");
        Novice::ScreenPrintf(518, 330, "PRESS R TO RETRY");
    }
    else if (boss.IsDefeated()) {
        Novice::ScreenPrintf(513, 180, "COPTER DOWN! GREAT FIGHT!");
    }
#ifdef _DEBUG
    const char* attack = "SHOT";
    // 【分岐表】現在の状態/攻撃種類に応じて、実行する処理を分ける。
    switch (boss.GetSelectedAttack()) {
    case BossAttack::Press:attack = "PRESS";break;
    case BossAttack::Shot:attack = "SHOT";break;
    case BossAttack::Rush:attack = "RUSH";break;
    case BossAttack::Top:attack = "TOP";break;
    case BossAttack::YoYo:attack = "YOYO";break;
    }
    Novice::ScreenPrintf(18, 189, "AI attack: %s, cycle: %d", attack, boss.GetAttackCount());
#endif
#ifdef USE_IMGUI
    DrawGameEditor();
#endif
}
// ------------------------------------------------------------------------
// 【関数】GetGravityScale()
// 何をする？ このステージで使う重力方向のベクトルを返す。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
Vector2 GetGravityScale(void) { return kGravityScale; }
