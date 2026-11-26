// ============================================================================
// 【このファイルの役割】Stage.cpp — ボス戦の司令塔
// PortalArena(プレイヤー)とBoss(敵)をつなぐ。ネジ解除、ダメージ、カメラ、クリア判定、描画順を決める。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

#include "Stage.h"
#include "Texture.h"
#include "Boss_coptar.h"
#include "PortalArena.h"
#include "PortalConfig.h"
#include "Camera.h"
#include <Novice.h>
#include <dinput.h>
#include <cstring>
#include <string>
#include <algorithm>
#include "BossBattleSettings.h"
#include "FinalTuningEditor.h"
#include "PlayerTuning.h"
#include "AssetEditor.h"
#include "BossPartsEditor.h"
#include "Stage2Tuning.h"
#include "BossCombatEditorSettings.h"
#include "PortalDraw.h"
#include "MiniUnityEditor.h"
#include "StageCollision.h"
#include "CameraBackgroundTuning.h"
#include "PortalMath.h"
#include "CreatorGameRuntime.h"
#include "GameHudEditor.h"
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
static int screwRepairCooldownFrames = 0;
static bool repairWasActive = false;
static char attackTestMessage[128] = "テスト待機中";
// 通常時はガイド文字列を表示せず、HPと実戦の警告だけ残す。
static bool showDebugOverlay = false;
static int clearDelayFrames = 0; // ボス撃破後、短い倒れる演出のあとクリア
static constexpr Vector2 kGravityScale = { 0.0f,-1.2f };
static void ResetStageBattle(StageId stage) {
    currentStage = stage;
    GameHudEditor::SelectAndReset(int(stage));
    CameraBackgroundTuning::SelectStage(int(stage));
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
    MiniUnityEditor::ResetRuntime();
    clearDelayFrames = 0;
    screwRepairCooldownFrames = BossCombatEditor::values.repairCooldown;
    repairWasActive = false;
    SetBossMashCamera(false, 640.0f, 360.0f);
    SetCameraFollowY(360.0f);
}

// IMGUI無効ビルドでもステージ2の保存値を読めるよう、共通ヘルパーは外側に置く。
static std::string CameraBGFilePath() {
    AssetEditor::FixSettingsLocation();
    return AssetEditor::resourceRoot + "\\camera_background_settings.txt";
}

static std::string PlayerTuningFilePath() {
    AssetEditor::FixSettingsLocation();
    return AssetEditor::resourceRoot + "\\player_tuning_settings.txt";
}

static std::string BossCombatFilePath() {
    AssetEditor::FixSettingsLocation();
    return AssetEditor::resourceRoot + "\\boss_combat_editor_settings.txt";
}
static std::string GameHudFilePath() {
    AssetEditor::FixSettingsLocation();
    return AssetEditor::resourceRoot + "\\game_hud_settings.txt";
}
static std::string Stage2FilePath() {
    AssetEditor::FixSettingsLocation();
    return AssetEditor::resourceRoot + "\\stage2_trajectory.txt";
}

#ifdef USE_IMGUI
static bool battleEditorDirty = false;
// ネジ番号は0～9。コード解析でも配列境界を確定できるよう、
// 動的添字ではなく定数添字で対応するGPを返す。
static BattlePoint* FindScrewPoint(BossBattleSettings& settings, int screwIndex) {
    switch (screwIndex) {
    case 0: return &settings.partGrips[0];
    case 1: return &settings.partGrips[1];
    case 2: return &settings.partGrips[2];
    case 3: return &settings.partGrips[3];
    case 4: return &settings.accessoryGrips[0];
    case 5: return &settings.accessoryGrips[1];
    case 6: return &settings.accessoryGrips[2];
    case 7: return &settings.accessoryGrips[3];
    case 8: return &settings.accessoryGrips[4];
    case 9: return &settings.accessoryGrips[5];
    default: return nullptr;
    }
}
// 【ボスのネジ調整】実際のグリップ接続点を編集する。
// ワールド座標は+Yが上。座標はボス中心からの相対値で保存される。
static void DrawScrewEasyEditor() {
    static int selected = 1;  // 0:本体, 1～3:プロペラ, 4～7:ジェット, 8:パイプ, 9:目
    const char* names[10] = { "本体（破壊不可）", "左プロペラ", "中央プロペラ", "右プロペラ",
        "ジェット1", "ジェット2", "ジェット3", "ジェット4", "パイプ", "目（破壊不可）" };
    ImGui::TextUnformatted("ネジを選んで移動できます。点をドラッグしても移動可能です。");
    ImGui::SetNextItemWidth(240.0f);
    ImGui::Combo("編集するネジ", &selected, names, 10);
    BattlePoint* point = FindScrewPoint(liveBattleConfig, selected);
    if (point == nullptr) {
        selected = 1;
        point = FindScrewPoint(liveBattleConfig, selected);
    }
    bool changed = false;
    if (ImGui::DragFloat2("ボス中心からの位置 X/Y", &point->x, 0.5f, -320.0f, 320.0f, "%.1f")) changed = true;
    if (ImGui::Button("中心に戻す")) { point->x = 0.0f; point->y = 0.0f; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("このネジを初期位置に戻す")) {
        BossBattleSettings defaults{};
        if (const BattlePoint* original = FindScrewPoint(defaults, selected)) {
            *point = *original;
        }
        changed = true;
    }
    ImGui::TextUnformatted("青:他のGP / 黄色:選択GP / 十字:ボス中心 (Yは上が+) ");
    const float side = 290.0f, scale = side / 680.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##ScrewDragCanvas", ImVec2(side, side));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + side, origin.y + side), IM_COL32(22, 31, 45, 255));
    const float cx = origin.x + side * .5f, cy = origin.y + side * .5f;
    dl->AddLine(ImVec2(cx, origin.y), ImVec2(cx, origin.y + side), IM_COL32(91, 108, 130, 255));
    dl->AddLine(ImVec2(origin.x, cy), ImVec2(origin.x + side, cy), IM_COL32(91, 108, 130, 255));
    dl->AddRect(ImVec2(cx - 54.0f, cy - 54.0f), ImVec2(cx + 54.0f, cy + 54.0f), IM_COL32(108, 135, 166, 190));
    for (int i = 0; i < 10; ++i) {
        const BattlePoint& p = *FindScrewPoint(liveBattleConfig, i);
        const ImVec2 v(cx + p.x * scale, cy - p.y * scale);
        dl->AddCircleFilled(v, i == selected ? 7.0f : 4.5f,
            i == selected ? IM_COL32(255, 210, 80, 255) : IM_COL32(99, 188, 241, 255));
        dl->AddText(ImVec2(v.x + 7.0f, v.y - 8.0f), IM_COL32(234, 239, 248, 255), std::to_string(i).c_str());
    }
    if (hovered && ImGui::GetIO().MouseClicked[0]) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        float best = 13.0f * 13.0f;
        for (int i = 0; i < 10; ++i) {
            const BattlePoint& p = *FindScrewPoint(liveBattleConfig, i);
            const float dx = mouse.x - (cx + p.x * scale), dy = mouse.y - (cy - p.y * scale);
            const float d = dx * dx + dy * dy;
            if (d < best) { best = d; selected = i; }
        }
    }
    if (ImGui::IsItemActive() && (ImGui::GetIO().MouseDown[0] && (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f))) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        // 配列外アクセスを避けるため、選択番号の範囲を明示してから参照する。
        // GP 0～3 は本体側、GP 4～9 は付属パーツ側（配列添字は 0～5）。
        BattlePoint* target = FindScrewPoint(liveBattleConfig, selected);
        if (target != nullptr) {
            const float x = (mouse.x - cx) / scale, y = (cy - mouse.y) / scale;
            target->x = (std::max)(-320.0f, (std::min)(320.0f, x));
            target->y = (std::max)(-320.0f, (std::min)(320.0f, y));
            changed = true;
        }
    }
    ImGui::Text("選択: %s  X %.1f / Y %.1f", names[selected], point->x, point->y);
    if (changed) {
        // プレビューも触手の接続先も同じボス設定を利用するので即反映。
        if (liveBattleConfig.Valid()) {
            boss.SetBattleSettings(liveBattleConfig);
            arena.SetBattleSettings(liveBattleConfig);
            battleEditorDirty = true;
        }
    }
    ImGui::TextUnformatted("※ 描画画像の部位位置は動かさず、触手の接続位置だけ移動します。");
    ImGui::TextUnformatted("保存する場合は上部の『まとめて保存』を押してください。");
}

// 軽量なボス戦デバッグ。1タブに攻撃ON/OFF・発射点・ネジ設定をまとめる。
static void DrawBossCombatPanel() {
    auto& v = BossCombatEditor::values;
    const char* attackNames[5] = { "縦突進 (Press)","射撃 (Shot)","横突進 (Rush)","コマ (Top)","ヨーヨー (YoYo)" };
    ImGui::TextUnformatted("通常AIのON/OFFと、テストボタンは独立しています。");
    ImGui::TextUnformatted("開始位置: ワールド座標のオフセット (+Yは上)");
    if (ImGui::BeginTable("##BossAttacks", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("攻撃");ImGui::TableSetupColumn("ON/OFF");
        ImGui::TableSetupColumn("開始位置 X/Y");ImGui::TableSetupColumn("単独テスト");
        ImGui::TableHeadersRow();
        for (int i = 0;i < 5;++i) {
            ImGui::PushID(i); ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);ImGui::TextUnformatted(attackNames[i]);
            ImGui::TableSetColumnIndex(1);ImGui::Checkbox("##enable", &v.attackEnabled[i]);
            ImGui::TableSetColumnIndex(2);ImGui::SetNextItemWidth(-1.0f);
            ImGui::PushItemWidth(65.0f);
            ImGui::DragFloat("X", &v.startOffsetX[i], 1.0f, -500.0f, 500.0f, "%.0f");
            ImGui::SameLine();
            ImGui::DragFloat("Y", &v.startOffsetY[i], 1.0f, -300.0f, 300.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button("実行")) {
                const BossAttack ids[5] = { BossAttack::Press,BossAttack::Shot,BossAttack::Rush,BossAttack::Top,BossAttack::YoYo };
                const bool ok = boss.TriggerAttackForEditor(ids[i]);
                std::snprintf(attackTestMessage, sizeof(attackTestMessage), "%s: %s", attackNames[i], ok ? "予兆から開始" : "ダウン中などで開始不可");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("ステージ2専用攻撃（第2ステージでテスト）");
    ImGui::Checkbox("大型ミサイル ON", &v.missileEnabled);
    ImGui::DragFloat("ミサイル発射X補正", &v.missileOffsetX, 1.0f, -400.0f, 400.0f);
    ImGui::DragFloat("ミサイル発射Y補正", &v.missileOffsetY, 1.0f, -300.0f, 300.0f);
    if (ImGui::Button("大型ミサイルをテスト")) {
        if (currentStage == StageId::Coaster) { arena.TestStage2Missile();std::snprintf(attackTestMessage, sizeof(attackTestMessage), "大型ミサイルの発射テストを開始"); }
        else std::snprintf(attackTestMessage, sizeof(attackTestMessage), "第2ステージで実行してください");
    }
    ImGui::Checkbox("8の字カッター ON", &v.cutterEnabled);
    ImGui::DragFloat("カッター開始X補正", &v.cutterOffsetX, 1.0f, -400.0f, 400.0f);
    ImGui::DragFloat("カッター開始Y補正", &v.cutterOffsetY, 1.0f, -300.0f, 300.0f);
    if (ImGui::Button("8の字カッターをテスト")) {
        if (currentStage == StageId::Coaster) { arena.TestStage2Cutter();std::snprintf(attackTestMessage, sizeof(attackTestMessage), "8の字カッターをテスト中"); }
        else std::snprintf(attackTestMessage, sizeof(attackTestMessage), "第2ステージで実行してください");
    }
    ImGui::TextWrapped("テスト: %s", attackTestMessage);
    ImGui::TextUnformatted("注意: 既存攻撃を予兆から開始。ゲームオーバー時はリスタートが必要です。");
    ImGui::Separator();
    if (ImGui::CollapsingHeader("ネジ / 緩ませ / 引き剥がし", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("緩ませ速度と必要張力は下の『ネジ位置・必要張力』で編集できます。");
        ImGui::SliderInt("引き剥がし失敗時のダメージ", &v.failedYankDamage, 0, 30);
        ImGui::Separator();
        ImGui::Checkbox("ボスがネジを巻き直す", &v.repairEnabled);
        ImGui::SliderFloat("巻き直しを始める緩み度", &v.repairThreshold, 0.05f, 1.0f, "%.2f");
        ImGui::SliderInt("巻き直し間隔 (フレーム)", &v.repairCooldown, 0, 1800);
        ImGui::SliderInt("巻き直し動作時間 (フレーム)", &v.repairDuration, 30, 600);
        ImGui::SliderFloat("1フレームあたり締める量", &v.repairPerFrame, 0.0001f, 0.05f, "%.4f");
        float loose = 0.0f;const int screw = arena.MostLooseScrew(&loose);
        ImGui::Text("対象ネジ: %d / 緩み %.1f%% / 巻き直し %s", screw, loose * 100.0f, boss.IsRepairingScrew() ? "動作中" : "待機中");
        ImGui::TextWrapped("緩ませ→片腕でネジ保持＋もう片腕でGPを巻く→張力到達で剥がす。緩ませだけでは壊れません。");
        ImGui::TextUnformatted("本体と目は破壊不可。外装が落ちない場合は必要張力とGP間距離を確認。");
    }
    BossCombatEditor::Clamp();
    ImGui::TextWrapped("保存状態: %s", BossCombatEditor::status);
}
// 旧F5/F6/F9を1つのウィンドウへ統合する。
static bool showGameEditor = false;
// 第2形態の軌道を数値とプレビューで同時に調整。
static void DrawStage2TrajectoryPanel() {
    using namespace Stage2Tuning;
    Settings& v = values;
    ImGui::TextWrapped("ステージ2専用。変更はその場で反映。保存は上の『まとめて保存』。");
    ImGui::Separator();
    if (ImGui::CollapsingHeader("8の字カッター", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("画面端の余白", &v.cutterEdgeMargin, 10.0f, 270.0f, "%.0f px");
        ImGui::SliderFloat("上端Y (画面座標)", &v.cutterTop, 15.0f, 300.0f, "%.0f px");
        ImGui::SliderFloat("下端Y (画面座標)", &v.cutterBottom, 400.0f, 700.0f, "%.0f px");
        ImGui::SliderFloat("中央通過X", &v.cutterCenterX, 400.0f, 880.0f, "%.0f px");
        ImGui::SliderInt("1周の長さ (frame)", &v.cutterDuration, 45, 180);
    }
    if (ImGui::CollapsingHeader("大型追尾ミサイル", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("初速", &v.missileStartSpeed, 1.0f, 25.0f, "%.2f");
        ImGui::SliderFloat("加速", &v.missileAcceleration, 0.0f, 0.50f, "%.3f");
        ImGui::SliderFloat("追尾旋回角 (度/frame)", &v.missileTurnDeg, 0.0f, 12.0f, "%.2f");
        ImGui::SliderInt("追尾フレーム", &v.missileHomingFrames, 0, 160);
        ImGui::SliderInt("発射間隔 (frame)", &v.missileInterval, 20, 180);
    }
    if (ImGui::CollapsingHeader("横突進・縦突進")) {
        ImGui::SliderFloat("横突進の高さ (世界Y)", &v.rushHeight, 180.0f, 650.0f, "%.0f");
        ImGui::SliderFloat("横突進の速度倍率", &v.rushSpeedMultiplier, 0.50f, 4.0f, "%.2f");
        ImGui::SliderFloat("縦突進の開始高度", &v.verticalStartHeight, 350.0f, 650.0f, "%.0f");
        ImGui::SliderFloat("横位置を合わせる速度", &v.alignXSpeed, 2.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("縦位置を合わせる速度", &v.alignYSpeed, 2.0f, 30.0f, "%.1f");
        ImGui::SliderInt("落下前の待機 (frame)", &v.verticalWaitFrames, 5, 120);
        ImGui::SliderFloat("急降下加速度", &v.verticalAccel, 0.1f, 8.0f, "%.2f");
        ImGui::SliderFloat("急降下最高速度", &v.verticalMaxSpeed, 8.0f, 70.0f, "%.1f");
        ImGui::SliderInt("急降下後の硬直", &v.verticalRecoveryFrames, 0, 180);
        ImGui::SliderFloat("急降下後の復帰高度", &v.verticalReturnHeight, 200.0f, 550.0f, "%.0f");
    }
    ImGui::Separator();
    ImGui::TextUnformatted("カッター軌道プレビュー（実際の軌道計算を使用）");
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = (std::min)(490.0f, ImGui::GetContentRegionAvail().x);
    const float h = w * 720.0f / 1280.0f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(19, 28, 42, 255));
    draw->AddRect(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(112, 126, 144, 255));
    for (int i = 0;i < 180;++i) {
        const Vector2 a = CutterPoint(float(i) / 180.0f), b = CutterPoint(float(i + 1) / 180.0f);
        draw->AddLine(ImVec2(pos.x + a.x * w / 1280.0f, pos.y + a.y * h / 720.0f),
            ImVec2(pos.x + b.x * w / 1280.0f, pos.y + b.y * h / 720.0f), IM_COL32(255, 112, 164, 255), 2.0f);
    }
    ImGui::Dummy(ImVec2(w, h));
    if (ImGui::Button("第2ステージの初期値に戻す")) v = Settings{};
    ImGui::TextWrapped("%s", Stage2Tuning::status);
}

static int editorRequestedTab = 5; // F5 の初期表示はマリオメーカー風ステージ配置
static bool f5Prev = false;
static bool f6Prev = false;
static bool f9Prev = false;
static bool f4Prev = false;
static char gameEditorStatus[256] = "保存ボタンで画像とボス設定をまとめて記録できます。";

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
    if (ImGui::CollapsingHeader("5. Background grips")) {
        valueI("Background grip count", &liveBattleConfig.anchorCount, 1, BossBattleSettings::kMaxAnchors);
        for (int i = 0; i < liveBattleConfig.anchorCount; ++i) {
            ImGui::PushID(i);
            if (ImGui::DragFloat2("Grip XY", &liveBattleConfig.anchors[i].x, 1.0f, 30.0f, 1250.0f)) changed = true;
            ImGui::PopID();
        }
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
// 操作はF5:ステージ配置 / F6:画像 / F9:ボス / F8:バランス再読込。
static void DrawGameEditor() {
    AssetEditor::opened = false; // プレビューは画像タブを開いている時だけ描画する
    if (!showGameEditor) return;
    // 画面いっぱいを初期サイズにする（Noviceのウィンドウ外には広げない）。
    // ウィンドウIDを更新し、古いimgui.iniの小さな位置・サイズ設定を引き継がない。
    const ImVec2 editorDisplay = ImGui::GetIO().DisplaySize;
    const ImVec2 editorSize((std::max)(320.0f, editorDisplay.x - 4.0f),
        (std::max)(320.0f, editorDisplay.y - 4.0f));
    ImGui::SetNextWindowPos(ImVec2(2.0f, 2.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(editorSize, ImGuiCond_Always);
    if (!ImGui::Begin("ゲーム調整エディター###UnifiedGameEditorLarge", &showGameEditor)) {
        ImGui::End(); return;
    }
    // 保存済みのウィンドウ位置・サイズに関係なく、ボタンで再最大化できる。
    if (ImGui::SmallButton("画面いっぱいに広げる")) {
        ImGui::SetWindowPos(ImVec2(2.0f, 2.0f));
        ImGui::SetWindowSize(editorSize);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("F5: 開閉  /  中央: ステージ配置  /  左右: パーツと設定");
    ImGui::Text("ステージ%d  ボスHP %d / %d  プレイヤーHP %d / %d",
        int(currentStage), boss.GetHp(), boss.GetMaxHp(),
        arena.GetPlayer().GetHp(), arena.GetPlayer().GetMaxHp());

    // 「まとめて保存」は画像・アニメとボス設定の両方を記録する。
    // 失敗時は失敗を明示し、保存できたと偽らない。
    if (ImGui::Button("まとめて保存")) {
        const bool assetOk = AssetEditor::Save();
        const bool partsOk = BossPartsEditor::Save();
        const bool bossOk = liveBattleConfig.Valid() &&
            liveBattleConfig.Save(AssetEditor::BossSettingsPath().c_str());
        const bool trajectoryOk = Stage2Tuning::Save(Stage2FilePath().c_str());
        const bool sceneOk = MiniUnityEditor::Save();
        const bool tilesOk = TileMapEditor::Save();
        const bool combatOk = BossCombatEditor::Save(BossCombatFilePath().c_str());
        const bool cameraOk = CameraBackgroundTuning::Save(CameraBGFilePath().c_str());
        const bool hudOk = GameHudEditor::Save(GameHudFilePath().c_str());
        const bool playerOk = PlayerTuning::Save(PlayerTuningFilePath().c_str());
        std::snprintf(gameEditorStatus, sizeof(gameEditorStatus),
            "保存: 画像=%s / ボス=%s / 軌道=%s / シーン=%s / カメラ=%s / 攻撃=%s",
            assetOk ? "OK" : "失敗", bossOk ? "OK" : "失敗",
            trajectoryOk ? "OK" : "失敗", sceneOk ? "OK" : "失敗", cameraOk ? "OK" : "失敗", combatOk ? "OK" : "失敗");
        if (!playerOk) ImGui::TextUnformatted("プレイヤー設定の保存に失敗しました");
        if (!hudOk) ImGui::TextUnformatted("HUD設定の保存に失敗しました");
        if (!partsOk) ImGui::TextUnformatted("ボスパーツの保存に失敗しました");
        if (!tilesOk) ImGui::TextUnformatted("マップチップの保存に失敗しました");
        if (bossOk) { battleEditorDirty = false; FinalTuningEditor::dirty = false; }
    }
    ImGui::SameLine();
    if (ImGui::Button("保存設定を再読込")) {
        AssetEditor::Initialize();
        const bool partsOk = BossPartsEditor::Load();
        if (!partsOk) ImGui::TextUnformatted("ボスパーツの設定を読み込めませんでした");
        const bool trajectoryOk = Stage2Tuning::Load(Stage2FilePath().c_str());
        const bool sceneOk = MiniUnityEditor::Load();
        const bool tilesOk = TileMapEditor::Load();
        if (!tilesOk) ImGui::TextUnformatted("マップチップの読込なし／失敗（現在の配置を維持）");
        const bool combatOk = BossCombatEditor::Load(BossCombatFilePath().c_str());
        const bool cameraOk = CameraBackgroundTuning::Load(CameraBGFilePath().c_str());
        const bool hudOk = GameHudEditor::Load(GameHudFilePath().c_str());
        (void)hudOk;
        const bool playerOk = PlayerTuning::Load(PlayerTuningFilePath().c_str());
        BossBattleSettings loaded;
        const bool bossOk = loaded.Load(AssetEditor::BossSettingsPath().c_str());
        if (bossOk) { liveBattleConfig = loaded; ResetStageBattle(currentStage); }
        if (!playerOk) std::snprintf(gameEditorStatus, sizeof(gameEditorStatus), "プレイヤー設定ファイルなし／読込失敗（現在の値を維持）");
        std::snprintf(gameEditorStatus, sizeof(gameEditorStatus),
            "再読込: 画像 %s / ボス %s / 軌道 %s / シーン %s / カメラ %s / 攻撃 %s", AssetEditor::status,
            bossOk ? "OK" : "なし／失敗", trajectoryOk ? "OK" : "初期値／失敗", sceneOk ? "OK" : "なし／失敗", cameraOk ? "OK" : "なし／失敗", combatOk ? "OK" : "初期値／失敗");
    }
    ImGui::SameLine();
    if (ImGui::Button("戦闘をやり直す")) {
        if (liveBattleConfig.Valid()) ResetStageBattle(currentStage);
    }
    ImGui::TextWrapped("%s", gameEditorStatus);
    if (!liveBattleConfig.Valid())
        ImGui::TextColored(ImVec4(1, .4f, .4f, 1), "ボスの設定値が無効です。保存前に修正してください。");
    ImGui::Separator();

    // Unity風の左右分割: 左は機能を探すナビゲーション、右は既存の各種編集パネル。
    // 編集処理そのものは既存タブを利用し、戦闘・カメラ・保存の動作を維持する。
    // F5/F6/F9 のタブ指定も有効なままにする。
    static int sidebarSelection = 5; // 初回はステージ配置を開く
    if (editorRequestedTab >= 0) sidebarSelection = editorRequestedTab;
    // 配置画面では中央キャンバスを広く使うため、サイドメニューを上部のショートカットに変更。
    const bool marioLayout = (sidebarSelection == 5);
    if (marioLayout) {
        const struct { const char* label; int tab; } quickMenus[] = {
            {"配置",5},{"ボス",8},{"カメラ",6},{"UI",9},{"画像",1},{"プレイヤー",7},{"攻撃",4},{"調整",0},{"詳細",3}
        };
        for (int i = 0;i<int(sizeof(quickMenus) / sizeof(quickMenus[0]));++i) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::SmallButton(quickMenus[i].label)) { sidebarSelection = quickMenus[i].tab;editorRequestedTab = sidebarSelection; }
        }
        ImGui::SameLine();
        // マップチップ専用ショートカット。パレットを閉じていても再表示する。
        if (ImGui::SmallButton("マップチップ##OpenTilePalette")) {
            sidebarSelection = 5;
            editorRequestedTab = 5;
            MiniUnityEditor::paintTool = MiniUnityEditor::PaintTool::Tile;
            MiniUnityEditor::marioShowPalette = true;
        }
    }
    if (!marioLayout) {
        ImGui::BeginChild("##UnityLeftNavigation", ImVec2(165.0f, 0), true);
        ImGui::TextUnformatted("HIERARCHY / TOOLS");
        ImGui::Separator();
        ImGui::Text("Stage %d", int(currentStage));
        ImGui::Spacing();
        const struct { const char* name; int tab; } editorMenus[] = {
            { "ステージ配置", 5 },
            { "ボスパーツ", 8 },
            { "カメラ・背景", 6 },
            { "UI・ルール", 9 },
            { "画像・アニメ", 1 },
            { "プレイヤー", 7 },
            { "ボス攻撃・GP", 4 },
            { "全体バランス", 0 },
            { "デバッグ", 3 }
        };
        for (int i = 0; i < int(sizeof(editorMenus) / sizeof(editorMenus[0])); ++i) {
            ImGui::PushID(i);
            const bool selected = sidebarSelection == editorMenus[i].tab;
            if (ImGui::Selectable(editorMenus[i].name, selected, 0, ImVec2(-1, 29))) {
                sidebarSelection = editorMenus[i].tab;
                editorRequestedTab = sidebarSelection;
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::TextWrapped("ステージ配置ではオブジェクトを選択して Inspector で編集できます。");
        ImGui::TextDisabled("F5: 開閉  F6: 画像  F9: ボス");
        ImGui::EndChild();
        ImGui::SameLine();
    } // ステージ配置以外は従来の左メニューを維持
    ImGui::BeginChild("##UnityRightWorkspace", ImVec2(0, 0), false);
    // 右側は元のタブ構造を残し、既存機能との互換性を保つ。
    if (ImGui::BeginTabBar("GameEditTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        const ImGuiTabItemFlags tab0 = editorRequestedTab == 0 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab1 = editorRequestedTab == 1 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab3 = editorRequestedTab == 3 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab4 = editorRequestedTab == 4 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab5 = editorRequestedTab == 5 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab6 = editorRequestedTab == 6 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab7 = editorRequestedTab == 7 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab8 = editorRequestedTab == 8 ? ImGuiTabItemFlags_SetSelected : 0;
        const ImGuiTabItemFlags tab9 = editorRequestedTab == 9 ? ImGuiTabItemFlags_SetSelected : 0;
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
                    const bool ok = boss.TriggerAttackForEditor(attack[action.testAttack]);
                    std::snprintf(attackTestMessage, sizeof(attackTestMessage), ok ? "攻撃テストを予兆から開始" : "攻撃テストできません（ダウン中など）");
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("UI・ゲームルール", nullptr, tab9)) {
            ImGui::BeginChild("##HudRuleEditor", ImVec2(0, 0), false);
            GameHudEditor::DrawPanel();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("プレイヤー", nullptr, tab7)) {
            ImGui::BeginChild("##PlayerTuningArea", ImVec2(0, 0), false);
            // HPだけは実際のPortalPlayerとボス戦設定へ直接反映する。
            // 最大HPと被ダメージ倍率は「まとめて保存」で永続化される。
            if (ImGui::CollapsingHeader("HP・耐久力", ImGuiTreeNodeFlags_DefaultOpen)) {
                int maxHp = liveBattleConfig.playerMaxHp;
                if (ImGui::SliderInt("最大HP##PlayerTab", &maxHp, 1, 1000)) {
                    liveBattleConfig.playerMaxHp = maxHp;
                    arena.GetPlayer().SetMaxHp(maxHp);
                    battleEditorDirty = true;
                }
                int currentHp = arena.GetPlayer().GetHp();
                ImGui::Text("現在のHP  %d / %d", currentHp, arena.GetPlayer().GetMaxHp());
                if (ImGui::SliderInt("現在HP##PlayerTab", &currentHp, 0, arena.GetPlayer().GetMaxHp())) {
                    arena.GetPlayer().SetCurrentHpForEditor(currentHp);
                }
                if (ImGui::Button("全回復##PlayerTab"))
                    arena.GetPlayer().SetCurrentHpForEditor(arena.GetPlayer().GetMaxHp());
                ImGui::SameLine();
                if (ImGui::Button("HPを1にする##PlayerTab"))
                    arena.GetPlayer().SetCurrentHpForEditor(1);
                if (ImGui::SliderInt("受けるダメージ倍率 (%)##PlayerTab", &liveBattleConfig.incomingDamagePercent, 0, 500)) {
                    battleEditorDirty = true;
                }
                ImGui::TextDisabled("現在HPはテスト用。再起動時には最大HPで開始します。");
            }
            ImGui::Separator();
            PlayerTuning::DrawPanel();
            ImGui::TextUnformatted("反映後は画面上部の『まとめて保存』を押してください。");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ボス攻撃", nullptr, tab4)) {
            ImGui::BeginChild("##AllBossAttacks", ImVec2(0, 0), false);
            if (ImGui::CollapsingHeader("攻撃ON/OFF・開始位置・単独テスト", ImGuiTreeNodeFlags_DefaultOpen))
                DrawBossCombatPanel();
            if (ImGui::CollapsingHeader("ネジ位置・必要張力（かんたん編集）", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawScrewEasyEditor();
                ImGui::Separator();
                if (ImGui::Button("サクサク設定を適用##QuickArmorPreset")) {
                    BossCombatEditor::ApplyQuickArmorPreset();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("GP吸着1.5倍 / 必要張力70%% / 最大緩み+45%%");
                ImGui::TextDisabled("ボタンを押すと即時反映。次回起動にも残す場合は「まとめて保存」。");
                auto& tension = BossCombatEditor::values;
                ImGui::SliderFloat("ボスGPの照準距離倍率", &tension.bossGripRangeMultiplier, 1.0f, 2.0f, "%.2f 倍");
                ImGui::SliderFloat("ボスGPの照準角の許容値", &tension.bossGripAimCos, -0.5f, 0.9f, "%.2f");
                ImGui::SliderFloat("ボスGPの優先度", &tension.bossGripPriorityBonus, 0.0f, 250.0f, "%.0f");

                ImGui::TextUnformatted("GPヒット時に一度だけ緩ませる（距離が遠いほど増加）");
                ImGui::SliderFloat("近距離ヒットの緩み度", &tension.hitMinLoosen, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("遠距離ヒットの緩み度", &tension.hitMaxLoosen, tension.hitMinLoosen, 1.0f, "%.2f");
                ImGui::SliderFloat("最短判定距離(px)", &tension.hitMinDistance, 0.0f, 1200.0f, "%.0f");
                ImGui::SliderFloat("最大判定距離(px)", &tension.hitMaxDistance, tension.hitMinDistance + 1.0f, 2000.0f, "%.0f");
                ImGui::SliderInt("同じネジへの再ヒット待機(フレーム)", &tension.hitCooldownFrames, 0, 180);
                if (ImGui::Button("緩ませ設定を初期値に戻す")) {
                    tension.hitMinLoosen = 0.10f; tension.hitMaxLoosen = 0.50f;
                    tension.hitMinDistance = 0.0f; tension.hitMaxDistance = 400.0f;
                    tension.hitCooldownFrames = 18;
                }
                ImGui::TextUnformatted("掴み続けても緩みは加算されません。再接続で再判定します。");
                ImGui::Separator();
                ImGui::TextUnformatted("装甲を外す2つの方法");
                ImGui::Checkbox("緩ませだけでネジを外す", &tension.loosenAutoPeelEnabled);
                if (tension.loosenAutoPeelEnabled) {
                    int thresholdPercent = static_cast<int>(tension.loosenAutoPeelThreshold * 100.0f + 0.5f);
                    if (ImGui::SliderInt("自動で外れる緩み度(%)", &thresholdPercent, 10, 100))
                        tension.loosenAutoPeelThreshold = thresholdPercent / 100.0f;
                }
                ImGui::Checkbox("背景GP＋ボスGPの2本引き剥がし", &tension.twoGripPeelEnabled);
                ImGui::TextWrapped("片方をボスのネジ、もう片方を背景GPに接続。背景側の巻き取りでボス側の触手が張ったときに、必要張力で判定します。");
                ImGui::TextDisabled("両方に接続しただけでは引き剥がしは起きません。");

                ImGui::SliderFloat("緩ませ前の必要張力", &tension.requiredTensionFull, 30.0f, 1500.0f, "%.0f");
                ImGui::SliderFloat("緩ませ100%%時の必要張力", &tension.requiredTensionLoose, 10.0f, tension.requiredTensionFull, "%.0f");
                BossCombatEditor::Clamp();
                ImGui::Text("現在のネジ: 最大緩み %.0f%%", [&]() { float loose = 0.0f; arena.MostLooseScrew(&loose); return loose * 100.0f; }());
                ImGui::TextUnformatted("値を下げるほど引き剥がしが成功しやすくなります。");
            }
            if (ImGui::CollapsingHeader("ステージ2：飛行・軌道・GP", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawStage2TrajectoryPanel();
                ImGui::Separator();
                ImGui::Checkbox("4種類のフライバイを使う", &Stage2Tuning::values.flybyVariants);
                ImGui::SliderFloat("画面外へ抜ける距離", &Stage2Tuning::values.flybyOutsideMargin, 20.f, 300.f, "%.0f px");
                ImGui::SliderFloat("上昇・S字の高度差", &Stage2Tuning::values.flybyArcHeight, 0.f, 280.f, "%.0f px");
                ImGui::SliderFloat("高速通過の速度倍率", &Stage2Tuning::values.flybySpeed, 0.5f, 3.f, "%.2f");
                const char* styles[4] = { "低空フライバイ", "高高度フライバイ", "急上昇フライバイ", "S字フライバイ" };
                for (int i = 0;i < 4;++i) {
                    ImGui::PushID(800 + i);
                    if (ImGui::Button(styles[i])) {
                        const bool ok = currentStage == StageId::Coaster && boss.TriggerFlybyForEditor(i);
                        std::snprintf(attackTestMessage, sizeof(attackTestMessage),
                            ok ? "飛行テストを開始しました" : "ステージ2で、ダウンしていない時に試してください");
                    }
                    if (i != 3) ImGui::SameLine();
                    ImGui::PopID();
                }
                Stage2Tuning::ClampValues(Stage2Tuning::values);
            }
            if (ImGui::CollapsingHeader("GP：配置・スクロール・ボスのネジ", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& v = Stage2Tuning::values;
                ImGui::SliderInt("ステージ2：流れるGP数", &v.gpCount, 0, 2);
                ImGui::SliderFloat("GPの流速", &v.gpScrollSpeed, 0.0f, 6.0f);
                ImGui::SliderFloat("GPの間隔", &v.gpSpacing, 260.0f, 1000.0f);
                ImGui::DragFloat("GP開始X", &v.gpStartX, 1.0f, 0.0f, 1280.0f);
                ImGui::DragFloat("GP 1 高さY", &v.gpLeftY, 1.0f, 50.0f, 650.0f);
                ImGui::DragFloat("GP 2 高さY", &v.gpRightY, 1.0f, 50.0f, 650.0f);
                ImGui::TextUnformatted("ボスのネジ座標は『ネジ位置・必要張力』で編集できます。");
                Stage2Tuning::ClampValues(v);
            }
            if (ImGui::CollapsingHeader("ボス詳細設定（ネジ位置・当たり判定・AI）")) {
                DrawBossBattleEditorContents();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ボスパーツ", nullptr, tab8)) {
            ImGui::BeginChild("##BossPartsArea", ImVec2(0, 0), false);
            BossPartsEditor::DrawPanel();
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
        // カメラ編集は探しやすいよう上部の独立タブにまとめる。
        if (ImGui::BeginTabItem("シーン編集", nullptr, tab5)) {
            ImGui::BeginChild("##MiniUnitySceneArea", ImVec2(0, 0), false);
            MiniUnityEditor::DrawPanel(int(currentStage), arena.GetPlayer().GetPosition(), boss.GetPosition());
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("カメラ・背景", nullptr, tab6)) {
            ImGui::BeginChild("##MiniUnityCameraArea", ImVec2(0, 0), false);
            CameraBackgroundTuning::DrawPanel();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("デバッグ・保存先", nullptr, tab3)) {
            ImGui::TextUnformatted("リソースを再起動後にも維持するには「まとめて保存」を押します。");
            ImGui::TextWrapped("画像: %s", AssetEditor::settingsFile.c_str());
            ImGui::TextWrapped("ボス: %s", AssetEditor::BossSettingsPath().c_str());
            ImGui::TextWrapped("軌道: %s", Stage2FilePath().c_str());
            ImGui::TextWrapped("直近の読み込み: %s", AssetEditor::status);
            ImGui::Separator();
            ImGui::Checkbox("画面上のデバッグ文字を表示", &showDebugOverlay);
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
    ImGui::EndChild(); // UnityRightWorkspace
    ImGui::End();
}
#endif


// ------------------------------------------------------------------------
// 【関数】InitStage()
// 何をする？ ボス戦の設定読込、プレイヤー・ボス初期化、カメラ位置を整える。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void InitStage(void) {
    // ボス1～4のパーツ配置を起動時に復元。画像未登録なら従来の描画を使用。
    AssetEditor::FixSettingsLocation();
    BossPartsEditor::Initialize(AssetEditor::resourceRoot + "\\boss_parts_layout.txt");
    // 設定ファイルが無い場合は前回正常に読めた値／初期値で続行。
    // 【注目】【設定読込】エディターの設定が無い場合は既定値を使う。
    liveBattleConfig.Load(AssetEditor::BossSettingsPath().c_str());
    Stage2Tuning::Load(Stage2FilePath().c_str());
    BossCombatEditor::Load(BossCombatFilePath().c_str());
    PlayerTuning::Load(PlayerTuningFilePath().c_str());
    MiniUnityEditor::Load();
    TileMapEditor::Load();
    CameraBackgroundTuning::Load(CameraBGFilePath().c_str());
    GameHudEditor::Load(GameHudFilePath().c_str());
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
    const bool f4Now = liveKeys[DIK_F4] != 0;
    if (f4Now && !f4Prev) { showGameEditor = true; editorRequestedTab = 5; }
    f4Prev = f4Now;
    const bool f5Now = liveKeys[DIK_F5] != 0;
    if (f5Now && !f5Prev) { showGameEditor = !showGameEditor; editorRequestedTab = 5; }
    f5Prev = f5Now;
    const bool f6Now = liveKeys[DIK_F6] != 0;
    if (f6Now && !f6Prev) { showGameEditor = true; editorRequestedTab = 1; }
    f6Prev = f6Now;
    const bool f9Now = liveKeys[DIK_F9] != 0;
    if (f9Now && !f9Prev) { showGameEditor = true; editorRequestedTab = 4; }
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
        PlayerTuning::Load(PlayerTuningFilePath().c_str());
        GameHudEditor::Load(GameHudFilePath().c_str());
        BossCombatEditor::Load(BossCombatFilePath().c_str());
        BossBattleSettings next;
        if (next.Load(AssetEditor::BossSettingsPath().c_str())) { liveBattleConfig = next; creatorRuntime.Reload(); ResetStageBattle(currentStage); reloadIndicatorFrames = 120; }
        else reloadIndicatorFrames = -120;
    }
    f8Prev = f8Now;
    if (reloadIndicatorFrames > 0)--reloadIndicatorFrames;
    if (reloadIndicatorFrames < 0)++reloadIndicatorFrames;
    creatorRuntime.Update();
    const bool bossOffscreen = currentStage == StageId::Coaster &&
        (boss.GetPosition().x < 0.0f || boss.GetPosition().x > 1280.0f);
    // 【重要】ボス外装の四隅ネジ4点を移動杭とは別の目標として渡す。
    // 各ネジは右スティックで狙って接続すると外せる。プレイヤー自身はボスへ引き寄せられない。
    Vector2 armorPoints[BossStage1::kArmorPlates];
    bool armorExists[BossStage1::kArmorPlates];
    // 【繰り返し】対象（触手・ネジ・弾など）を順に確認する。
    for (int i = 0;i < BossStage1::kArmorPlates;++i) {
        armorPoints[i] = boss.GetArmorGripPoint(i);
        // 本体0番は移動用GP（破壊不可）。破壊対象としては表示しない。
        armorExists[i] = !bossOffscreen && boss.IsArmorAttached(i);
    }
    arena.SetBossArmorTargets(armorPoints, armorExists);
    Vector2 extraBossPoints[Boss::kAccessoryGripCount];
    bool extraBossEnabled[Boss::kAccessoryGripCount];
    for (int i = 0; i < Boss::kAccessoryGripCount; ++i) {
        extraBossPoints[i] = boss.GetAccessoryGripPoint(i);
        extraBossEnabled[i] = !bossOffscreen && boss.IsAccessoryGripEnabled(i);
    }
    arena.SetBossExtraTargets(extraBossPoints, extraBossEnabled);
    // プレイヤー更新より前に退場を通知する（退場時のボスGP接続解除）。
    arena.SetBossOutsideScreen(bossOffscreen);
    // Update前の位置を衝突判定に渡し、ワイヤー移動中の床・壁へのめり込みを防ぐ。
    const Vector2 beforeMove = arena.GetPlayer().GetPosition();
    if (arena.Update()) {
        ResetStageBattle(currentStage);
        return;
    }
    {
        StageCollision::Rect worldColliders[StageCollision::kMaxRects]{};
        int used = MiniUnityEditor::CollectSolidColliders(int(currentStage), worldColliders, StageCollision::kMaxRects);
        if (used < StageCollision::kMaxRects) {
            used += TileMapEditor::CollectColliders(int(currentStage), worldColliders + used, StageCollision::kMaxRects - used);
        }
        // ボスパーツ側も同じ物理判定を共有する（外れた部位は判定も消す）。
        const int bossForm = currentStage == StageId::Copter ? 0 : 1;
        const int rest = StageCollision::kMaxRects - used;
        const int additional = BossPartsEditor::CollectColliders(bossForm, boss.GetPosition(),
            worldColliders + used, rest, [&](int link)->bool {
                if (link >= 0 && link < BossStage1::kArmorPlates) return boss.IsArmorAttached(link);
                if (link >= 10 && link < 10 + Boss::kAccessoryGripCount) return boss.IsAccessoryGripEnabled(link - 10);
                return true;
            });
        used += additional;
        arena.GetPlayer().ResolveStageColliders(beforeMove, worldColliders, used);
    }
    // 既存のプレイヤー基本物理はそのまま。時間制だけHP敗北を使わない。
    if (!GameHudEditor::UsesHp())
        arena.GetPlayer().SetCurrentHpForEditor(arena.GetPlayer().GetMaxHp());
    bool timerActive = !stageCleared && !boss.IsDefeated() && !arena.GetPlayer().IsDefeated();
#ifdef USE_IMGUI
    timerActive = timerActive && !showGameEditor; // 編集中は制限時間を消費しない
#endif
    GameHudEditor::Tick(timerActive);
    ++tutorialFrame;
    CameraBackgroundTuning::Tick();
    MiniUnityEditor::TickRuntime();
    PortalPlayer& player = arena.GetPlayer();
    CameraBackgroundTuning::SetTarget(player.GetPosition().x, player.GetPosition().y);
    // シーン編集で配置した敵・危険領域との接触をゲームへ反映。
    // 既存のプレイヤー物理・ジャンプ・触手処理は一切変更しない。
    if (GameHudEditor::UsesHp() && tutorialFrame % 30 == 0 && !player.IsDefeated()) {
        int damage = 0;
        Vector2 source{};
        if (MiniUnityEditor::CheckPlayerHit(int(currentStage), player.GetPosition(),
            24.0f, &damage, &source)) player.TakeDamage(damage, source);
    }
    // ボス編集パーツの接触ダメージ（画像の有効/破壊状態と連動）。
    if (GameHudEditor::UsesHp() && tutorialFrame % 30 == 0 && !player.IsDefeated()) {
        StageCollision::Rect partRects[BossPartsEditor::kMaxParts]{};
        const int bossForm = currentStage == StageId::Copter ? 0 : 1;
        const int n = BossPartsEditor::CollectColliders(bossForm, boss.GetPosition(), partRects,
            BossPartsEditor::kMaxParts, [&](int link)->bool {
                if (link >= 0 && link < BossStage1::kArmorPlates) return boss.IsArmorAttached(link);
                if (link >= 10 && link < 10 + Boss::kAccessoryGripCount) return boss.IsAccessoryGripEnabled(link - 10);
                return true;
            });
        const Vector2 pos = player.GetPosition();
        for (int i = 0;i < n;++i) {
            const auto& r = partRects[i];
            if (r.type != StageCollision::Type::Damage || r.damage <= 0) continue;
            if (StageCollision::Intersect(pos.x - 18.f, pos.x + 18.f, r.left, r.right) &&
                StageCollision::Intersect(pos.y - 30.f, pos.y + 30.f, r.bottom, r.top)) {
                player.TakeDamage(r.damage, boss.GetPosition()); break;
            }
        }
    }
    if (GameHudEditor::UsesHp() && creatorRuntime.showCollision && creatorRuntime.PlayerInHazard(player.GetPosition()) && (tutorialFrame % 45 == 0))
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
    if (GameHudEditor::IsTimeUp() || player.IsDefeated() || stageCleared) return;
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
        // 更新前のボスGP座標を描画に使わない。ボス本体移動後の座標へ同期。
        // プレイヤーの接続・巻き取り物理やボスAIの計算は変更しない。
        {
            Vector2 syncedArmor[BossStage1::kArmorPlates]{};
            bool syncedArmorEnabled[BossStage1::kArmorPlates]{};
            for (int i = 0; i < BossStage1::kArmorPlates; ++i) {
                syncedArmor[i] = boss.GetArmorGripPoint(i);
                syncedArmorEnabled[i] = boss.IsArmorAttached(i);
            }
            arena.SetBossArmorTargets(syncedArmor, syncedArmorEnabled);
            Vector2 syncedExtras[Boss::kAccessoryGripCount]{};
            bool syncedExtrasEnabled[Boss::kAccessoryGripCount]{};
            for (int i = 0; i < Boss::kAccessoryGripCount; ++i) {
                syncedExtras[i] = boss.GetAccessoryGripPoint(i);
                syncedExtrasEnabled[i] = boss.IsAccessoryGripEnabled(i);
            }
            arena.SetBossExtraTargets(syncedExtras, syncedExtrasEnabled);
        }
        // 巻き直しAI：最も緩んだネジを優先。ボスはこの間攻撃しない。
        if (screwRepairCooldownFrames > 0)--screwRepairCooldownFrames;
        if (repairWasActive && !boss.IsRepairingScrew()) {
            screwRepairCooldownFrames = BossCombatEditor::values.repairCooldown;
            repairWasActive = false;
        }
        if (!boss.IsRepairingScrew() && screwRepairCooldownFrames == 0 && boss.GetState() == BossState::Idle) {
            float mostLoose = 0.0f;
            const int targetScrew = arena.MostLooseScrew(&mostLoose);
            if (targetScrew >= 0 && mostLoose >= BossCombatEditor::values.repairThreshold)
                repairWasActive = boss.BeginScrewRepair(targetScrew);
        }
        if (boss.IsRepairingScrew()) {
            const int targetScrew = boss.RepairScrewIndex();
            if (arena.ScrewExists(targetScrew))
                arena.TightenScrew(targetScrew, BossCombatEditor::values.repairPerFrame);
        }
        if (boss.IsCoasterStage()) {
            arena.UpdateStage2(boss.GetPosition(), boss.GetHp(), boss.GetMaxHp(),
                boss.GetStage2SpecialMode(), boss.GetStage2SpecialFrame());
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
        if (GameHudEditor::UsesHp() && !boss.IsDefeated()) {
            Vector2 source = {};
            int hit = boss.TryHitPlayer(player.GetPosition(), 24.0f, &source);
            if (hit <= 0 && boss.IsCoasterStage()) hit = arena.Stage2HitPlayer(player.GetPosition(), &source);
            if (hit > 0) {
                const int adjustedDamage = (hit * liveBattleConfig.incomingDamagePercent + 50) / 100;
                if (adjustedDamage > 0) player.TakeDamage(adjustedDamage, source);
            }
        }
    }
    // 時間制のみでは残りHPをゲームオーバー条件に使用しない。
    if (!GameHudEditor::UsesHp())
        player.SetCurrentHpForEditor(player.GetMaxHp());
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
    TileMapEditor::DrawWorld(int(currentStage));
    MiniUnityEditor::DrawWorld(int(currentStage), 0);
    // 【注目】【描画順②】ボスはプレイヤーより後ろに描く。
    boss.Draw();
    if (!(boss.IsCoasterStage() && (boss.GetPosition().x < 0.0f || boss.GetPosition().x > 1280.0f)) &&
        boss.IsRepairingScrew() && arena.ScrewExists(boss.RepairScrewIndex())) {
        // 既存のボス絵に重ねる軽量な巻き直し演出。新しいPNGは不要。
        const Vector2 screw = arena.GetScrewWorldPosition(boss.RepairScrewIndex());
        const Vector2 body = boss.GetPosition();
        PortalDraw::Line({ body.x,body.y + 20.0f }, screw, 0xFFBE63FF);
        PortalDraw::Circle(screw, 18.0f, 0xFFE19AFF);
        Novice::ScreenPrintf(500, 155, "BOSS REPAIRING SCREW %d", boss.RepairScrewIndex() + 1);
    }
    MiniUnityEditor::DrawWorld(int(currentStage), 1);
    // 【注目】【描画順③】プレイヤーと触手は最後に描いて手前へ。
    arena.DrawPlayer();
    MiniUnityEditor::DrawWorld(int(currentStage), 2);
    if (showDebugOverlay) { arena.DrawHUD(); creatorRuntime.DrawHUD(); }
    if (showDebugOverlay && reloadIndicatorFrames > 0)Novice::ScreenPrintf(18, 210, "BOSS EDITOR DATA RELOADED (F8)");
    else if (showDebugOverlay && reloadIndicatorFrames < 0)Novice::ScreenPrintf(18, 210, "NO VALID bossbattle_stage1.txt");
    // HUDはシーンのカメラに影響されないスクリーン座標で描く。
    const int playerHp = arena.GetPlayer().GetHp();
    const int bossHp = boss.GetHp();
    GameHudEditor::Draw(playerHp, arena.GetPlayer().GetMaxHp(),
        bossHp, boss.GetMaxHp(), boss.GetArmorPlates(), boss.GetMaxArmorPlates(), boss.IsEnraged());

    if (showDebugOverlay) {
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
    else if (GameHudEditor::IsTimeUp() || arena.GetPlayer().IsDefeated()) {
        Novice::DrawBox(370, 258, 545, 140, 0, 0x161B27DD, kFillModeSolid);
        Novice::ScreenPrintf(567, 300, GameHudEditor::IsTimeUp() ? "TIME UP" : "GAME OVER");
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
