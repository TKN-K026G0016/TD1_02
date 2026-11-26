#include "AssetEditor.h"
// ============================================================================
// 【このファイルの役割】PortalArena.cpp — ボス戦の床・杭・プレイヤー管理
// 移動杭と外装ネジの座標をまとめ、プレイヤー更新へ渡す。背景/杭とプレイヤーは別々に描く。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

#include "PortalArena.h"
#include "ExactPlayerBattle.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include "Texture.h"
#include <Novice.h>
#include <cmath>

#pragma region データ: 背景
//Form1
Texture bgForm1Texture;
//form2
Texture bg1Form2Texture;
Texture bg2Form2Texture;
Texture bg3Form2Texture;

#pragma endregion

using namespace PortalMath;
namespace {
    constexpr Vector2 kCoasterAnchors[] = { {155, 495}, {480, 525}, {800, 525}, {1125, 495} };
    constexpr int kCoasterAnchorCount = 2; // Stage 2: fewer moving grip points

    void DrawCoaster(int frame) {
        if (!AssetEditor::DrawBackground(AssetEditor::Background2))
            PortalDraw::Box({ 640, 360 }, 1500, 800, 0x19364CFF);
        const int farShift = (frame % 240) * 3;
        for (int i = -1; i < 10; ++i) {
            const float x = static_cast<float>(i * 240 - farShift);
            const float height = 100.0f + ((i + 1) % 3) * 40.0f;
            PortalDraw::Box({ x, 70.0f + height * 0.5f }, 160, height, 0x28526AFF);
        }
        const int railShift = (frame % 10) * 12;
        PortalDraw::Box({ 640, 15 }, 1400, 7, 0x86ACB5FF);
        for (int i = 0; i < 12; ++i) {
            const float x = static_cast<float>(i * 120 - railShift);
            PortalDraw::Line({ x, 15 }, { x - 38.0f, -7 }, 0x43667DFF);
        }
        PortalDraw::Box({ 640, 44 }, 1270, 34, 0x2B3555FF);
        PortalDraw::Box({ 640, 62 }, 1270, 5, 0xFFBB62FF);
        for (int i = 0; i < 8; ++i) {
            const float x = 80.0f + 160.0f * i;
            PortalDraw::Box({ x, 44 }, 110, 16, 0x52658AFF);
            PortalDraw::Circle({ x, 21 }, 12.0f, 0x141C30FF);
        }
        PortalDraw::Line({ 55, 515 }, { 1225, 515 }, 0x648DA4FF);
    }
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::Initialize()
// 何をする？ オブジェクトを使い始めるときに値を初期状態へ戻す。リトライ時にも重要。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::Initialize() {
    player_.Initialize({ 420.0f,110.0f });
    ExactPlayerBattle::Reset();
    coasterStage_ = false;
    frame_ = 0;stage2_.Reset();armorTension_.Reset();connected_ = false;mashPulse_ = false;
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) { armorPointEnabled_[i] = false;armorPoints_[i] = {}; }
    for (int i = 0;i < kExtraBossPoints;++i) { extraPointEnabled_[i] = false;extraPoints_[i] = {}; }

#pragma region テクスチャ読み込み処理
    bgForm1Texture = GetTexture(TextureType::BackgroundForm1);
    bg1Form2Texture = GetTexture(TextureType::Background1Form2);
    bg2Form2Texture = GetTexture(TextureType::Background2Form2);
    bg3Form2Texture = GetTexture(TextureType::Background3Form2);

#pragma endregion

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
    const int anchorCount = coasterStage_ ? kCoasterAnchorCount : anchorCount_;
    const Vector2* anchors = anchors_;
    // 【注目】【入力取得】キーボード／ゲームパッドを共通形式へ変換。
    const PortalCommands commands = input_.Read();
    connected_ = commands.connected;
    mashPulse_ = commands.mashTap;
    if (commands.reset) { Initialize();return true; }
    // 【ターゲット一覧】設定された移動用杭＋ボス外装の四隅ネジ4点。
    // 取り外した外装は巨大な距離へ追い出して選択できなくする。
    // 【注目】【目標配列】前半＝移動杭、後半＝ボスのネジ。固定杭の個数を境界としてPlayerに渡す。
    Vector2 points[kMaxAnchors + BossBattleSettings::kArmorHooks + kExtraBossPoints] = {};
    for (int i = 0;i < anchorCount;++i) points[i] = coasterStage_ ? stage2_.Anchor(i) : anchors[i];
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) points[anchorCount + i] = armorPointEnabled_[i]
        ? armorPoints_[i] : Vector2{ 100000.0f,100000.0f };
    // ジェットとパイプも部位剥がしの対象として数える。
    // 目と本体はBoss側で破壊不可にしている。
    // 【プレイヤー更新】敵に倒されていなければ物理・攻撃・触手を進める。
    for (int i = 0; i < kExtraBossPoints; ++i)
        points[anchorCount + BossBattleSettings::kArmorHooks + i] = extraPointEnabled_[i]
        ? extraPoints_[i] : Vector2{ 100000.0f,100000.0f };
    // Run the exact source physical simulation once, not PortalPlayer's old physics.
    static bool f2Prev = false;
    char modeKeys[256] = {}; Novice::GetHitKeyStateAll(modeKeys);
    const bool f2Now = modeKeys[DIK_F2] != 0;
    if (f2Now && !f2Prev) ExactPlayerBattle::SetAlternate(!ExactPlayerBattle::IsAlternate());
    f2Prev = f2Now;
    if (!player_.IsDefeated()) {
        ExactPlayerBattle::SetBossAnchorStart(anchorCount);
        ExactPlayerBattle::Update(points, anchorCount + BossBattleSettings::kArmorHooks + kExtraBossPoints);
        const auto snap = ExactPlayerBattle::GetSnapshot();
        player_.SyncSamplePose(snap.position, snap.velocity, snap.gripping, snap.pulling);
        // Loosening is preparation, not damage. Only a real two-arm yank
        // can produce an armor peel request. GP removal remains sample-owned.
        const int part = armorTension_.Update(snap, anchorCount,
            BossBattleSettings::kArmorHooks + kExtraBossPoints);
        if (part >= 0) player_.QueueSampleArmorPeel(part);
    }
    ++frame_;
    return false;
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::DrawWorld()
// 何をする？ 背景、床、移動杭などを描画。ボスより先に呼ぶ。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::DrawWorld() const {
    const int anchorCount = coasterStage_ ? kCoasterAnchorCount : anchorCount_;
    const Vector2* anchors = anchors_;
    // 【描画制限対策】固定アリーナは背景1BOX＋床2BOXのみ。
    // 上空の出口や高い壁は置かず、ワイヤーを「攻撃回避」に使わせる。
    if (coasterStage_) {
        DrawCoaster(frame_);
        if (stage2_.IsSplit()) {
            PortalDraw::Box({ 640.0f,44.0f }, 180.0f, 52.0f, 0x19364CFF);
            PortalDraw::Box({ 535.0f,44.0f }, 14.0f, 60.0f, 0xEB687AFF);
            PortalDraw::Box({ 745.0f,44.0f }, 14.0f, 60.0f, 0xEB687AFF);

            DrawTextureAsObj(bg1Form2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);
            DrawTextureAsObj(bg2Form2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);
            DrawTextureAsObj(bg3Form2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);
        }
    }
    else {
        if (!AssetEditor::DrawBackground(AssetEditor::Background1))
            PortalDraw::Box({ 640.0f,360.0f }, 1500.0f, 800.0f, 0xAECBD7FF);
        PortalDraw::Box({ 640.0f,26.0f }, 1270.0f, 76.0f, 0x31364EFF);

        PortalDraw::Box({ 640.0f,64.0f }, 1270.0f, 5.0f, 0xA2A8C4FF);

#pragma region 背景描画
        //DrawTextureAsObj(bg1Stage2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);
        //DrawTextureAsObj(bg2Stage2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);
        //DrawTextureAsObj(bg3Stage2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);

        DrawTextureAsObj(bgForm1Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);

#pragma endregion


    }
    for (int i = 0;i < anchorCount;++i) {
        const Vector2 a = coasterStage_ ? stage2_.Anchor(i) : anchors[i];
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
    // Remaining screw heads show persistent looseness (amber ring).
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) {
        if (!armorPointEnabled_[i]) continue;
        const float loose = armorTension_.Loosen(i);
        const Vector2 p = armorPoints_[i];
        PortalDraw::Box({ p.x,p.y + 19.0f }, 32.0f, 4.0f, 0x1F2232FF);
        PortalDraw::Box({ p.x - 16.0f + 16.0f * loose,p.y + 19.0f },
            32.0f * loose, 4.0f, 0xFFCB65FF);
    }
    // 【ボス外装の取っ掛かり】照準が合ったネジだけ白い四角で強調する。
    // 通常はボス自身が十字を描くので追加描画は選択中の2BOXだけ。
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) if (armorPointEnabled_[i] &&
        player_.GetSelectedAnchor() == anchorCount + i) {
        const Vector2 p = armorPoints_[i];
        PortalDraw::Box({ p.x,p.y + 13.0f }, 24, 3, 0xFFFFFFFF);
        PortalDraw::Box({ p.x,p.y - 13.0f }, 24, 3, 0xFFFFFFFF);
    }
    const int idx = player_.GetSelectedAnchor();
    const int armorEnd = anchorCount + BossBattleSettings::kArmorHooks;
    const int total = armorEnd + kExtraBossPoints;
    if (idx >= 0 && idx < total) {
        Vector2 p = {};
        bool valid = false;
        if (idx < anchorCount) { p = coasterStage_ ? stage2_.Anchor(idx) : anchors[idx]; valid = true; }
        else if (idx < armorEnd) {
            const int part = idx - anchorCount;
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
    if (coasterStage_) stage2_.Draw();
    ExactPlayerBattle::Draw();
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
    if (coasterStage_) stage2_.DrawHud();
    Novice::ScreenPrintf(18, 12, "A/D MOVE  SPACE JUMP  Q/J LEFT ARM  E/K RIGHT ARM  SHIFT RELEASE");
    Novice::ScreenPrintf(18, 190, "F2 MODE: %s / 2 TENTACLES",
        ExactPlayerBattle::IsAlternate() ? "ALTERNATE" : "NON ALTERNATE");
    Novice::ScreenPrintf(18, 32, "%s", coasterStage_
        ? "HOLD RIGHT ON AN UPPER GRIP POINT TO DODGE -> COUNTER"
        : "GROUND RUSH! JUMP / WIRE OVER IT -> BOSS STUN -> ATTACK!");
    // Screw loosening prepares a lower-force two-arm peel; it cannot destroy armor alone.
    const char* status = player_.IsPullingArmor() ? "ARMOR SCREW: LOCK -> REMOVE" :
        player_.IsAutoLaunching() ? "WIRE LAUNCH!" :
        player_.IsGripping() ? "WIRE SPIN: LIGHT AUTO ATTACK" :
        player_.IsSpinning() ? "AIR SPIN ATTACK" :
        player_.GetSelectedAnchor() >= 0 ? "LOCK: HOLD RIGHT STICK -> AUTO GRAPPLE" : "RIGHT STICK: INSTANT TENTACLE ATTACK";
    const int target = player_.GetSelectedAnchor();
    const int anchorCount = coasterStage_ ? kCoasterAnchorCount : anchorCount_;
    if (target >= anchorCount && target < anchorCount + BossBattleSettings::kArmorHooks)
        status = "BOSS SCREW: WIND TO LOOSEN, TWO-ARM YANK TO PEEL";
    else if (target >= anchorCount + BossBattleSettings::kArmorHooks)
        status = "BOSS POINT: GRAPPLE TO MOVE";
    Novice::ScreenPrintf(18, 52, "%s  /  %s", connected_ ? "GAMEPAD" : "KEYBOARD", status);
    const int screw = armorTension_.DisplayTarget();
    if (screw >= 0) {
        const float loose = armorTension_.Loosen(screw);
        const float required = armorTension_.RequiredTension(screw);
        Novice::ScreenPrintf(18, 106, "SCREW %d  LOOSEN %d%%  REQUIRED TENSION %.0f",
            screw, static_cast<int>(loose * 100.0f), required);
        Novice::DrawBox(18, 126, 220, 10, 0, 0x263348FF, kFillModeSolid);
        Novice::DrawBox(18, 126, static_cast<int>(220 * loose), 10, 0, 0xFFBD60FF, kFillModeSolid);
    }
    Novice::ScreenPrintf(18, 142, "WIND: ONE ARM / PEEL: BOSS SCREW + DISTANT GP WITH BOTH ARMS");
    if (armorTension_.ResultFrames() > 0)
        Novice::ScreenPrintf(18, 160, armorTension_.LastSuccess() ?
            "TENSION OK! ARMOR REMOVED" : "TENSION TOO LOW - LOOSEN FIRST (OR USE DISTANT GP)");
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

// Stage 2 only. No changes to sample player movement or tentacle rendering.
void PortalArena::UpdateStage2(Vector2 bossPosition, int bossHp, int bossMaxHp) {
    if (!coasterStage_) return;
    stage2_.Update(player_.GetPosition(), bossPosition, bossHp, bossMaxHp,
        player_.IsGripping() || player_.HasWireMomentum());
}
int PortalArena::Stage2HitPlayer(Vector2 player, Vector2* source) {
    return coasterStage_ ? stage2_.CheckHit(player, 24.0f, source) : 0;
}
