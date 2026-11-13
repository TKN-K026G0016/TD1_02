#include "PortalArena.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include <Novice.h>
#include <cmath>

using namespace PortalMath;
void PortalArena::Initialize() {
    player_.Initialize({ 420.0f,110.0f });
    frame_ = 0;connected_ = false;
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) { armorPointEnabled_[i] = false;armorPoints_[i] = {}; }
}
void PortalArena::SetBattleSettings(const BossBattleSettings& settings) {
    if (!settings.Valid())return;
    anchorCount_ = settings.anchorCount;
    for (int i = 0;i < anchorCount_;++i)anchors_[i] = { settings.anchors[i].x,settings.anchors[i].y };
}
void PortalArena::SetBossArmorTargets(const Vector2 points[BossBattleSettings::kArmorHooks], const bool enabled[BossBattleSettings::kArmorHooks]) {
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) { armorPointEnabled_[i] = enabled[i];armorPoints_[i] = points[i]; }
}
bool PortalArena::Update() {
    const PortalCommands commands = input_.Read();
    connected_ = commands.connected;
    if (commands.reset) { Initialize();return true; }
    // 【ターゲット一覧】移動用杭3つ＋ボス外装3つ。
    // 取り外した外装は巨大な距離へ追い出して選択できなくする。
    Vector2 points[kMaxAnchors + BossBattleSettings::kArmorHooks] = {};
    for (int i = 0;i < anchorCount_;++i) points[i] = anchors_[i];
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) points[anchorCount_ + i] = armorPointEnabled_[i]
        ? armorPoints_[i] : Vector2{ 100000.0f,100000.0f };
    if (!player_.IsDefeated()) player_.Update(commands, points, anchorCount_ + BossBattleSettings::kArmorHooks, anchorCount_);
    ++frame_;
    return false;
}
void PortalArena::Draw() const {
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
    // 【ボス外装の取っ掛かり】照準が合った板だけ白い四角で強調する。
    // 通常はボス自身が十字を描くので追加描画は選択中の2BOXだけ。
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) if (armorPointEnabled_[i] &&
        player_.GetSelectedAnchor() == anchorCount_ + i) {
        const Vector2 p = armorPoints_[i];
        PortalDraw::Box({ p.x,p.y + 13.0f }, 24, 3, 0xFFFFFFFF);
        PortalDraw::Box({ p.x,p.y - 13.0f }, 24, 3, 0xFFFFFFFF);
    }
    if (player_.GetSelectedAnchor() >= 0) {
        const int idx = player_.GetSelectedAnchor();
        const Vector2 target = idx < anchorCount_ ? anchors_[idx] : armorPoints_[idx - anchorCount_];
        PortalDraw::Line(player_.GetPosition(), target, 0xB5F6C966);
    }
    player_.Draw();
}
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
    if (target >= anchorCount_ && !player_.IsPullingArmor()) status = "SCREW: AIM AT THE CORNER AND LOCK";
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
