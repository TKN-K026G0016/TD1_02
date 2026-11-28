#include "AssetEditor.h"
#include "CameraBackgroundTuning.h"
#include "PortalArena.h"
#include "ExactPlayerBattle.h"
#include "MiniUnityEditor.h"
#include "PortalDraw.h"
#include "PortalMath.h"

#include "Texture.h"
#include "Effect.h"

#include <Novice.h>
#include <cmath>
using namespace PortalMath;
namespace {
    constexpr int kEditorGripCapacity = MiniUnityEditor::kMaxEditorGripPoints;

    // 背景のみスクロール。ワールド座標/当たり判定へは干渉しない。
    // 描画済み画像ハンドルを再利用するので、フレーム中に画像をロードしない。
    bool DrawParallax(AssetEditor::Slot background) {
        using namespace CameraBackgroundTuning;
        const Settings& cfg = Current();
        if (!cfg.parallax) return AssetEditor::DrawBackground(background);
        const AssetEditor::Slot slots[3] = { background,AssetEditor::Custom1,AssetEditor::Custom2 };
        bool drew = false;
        for (int i = 0;i < 3;++i) {
            const auto& a = AssetEditor::assets[slots[i]];
            if (a.handle < 0 || a.width <= 0 || a.height <= 0) continue;
            const float scale = AssetEditor::Scale(slots[i]);
            const int w = (std::max)(1, int(1280.0f * scale));
            const int h = (std::max)(1, int(720.0f * scale));
            if (w > 8192 || h > 8192) continue;
            const float raw = (cameraX - 640.0f) * cfg.speeds[i] + backgroundTime * cfg.autoSpeed[i];
            const float wrapped = std::fmod(raw, float(w));
            const float shift = wrapped < 0 ? wrapped + float(w) : wrapped;
            const int y = (720 - h) / 2;
            // 必要な枚数だけを描画。無制限の繰り返しを避ける。
            for (int x = -int(shift) - w;x < 1280;x += w) {
                Novice::DrawQuad(x, y, x + w, y, x, y + h, x + w, y + h,
                    0, 0, a.width, a.height, a.handle, WHITE);
            }
            drew = true;
        }
        return drew;
    }
    void DrawCoaster(int frame) {
        if (!DrawParallax(AssetEditor::Background2))
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
// 【関数】PortalArena::DrawWorld()
// 何をする？ 背景、床、移動杭などを描画。ボスより先に呼ぶ。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::DrawWorld() const {
    const int anchorCount = ActiveAnchorCount();
    Vector2 sceneGrips[kEditorGripCapacity]{};
    const int additionalCount = CollectSceneGrips(sceneGrips, kEditorGripCapacity);
    const int fixedCount = anchorCount + additionalCount;
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
        if (!DrawParallax(AssetEditor::Background1))
            PortalDraw::Box({ 640.0f,360.0f }, 1500.0f, 800.0f, 0xAECBD7FF);
        PortalDraw::Box({ 640.0f,26.0f }, 1270.0f, 76.0f, 0x31364EFF);

        PortalDraw::Box({ 640.0f,64.0f }, 1270.0f, 5.0f, 0xA2A8C4FF);

#pragma region 背景描画
        //DrawTextureAsObj(bg1Stage2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);
        //DrawTextureAsObj(bg2Stage2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);
        //DrawTextureAsObj(bg3Stage2Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);

        DrawTextureAsObj(bgForm1Texture, { 640, 360 }, { 1280, 720 }, 0.0f, false);

        //背景エフェクト
        DrawEffectAsBG();

#pragma endregion


    }
    for (int i = 0;i < anchorCount;++i) {
        if (!MovingGpAvailable(i)) continue;
        const Vector2 a = AnchorPosition(i);
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
    // ImGuiで配置したGP：実際に触手の接続候補となる点を強調表示。
    for (int i = 0; i < additionalCount; ++i) {
        const Vector2 p = sceneGrips[i];
        PortalDraw::Box(p, 24.0f, 24.0f, 0xAD78EFFF);
        PortalDraw::Box(p, 13.0f, 13.0f, 0xFFE0B5FF);
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
        player_.GetSelectedAnchor() == fixedCount + i) {
        const Vector2 p = armorPoints_[i];
        PortalDraw::Box({ p.x,p.y + 13.0f }, 24, 3, 0xFFFFFFFF);
        PortalDraw::Box({ p.x,p.y - 13.0f }, 24, 3, 0xFFFFFFFF);
    }
    const int idx = player_.GetSelectedAnchor();
    const int armorEnd = fixedCount + BossBattleSettings::kArmorHooks;
    const int total = armorEnd + kExtraBossPoints;
    if (idx >= 0 && idx < total) {
        Vector2 p = {};
        bool valid = false;
        if (idx < anchorCount) { p = AnchorPosition(idx); valid = MovingGpAvailable(idx); }
        else if (idx < fixedCount) { p = sceneGrips[idx - anchorCount]; valid = true; }
        else if (idx < armorEnd) {
            const int part = idx - fixedCount;
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
    const int anchorCount = ActiveAnchorCount();
    Vector2 sceneGrips[kEditorGripCapacity]{};
    const int extraGP = CollectSceneGrips(sceneGrips, kEditorGripCapacity);
    const int fixedCount = anchorCount + extraGP;
    if (target >= anchorCount && target < fixedCount)
        status = "SCENE GP: GRAPPLE FOR DODGE";
    else if (target >= fixedCount && target < fixedCount + BossBattleSettings::kArmorHooks)
        status = "BOSS SCREW: WIND TO LOOSEN, TWO-ARM YANK TO PEEL";
    else if (target >= fixedCount + BossBattleSettings::kArmorHooks)
        status = "BOSS POINT: GRAPPLE TO MOVE";
    Novice::ScreenPrintf(18, 52, "%s  /  %s", connected_ ? "GAMEPAD" : "KEYBOARD", status);
    const int yankState = armorTension_.LastYankState();
    if (yankState == 1) Novice::ScreenPrintf(18, 127, "YANK: TENSION OK (armor only)");
    else if (yankState == 2) Novice::ScreenPrintf(18, 127, "YANK FAILED: LOOSEN FIRST OR USE FARTHER GP");
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

