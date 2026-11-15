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
#include "BossBattleSettings.h"
#include "PortalMath.h"

// ================================================================
// 【第1ステージ】「突進の予兆→ワイヤー回避→失速時に反撃→外装/コアを破壊」。
// 現在は左スティックで移動、右スティックで8方向触手攻撃。Jはキーボード用。AIはBoss、プレイヤーはPortalArenaが管理。
// 画面の派手さは運動/停止/画面揺れを使い、オブジェクト描画は最小限。
// ================================================================
static Boss boss;
static PortalArena arena;
static bool stageCleared = false;
static BossBattleSettings liveBattleConfig;
static int reloadIndicatorFrames = 0;
static bool f8Prev = false;
static int tutorialFrame = 0;
static int clearDelayFrames = 0; // ボス撃破後、短い倒れる演出のあとクリア
static constexpr Vector2 kGravityScale = { 0.0f,-1.2f };

// ------------------------------------------------------------------------
// 【関数】InitStage()
// 何をする？ ボス戦の設定読込、プレイヤー・ボス初期化、カメラ位置を整える。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void InitStage(void) {
    // 設定ファイルが無い場合は前回正常に読めた値／初期値で続行。
    // 【注目】【設定読込】エディターの設定が無い場合は既定値を使う。
    liveBattleConfig.Load();
    // 【注目】【プレイヤー初期化】ボスより先に初期化して、プレイヤー位置への参照を有効にする。
    arena.Initialize();
    arena.SetBattleSettings(liveBattleConfig);
    boss.SetBattleSettings(liveBattleConfig);
    boss.Initialize({ liveBattleConfig.bossSpawn.x,liveBattleConfig.bossSpawn.y });
    // 【注目】【重要：ポインタ】毎フレーム変わるプレイヤーの位置をボスAIへ渡す。
    boss.SetTarget(&arena.GetPlayerPosition());
    stageCleared = false;
    tutorialFrame = 0;
    clearDelayFrames = 0;
    SetBossMashCamera(false, 640.0f, 360.0f);
    SetCameraFollowY(360.0f); // 戦闘中は固定画面
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
    const bool f8Now = liveKeys[DIK_F8] != 0;
    if (f8Now && !f8Prev) {
        BossBattleSettings next;
        if (next.Load()) { liveBattleConfig = next;InitStage();reloadIndicatorFrames = 120; }
        else reloadIndicatorFrames = -120;
    }
    f8Prev = f8Now;
    if (reloadIndicatorFrames > 0)--reloadIndicatorFrames;
    if (reloadIndicatorFrames < 0)++reloadIndicatorFrames;
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
    if (arena.Update()) {
        boss.SetBattleSettings(liveBattleConfig);
        boss.Initialize({ liveBattleConfig.bossSpawn.x,liveBattleConfig.bossSpawn.y });
        // 【注目】【重要：ポインタ】毎フレーム変わるプレイヤーの位置をボスAIへ渡す。
        boss.SetTarget(&arena.GetPlayerPosition());
        stageCleared = false;
        tutorialFrame = 0;
        clearDelayFrames = 0;
        SetCameraFollowY(360.0f);
        return;
    }
    ++tutorialFrame;
    PortalPlayer& player = arena.GetPlayer();
    // 【連打】攻撃が届かない距離でもスタン中は右スティック弾き直しでコアを殴れる。
    const bool mashPulse = arena.ConsumeMashPulse();
    if (boss.IsMashActive() && mashPulse) boss.RegisterMashHit();
    SetBossMashCamera(boss.IsMashActive(), boss.GetPosition().x, boss.GetPosition().y);

    // 四隅ネジは接続時に解除される。長押し張力ゲージは使わない。
    // 四隅のネジを1個ずつ外す。最後の1個で外装めくれ演出開始。
    const int peelRequest = player.ConsumeArmorPeelRequest();
    if (peelRequest >= 0 && !boss.IsDefeated()) {
        // 【外装ヒット確認】本当に剥がれた場合だけプレイヤーのポーズを強調。
        if (boss.TryPeelArmor(peelRequest)) player.OnArmorPeeled();
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
        case PortalAttackKind::Normal:damage = PortalConfig::kNormalDamage;break;
        case PortalAttackKind::Spin:damage = PortalConfig::kSpinDamage;break;
        case PortalAttackKind::Grapple:damage = PortalConfig::kGrappleDamage;break;
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
            if (hit > 0)player.TakeDamage(hit, source);
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
    // 【注目】【描画順②】ボスはプレイヤーより後ろに描く。
    boss.Draw();
    // 【注目】【描画順③】プレイヤーと触手は最後に描いて手前へ。
    arena.DrawPlayer();
    arena.DrawHUD();
    if (reloadIndicatorFrames > 0)Novice::ScreenPrintf(18, 210, "BOSS EDITOR DATA RELOADED (F8)");
    else if (reloadIndicatorFrames < 0)Novice::ScreenPrintf(18, 210, "NO VALID bossbattle_stage1.txt");
    // HPと外装は画面固定のBOX。文字もスクリーン座標なので描画数が増えない。
    const int playerHp = arena.GetPlayer().GetHp();
    const int bossHp = boss.GetHp();
    Novice::DrawBox(17, 92, 284, 15, 0, 0x222735FF, kFillModeSolid);
    Novice::DrawBox(18, 93, static_cast<int>(282.0f * playerHp / PortalPlayer::kMaxHp), 13, 0,
        0x46DBADFF, kFillModeSolid);
    Novice::DrawBox(974, 92, 286, 15, 0, 0x222735FF, kFillModeSolid);
    Novice::DrawBox(975, 93, static_cast<int>(284.0f * bossHp / Boss::kMaxHp), 13, 0,
        boss.IsEnraged() ? 0xFF9A54FF : 0xD65B67FF, kFillModeSolid);
    Novice::ScreenPrintf(18, 114, "PLAYER HP %d/%d", playerHp, PortalPlayer::kMaxHp);
    Novice::ScreenPrintf(970, 114, "COPTER CORE HP %d/%d", bossHp, Boss::kMaxHp);
    Novice::ScreenPrintf(970, 134, "ARMOR %d/%d", boss.GetArmorPlates(), boss.GetMaxArmorPlates());

    // 【ゲームの流れ】操作を覚えるため、目的を画面上に常時短文で表示する。
    if (boss.GetArmorPlates() > 0) {
        Novice::ScreenPrintf(18, 145, "ARMOR: HIT ALL FOUR CORNER SCREWS WITH TENTACLES!");
    }
    else if (!boss.IsDefeated()) {
        Novice::ScreenPrintf(18, 145, "GOAL 2: DODGE RUSH -> EXPOSE CORE -> ATTACK!");
    }
    else {
        Novice::ScreenPrintf(18, 145, "COPTER DOWN!  STAGE CLEAR IN A MOMENT!");
    }
    // 【読みやすさ優先】技ごとに避け方を教える。予兆だけ1行表示、追加BOXゼロ。
    // 単調な攻撃ではなく「予兆を読む→対処を選ぶ」遊びにする。
    if (boss.GetState() == BossState::Warning) {
        const char* guide = "DODGE!";
        // 【分岐表】現在の状態/攻撃種類に応じて、実行する処理を分ける。
        switch (boss.GetSelectedAttack()) {
        case BossAttack::Rush: guide = "RUSH: FLOOR LANE - JUMP / WIRE!";break;
        case BossAttack::Shot: guide = "SHOT: TWO TRIPLE VOLLEYS - KEEP MOVING!";break;
        case BossAttack::Press:guide = "PRESS: LEAVE THE RED FLOOR MARK!";break;
        case BossAttack::Top:  guide = "TOP: BOUNCES AT WALL - WATCH RETURN!";break;
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
        Novice::ScreenPrintf(555, 300, "STAGE 1 CLEAR!");
        Novice::ScreenPrintf(518, 330, "PRESS R TO RETRY");
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
}
// ------------------------------------------------------------------------
// 【関数】GetGravityScale()
// 何をする？ このステージで使う重力方向のベクトルを返す。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
Vector2 GetGravityScale(void) { return kGravityScale; }
