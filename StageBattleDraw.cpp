#include "StageBattle.h"
#include "Stage1.h"
#include "Stage2.h"
#include <Novice.h>
#include <dinput.h>
#include <cstring>
#include <string>
#include <algorithm>
#include "PortalDraw.h"
#include "MiniUnityEditor.h"
#include "GameHudEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include "StageBattleContext.h"
using namespace StageBattleInternal;

// ------------------------------------------------------------------------
// 【関数】DrawStage()
// 何をする？ 背景→ボス→プレイヤー→UIの順で画面を描く。後から描くものが前に出る。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------

void StageBattle::Draw() {
    // 描画順：背景・杭 → ボス → プレイヤー・触手 → HUD。
    // ボスの後に背景を描くとボスが隠れるため、この順序を守る。
    // 【注目】【描画順①】床・背景・杭を先に描く（最も奥）。

    battleContext.arena.DrawWorld();
    battleContext.creatorRuntime.DrawWorld();
    TileMapEditor::DrawWorld(int(GetStageId()));
    MiniUnityEditor::DrawWorld(int(GetStageId()), 0);
    // 【注目】【描画順②】ボスはプレイヤーより後ろに描く。
    battleContext.boss.Draw(battleContext.arena);

    if (!(battleContext.boss.IsCoasterStage() && Stage2::IsBossOffscreen(battleContext.boss)) &&
        battleContext.boss.IsRepairingScrew() && battleContext.arena.ScrewExists(battleContext.boss.RepairScrewIndex())) {
        // 既存のボス絵に重ねる軽量な巻き直し演出。新しいPNGは不要。
        const Vector2 screw = battleContext.arena.GetScrewWorldPosition(battleContext.boss.RepairScrewIndex());
        const Vector2 body = battleContext.boss.GetPosition();
        PortalDraw::Line({ body.x,body.y + 20.0f }, screw, 0xFFBE63FF);
        PortalDraw::Circle(screw, 18.0f, 0xFFE19AFF);
        Novice::ScreenPrintf(500, 155, "BOSS REPAIRING SCREW %d", battleContext.boss.RepairScrewIndex() + 1);
    }
    MiniUnityEditor::DrawWorld(int(GetStageId()), 1);
    // 【注目】【描画順③】プレイヤーと触手は最後に描いて手前へ。
    battleContext.arena.DrawPlayer();
    MiniUnityEditor::DrawWorld(int(GetStageId()), 2);
    if (battleContext.showDebugOverlay) { battleContext.arena.DrawHUD(); battleContext.creatorRuntime.DrawHUD(); }
    if (battleContext.showDebugOverlay && battleContext.reloadIndicatorFrames > 0)Novice::ScreenPrintf(18, 210, "BOSS EDITOR DATA RELOADED (F8)");
    else if (battleContext.showDebugOverlay && battleContext.reloadIndicatorFrames < 0)Novice::ScreenPrintf(18, 210, "NO VALID bossbattle_stage1.txt");
    // HUDはシーンのカメラに影響されないスクリーン座標で描く。
    const int playerHp = battleContext.arena.GetPlayer().GetHp();
    const int bossHp = battleContext.boss.GetHp();
    GameHudEditor::Draw(playerHp, battleContext.arena.GetPlayer().GetMaxHp(),
        bossHp, battleContext.boss.GetMaxHp(), battleContext.boss.GetArmorPlates(), battleContext.boss.GetMaxArmorPlates(), battleContext.boss.IsEnraged());

    if (battleContext.showDebugOverlay) {
        // 【ゲームの流れ】操作を覚えるため、目的を画面上に常時短文で表示する。
        // 攻略順：ジェット・パイプ → プロペラ3本 → 本体と目は残して弱点攻撃。
        bool hasAccessory = false;
        for (int i = 0; i < Boss::kAccessoryGripCount - 1; ++i) {
            if (battleContext.boss.IsAccessoryGripEnabled(i)) { hasAccessory = true; break; }
        }
        if (battleContext.boss.IsCoasterStage() && !battleContext.boss.IsDefeated()) {
            Novice::ScreenPrintf(18, 145, "STAGE 2: COASTER / GRAPPLE ABOVE THE RUSH!");
            if (battleContext.boss.GetState() == BossState::Dizzy)
                Novice::ScreenPrintf(18, 168, "COUNTER NOW! AIM FOR THE EYE!");
        }
        else if (hasAccessory) {
            Novice::ScreenPrintf(18, 145, "STEP 1: REMOVE JETS AND PIPES!");
        }
        else if (battleContext.boss.GetArmorPlates() > 0) {
            Novice::ScreenPrintf(18, 145, "STEP 2: REMOVE THREE PROPELLERS!");
        }
        else if (!battleContext.boss.IsDefeated()) {
            Novice::ScreenPrintf(18, 145, "STEP 3: BODY AND EYE REMAIN! ATTACK THE EYE!");
        }
        else {
            Novice::ScreenPrintf(18, 145, "COPTER DOWN! STAGE CLEAR IN A MOMENT!");
        }
        // 【読みやすさ優先】技ごとに避け方を教える。予兆だけ1行表示、追加BOXゼロ。
        // 単調な攻撃ではなく「予兆を読む→対処を選ぶ」遊びにする。
        if (battleContext.boss.GetState() == BossState::Warning) {
            const char* guide = "DODGE!";
            // 【分岐表】現在の状態/攻撃種類に応じて、実行する処理を分ける。
            switch (battleContext.boss.GetSelectedAttack()) {
            case BossAttack::Rush: guide = battleContext.boss.IsCoasterStage() ? "RUSH: GRAPPLE UP NOW!" : "RUSH: FLOOR LANE - JUMP / WIRE!";break;
            case BossAttack::Shot: guide = "SHOT: TWO TRIPLE VOLLEYS - KEEP MOVING!";break;
            case BossAttack::Press:guide = "PRESS: LEAVE THE RED FLOOR MARK!";break;
            case BossAttack::Top:  guide = "TOP: JUMP OVER THE SPINNING TOP!";break;
            case BossAttack::YoYo: guide = "YOYO: OUT AND BACK - CHANGE HEIGHT!";break;
            }
            Novice::ScreenPrintf(18, 168, "%s", guide);
        }
    }
    // 【連打タイム】既存ScreenPrintfとBOX2枚だけのUI。追加パーティクルなし。
    if (battleContext.boss.IsMashActive()) {
        const int hits = battleContext.boss.GetMashCount();
        const int remaining = battleContext.boss.GetMashTimeLeft();
        const int bar = remaining * 320 / BossStage1::kMashTimeFrames;
        Novice::DrawBox(430, 145, 420, 106, 0, 0x131526D9, kFillModeSolid);
        Novice::ScreenPrintf(492, 160, "RENDA!! RIGHT STICK / J !!");
        Novice::ScreenPrintf(515, 188, "COMBO %d HITS", hits);
        Novice::DrawBox(480, 215, 320, 12, 0, 0x292E44FF, kFillModeSolid);
        Novice::DrawBox(480, 215, bar, 12, 0, 0xFFD35CFF, kFillModeSolid);
    }
    else if (battleContext.boss.IsArmorFalling()) {
        Novice::ScreenPrintf(490, 157, "ARMOR BROKEN! COPTER FALLING!");
    }
    if (battleContext.boss.IsArmorStun()) Novice::ScreenPrintf(975, 155, "ARMOR OPEN! MASH NOW!");
    else if (battleContext.boss.IsWaking()) Novice::ScreenPrintf(975, 155, "WAKE BLAST! FALL BACK!");
    else if (battleContext.boss.IsDizzy())
        Novice::ScreenPrintf(975, 155, "WIRE DODGE SUCCESS! COUNTER CHANCE!");
    else if (battleContext.boss.IsWeakPointOpen())
        Novice::ScreenPrintf(975, 155, "CORE BONUS DAMAGE!");
    if (battleContext.stageCleared) {
        Novice::DrawBox(370, 258, 545, 140, 0, 0x161B27DD, kFillModeSolid);
        Novice::ScreenPrintf(555, 300, "STAGE %d CLEAR!", static_cast<int>(GetStageId()));
        if (GetStageId() == StageId::Copter) Stage1::DrawClearGuide();
        else Stage2::DrawClearGuide();
    }
    else if (GameHudEditor::IsTimeUp() || battleContext.arena.GetPlayer().IsDefeated()) {
        Novice::DrawBox(370, 258, 545, 140, 0, 0x161B27DD, kFillModeSolid);
        Novice::ScreenPrintf(567, 300, GameHudEditor::IsTimeUp() ? "TIME UP" : "GAME OVER");
        Novice::ScreenPrintf(518, 330, "PRESS R TO RETRY");
    }
    else if (battleContext.boss.IsDefeated()) {
        Novice::ScreenPrintf(513, 180, "COPTER DOWN! GREAT FIGHT!");
    }
#ifdef _DEBUG
    const char* attack = "SHOT";
    // 【分岐表】現在の状態/攻撃種類に応じて、実行する処理を分ける。
    switch (battleContext.boss.GetSelectedAttack()) {
    case BossAttack::Press:attack = "PRESS";break;
    case BossAttack::Shot:attack = "SHOT";break;
    case BossAttack::Rush:attack = "RUSH";break;
    case BossAttack::Top:attack = "TOP";break;
    case BossAttack::YoYo:attack = "YOYO";break;
    }
    Novice::ScreenPrintf(18, 189, "AI attack: %s, cycle: %d", attack, battleContext.boss.GetAttackCount());
#endif
#ifdef USE_IMGUI
    StageBattleEditor::Draw();
#endif
}
