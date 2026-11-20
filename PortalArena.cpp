// ============================================================================
// 【このファイルの役割】PortalArena.cpp — ボス戦の床・杭・プレイヤー管理
// 移動杭と外装ネジの座標をまとめ、プレイヤー更新へ渡す。背景/杭とプレイヤーは別々に描く。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

#include "PortalArena.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include <Novice.h>
#include <cmath>

using namespace PortalMath;
// ------------------------------------------------------------------------
// 【関数】PortalArena::Initialize()
// 何をする？ オブジェクトを使い始めるときに値を初期状態へ戻す。リトライ時にも重要。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::Initialize() {
    player_.Initialize({ 420.0f,110.0f });
    frame_ = 0;connected_ = false;mashPulse_ = false;
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) { armorPointEnabled_[i] = false;armorPoints_[i] = {}; }
    for (int i = 0;i < kExtraBossPoints;++i) { extraPointEnabled_[i] = false;extraPoints_[i] = {}; }
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::SetBattleSettings()
// 何をする？ エディターで保存した戦闘設定を、このオブジェクトへ反映する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::SetBattleSettings(const BossBattleSettings& settings) {
    if (!settings.Valid())return;
    anchorCount_ = settings.anchorCount;
    for (int i = 0;i < anchorCount_;++i)anchors_[i] = { settings.anchors[i].x,settings.anchors[i].y };
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::SetBossArmorTargets()
// 何をする？ 現在残っているボス外装ネジの座標をプレイヤーへ渡すために保存する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::SetBossArmorTargets(const Vector2 points[BossBattleSettings::kArmorHooks], const bool enabled[BossBattleSettings::kArmorHooks]) {
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) { armorPointEnabled_[i] = enabled[i];armorPoints_[i] = points[i]; }
}
void PortalArena::SetBossExtraTargets(const Vector2 points[kExtraBossPoints], const bool enabled[kExtraBossPoints]) {
    for (int i = 0; i < kExtraBossPoints; ++i) { extraPointEnabled_[i] = enabled[i];extraPoints_[i] = points[i]; }
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::Update()
// 何をする？ 1フレーム分の入力・物理・状態遷移を進める。描画ではなく内部状態を更新する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
bool PortalArena::Update() {
    // 【注目】【入力取得】キーボード／ゲームパッドを共通形式へ変換。
    const PortalCommands commands = input_.Read();
    connected_ = commands.connected;
    mashPulse_ = commands.mashTap;
    if (commands.reset) { Initialize();return true; }
    // 【ターゲット一覧】設定された移動用杭＋ボス外装の四隅ネジ4点。
    // 取り外した外装は巨大な距離へ追い出して選択できなくする。
    // 【注目】【目標配列】前半＝移動杭、後半＝ボスのネジ。固定杭の個数を境界としてPlayerに渡す。
    Vector2 points[kMaxAnchors + BossBattleSettings::kArmorHooks + kExtraBossPoints] = {};
    for (int i = 0;i < anchorCount_;++i) points[i] = anchors_[i];
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) points[anchorCount_ + i] = armorPointEnabled_[i]
        ? armorPoints_[i] : Vector2{ 100000.0f,100000.0f };
    // ジェットとパイプも部位剥がしの対象として数える。
    // 目と本体はBoss側で破壊不可にしている。
    // 【プレイヤー更新】敵に倒されていなければ物理・攻撃・触手を進める。
    for (int i = 0; i < kExtraBossPoints; ++i)
        points[anchorCount_ + BossBattleSettings::kArmorHooks + i] = extraPointEnabled_[i]
        ? extraPoints_[i] : Vector2{ 100000.0f,100000.0f };
    if (!player_.IsDefeated()) player_.Update(commands, points,
        anchorCount_ + BossBattleSettings::kArmorHooks + kExtraBossPoints,
        anchorCount_, BossBattleSettings::kArmorHooks + kExtraBossPoints);
    ++frame_;
    return false;
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::DrawWorld()
// 何をする？ 背景、床、移動杭などを描画。ボスより先に呼ぶ。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::DrawWorld() const {
    // 【描画制限対策】固定アリーナは背景1BOX＋床2BOXのみ。
    // 上空の出口や高い壁は置かず、ワイヤーを「攻撃回避」に使わせる。
    PortalDraw::Box({ 640.0f,360.0f }, 1500.0f, 800.0f, 0xAECBD7FF);
    PortalDraw::Box({ 640.0f,26.0f }, 1270.0f, 76.0f, 0x31364EFF);
    PortalDraw::Box({ 640.0f,64.0f }, 1270.0f, 5.0f, 0xA2A8C4FF);
    for (int i = 0;i < anchorCount_;++i) {
        const Vector2 a = anchors_[i];
        const float pulse = 4.0f + 2.5f * std::sin(frame_ * 0.13f + i);
        PortalDraw::Box(a, 32.0f + pulse, 32.0f + pulse, 0x734DC3FF);
        PortalDraw::Box(a, 19.0f, 19.0f, 0xFFD0AAFF);
        if (i == player_.GetSelectedAnchor()) {
            // 選択中は4辺のBOXで大きく強調。これだけで狙いやすさが上がる。
            PortalDraw::Box({ a.x - 27,a.y }, 3, 35, 0xFFFFFFFF);
            PortalDraw::Box({ a.x + 27,a.y }, 3, 35, 0xFFFFFFFF);
            PortalDraw::Box({ a.x,a.y + 27 }, 35, 3, 0xFFFFFFFF);
            PortalDraw::Box({ a.x,a.y - 27 }, 35, 3, 0xFFFFFFFF);
        }
    }
    // 【ボス外装の取っ掛かり】照準が合ったネジだけ白い四角で強調する。
    // 通常はボス自身が十字を描くので追加描画は選択中の2BOXだけ。
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) if (armorPointEnabled_[i] &&
        player_.GetSelectedAnchor() == anchorCount_ + i) {
        const Vector2 p = armorPoints_[i];
        PortalDraw::Box({ p.x,p.y + 13.0f }, 24, 3, 0xFFFFFFFF);
        PortalDraw::Box({ p.x,p.y - 13.0f }, 24, 3, 0xFFFFFFFF);
    }
    const int idx = player_.GetSelectedAnchor();
    const int armorEnd = anchorCount_ + BossBattleSettings::kArmorHooks;
    const int total = armorEnd + kExtraBossPoints;
    if (idx >= 0 && idx < total) {
        Vector2 p = {};
        bool valid = false;
        if (idx < anchorCount_) { p = anchors_[idx]; valid = true; }
        else if (idx < armorEnd) {
            const int part = idx - anchorCount_;
            p = armorPoints_[part];valid = armorPointEnabled_[part];
        }
        else {
            const int part = idx - armorEnd;
            p = extraPoints_[part];valid = extraPointEnabled_[part];
        }
        if (valid) {
            PortalDraw::Line(player_.GetPosition(), p, 0xB5F6C966);
            if (idx >= armorEnd) {
                PortalDraw::Box({ p.x - 18.0f,p.y }, 3.0f, 34.0f, 0xFFFFFFFF);
                PortalDraw::Box({ p.x + 18.0f,p.y }, 3.0f, 34.0f, 0xFFFFFFFF);
            }
        }
    }
}

// ------------------------------------------------------------------------
// 【関数】PortalArena::DrawPlayer()
// 何をする？ プレイヤーと触手を描画。ボスより後に呼ぶことで手前になる。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::DrawPlayer() const {
    // ボスの描画後に呼ぶと、プレイヤーと触手がボスより手前に描かれる。
    player_.Draw();
}

// ------------------------------------------------------------------------
// 【関数】PortalArena::Draw()
// 何をする？ 現在の状態を画面に表示する。基本的にゲームの座標やHPは変えない。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::Draw() const {
    // 互換用。Stage.cppでは DrawWorld/DrawPlayer を別々に呼ぶ。
    DrawWorld();
    DrawPlayer();
}

// ------------------------------------------------------------------------
// 【関数】PortalArena::DrawHUD()
// 何をする？ 操作説明やHP等の固定UIを描く。カメラを通さない。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::DrawHUD() const {
    Novice::ScreenPrintf(18, 12, "LEFT: MOVE/UP JUMP  RIGHT: 8-WAY ATTACK  HOLD RIGHT ON LOCK: WIRE");
    Novice::ScreenPrintf(18, 32, "GROUND RUSH! JUMP / WIRE OVER IT -> BOSS STUN -> ATTACK!");
    // 四隅のネジは狙って接続した時点で1つ解除。4個目で外装がめくれる。
    const char* status = player_.IsPullingArmor() ? "ARMOR SCREW: LOCK -> REMOVE" :
        player_.IsAutoLaunching() ? "WIRE LAUNCH!" :
        player_.IsGripping() ? "WIRE SPIN: LIGHT AUTO ATTACK" :
        player_.IsSpinning() ? "AIR SPIN ATTACK" :
        player_.GetSelectedAnchor() >= 0 ? "LOCK: HOLD RIGHT STICK -> AUTO GRAPPLE" : "RIGHT STICK: INSTANT TENTACLE ATTACK";
    const int target = player_.GetSelectedAnchor();
    if (target >= anchorCount_ && target < anchorCount_ + BossBattleSettings::kArmorHooks)
        status = "BOSS PART: GRAPPLE TO PEEL / LAUNCH";
    else if (target >= anchorCount_ + BossBattleSettings::kArmorHooks)
        status = "BOSS POINT: GRAPPLE TO MOVE";
    Novice::ScreenPrintf(18, 52, "%s  /  %s", connected_ ? "GAMEPAD" : "KEYBOARD", status);
    if (player_.IsPullingArmor()) {
        const float progress = player_.GetArmorPullProgress();
        // 文字とBOX2個だけ。新しいパーティクルや長いロープは描かない。
        Novice::DrawBox(18, 72, 180, 8, 0, 0x222735FF, kFillModeSolid);
        // 22F経過後はずっと緑。Jを離せば確実に剥がれる。
        const unsigned int color = player_.IsArmorPullReady() ?
            0x65E4A5FF : 0xFFB468FF;
        Novice::DrawBox(18, 72, static_cast<int>(180 * progress), 8, 0, color, kFillModeSolid);
    }
}
