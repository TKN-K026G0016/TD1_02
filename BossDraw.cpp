#include "BossBase.h"
#include "BossPartsEditor.h"
#include "Stage2Tuning.h"
#include "BossCombatEditorSettings.h"
#include "Camera.h"
#include "Texture.h"
#include "PortalDraw.h"
#include <cmath>
#include <cstring>
#include <Novice.h>
#include "BossHelpers.h"
#include "PortalArena.h"
using namespace BossHelpers;
using PortalMath::Clamp;
using PortalMath::Abs;
using PortalMath::Dist2;
using PortalMath::Approach;

#pragma region データ: テクスチャ

static Texture boltTexture = {};
static Texture boltMeterTexture = {};

#pragma endregion

namespace {
    // コマ・ヨーヨーは既存の専用PNGを原寸表示する。
    // 画像はキャッシュし、毎フレームLoadTextureを呼ばない。
    void DrawToySprite(const char* path, Vector2 center, int pixelSize, float angle, int& handle) {
        if (handle == -2) handle = Novice::LoadTexture(path);
        if (handle < 0) {
            PortalDraw::RotatedBox(center, static_cast<float>(pixelSize) * 0.5f,
                static_cast<float>(pixelSize) * 0.5f, angle, 0xC9C4DAFF);
            return;
        }
        const Vector2i screen = PortalDraw::ToScreen(center);
        const float scale = GetCameraZoom(); // 1.0倍が基準。256px/128pxのまま描画。
        const int size = static_cast<int>(pixelSize * scale);
        Novice::DrawSprite(screen.x - size / 2, screen.y - size / 2,
            handle, scale, scale, -angle, 0xFFFFFFFF);
    }
    void DrawSpinningTop(Vector2 center, int frame) {
        constexpr int kSize = 256;
        static const int handle = Novice::LoadTexture("./resources/texture/boss/form1/gyro.png");
        // 先端を下に向けたまま、横幅の変化で軸回転を表現する。
        const float widthFactor = TopWidthFactor(frame);
        if (handle < 0) {
            PortalDraw::Box(center, kSize * widthFactor * 0.5f, kSize * 0.5f, 0xC9C4DAFF);
            return;
        }
        const float scale = GetCameraZoom();
        const float scaleX = scale * widthFactor;
        const Vector2i screen = PortalDraw::ToScreen(center);
        const int width = static_cast<int>(kSize * scaleX);
        const int height = static_cast<int>(kSize * scale);
        Novice::DrawSprite(screen.x - width / 2, screen.y - height / 2,
            handle, scaleX, scale, 0.0f, 0xFFFFFFFF);
    }
}

/// <summary>
/// テクスチャの取得
/// </summary>
void Boss::LoadTexture(void) const {
    boltTexture = GetTexture(TextureType::Bolt);
    boltMeterTexture = GetTexture(TextureType::BoltMeter);
}

/// <summary>
/// ネジの描画処理
/// </summary>
void Boss::DrawBolts(const PortalArena& arena) const {
    for (int i = 0; i < kAccessoryGripCount; ++i) {
        if (!IsAccessoryGripEnabled(i)) continue;
        DrawTextureAsObj(boltTexture, GetAccessoryGripPoint(i), { 32,32 }, 0.0f, false);

        //ゆるみ度ゲージ

        const int screwIndex = BossBattleSettings::kArmorHooks + i;

        float gaugeRate = arena.GetScrewLoose(screwIndex);
        DrawGaugeYAsObj(boltMeterTexture, GetAccessoryGripPoint(i), { 32, 32 }, gaugeRate);
    }

    for (int i = 0; i < BossStage1::kArmorPlates; ++i) {
        if (!IsArmorAttached(i)) continue;

        DrawTextureAsObj(boltTexture, GetArmorGripPoint(i), { 32,32 }, 0.0f, false);

        //ゆるみ度ゲージ
        float gaugeRate = arena.GetScrewLoose(i);
        DrawGaugeYAsObj(boltMeterTexture, GetArmorGripPoint(i), { 32, 32 }, gaugeRate);
    }
}

/// <summary>
/// 描画処理
/// </summary>
void Boss::Draw(const PortalArena& arena) const {
    // ステージ2の退場飛行。画面端に旧VFXや尻尾が残らないよう、
    // 機体の中心が退場したらボスの描画一式を停止する。
    // 攻撃予兆は機体が場外でも表示する。機体・GPだけを場外で非表示にする。
    bool fullyOutside = false;
    if (coasterStage_) {
        const float half = Stage2VisualHalfWidth();
        // 向きと回転を考慮した保守的な表示判定。画面端の機体の一部を途中で消さない。
        const float extent = (std::max)(half, Stage2VisualHalfHeight());
        fullyOutside = position_.x < -extent || position_.x > 1280.0f + extent ||
            position_.y < -extent || position_.y > 720.0f + extent;
    }
    // ミサイルとカッターにも30Fの予兆を付ける（実行中は表示しない）。
    if (coasterStage_ && state_ == BossState::Stage2Special && stateFrame_ < 30) {
        const unsigned int blink = (stateFrame_ / 6) % 2 ? 0xFF6A6ADA : 0xFF6A6A66;
        if (stage2SpecialMode_ == 1) {
            const Vector2 t = targetPosition_ ? *targetPosition_ : Vector2{ 640.0f, 350.0f };
            PortalDraw::Line(position_, t, blink);
        }
        else if (stage2SpecialMode_ == 2) {
            // カッターは線で全軌道を描かず、発射前の短い点滅だけ示す。
            PortalDraw::Circle(position_, 42.0f, blink);
        }
    }
    // 【技が見て分かる予兆】種類を増やさず、各攻撃で異なる警告図形を1～2個だけ表示。
    // 大量パーティクルは使わない。攻撃開始後は予兆を消して判定と対応させる。
    if (state_ == BossState::Warning) {
        const unsigned int color = (stateFrame_ / 5) % 2 ? 0xF15D58AA : 0xF15D5844;
        const Vector2 target = targetPosition_ ? *targetPosition_ : position_;
        switch (nextAttack_) {
        case BossAttack::Rush:
            if (coasterStage_) {
                // ステージ2: 水平・高空・上昇・S字を実際の飛行式と同じ計算で予告。
                // 旧コードはどの軌道も水平線だったのでS字や上昇時に警告がずれていた。
                const float arc = Stage2Tuning::values.flybyArcHeight;
                constexpr float pi = 3.14159265f;
                Vector2 prev{};
                for (int i = 0; i <= 32; ++i) {
                    const float t = static_cast<float>(i) / 32.0f;
                    float y = flybyStartY_;
                    if (flybyStyle_ == 2) y += arc * t;
                    if (flybyStyle_ == 3) y += arc * std::sin(t * pi * 2.0f);
                    const Vector2 point = { rushDirection_ > 0 ? 1280.0f * t : 1280.0f * (1.0f - t), y };
                    if (i > 0) PortalDraw::Line(prev, point, color);
                    prev = point;
                }
            }
            else {
                PortalDraw::Box({ (BossStage1::kArenaLeft + BossStage1::kArenaRight) * 0.5f, rushTargetY_ },
                    BossStage1::kArenaRight - BossStage1::kArenaLeft, 7.0f, color);
            }
            break;
        case BossAttack::Press:
            if (coasterStage_) {
                // 縦突進: 実際に固定したX座標を画面全体へ予告する。
                // 攻撃の中心線と当たり判定幅を別々に示す。
                PortalDraw::Box({ pressTargetX_, 360.0f }, BossStage1::kBossWidth + 35.0f, 720.0f, 0xF15D5828);
                PortalDraw::Line({ pressTargetX_, 0.0f }, { pressTargetX_, 720.0f }, color);
            }
            else {
                PortalDraw::Box({ pressTargetX_,BossStage1::kFloorY + 5 }, BossStage1::kBossWidth + 35.0f, 7.0f, color);
            }
            break;
        case BossAttack::Shot:
            // 射線を1本だけ示す。発射後の弾の位置は固定される。
            PortalDraw::Line(position_, target, color);
            break;
        case BossAttack::Top:
            // 地面を走るコマ：床が危険になることを線で示す。
            PortalDraw::Box({ 640.0f,BossStage1::kFloorY + 24.0f }, 1080.0f, 3.0f, color);
            break;
        case BossAttack::YoYo:
            // プレイヤーの高度で往復するヨーヨー。
            PortalDraw::Line(position_, { target.x,target.y }, color);
            PortalDraw::Box(target, 28.0f, 3.0f, color);
            break;
        }
    }
    if (fullyOutside) return;
    // 第2ステージでは第1形態の装飾/尻尾VFXを出さない。
    if (!coasterStage_) visual_.DrawBehind(position_);
    float bodyLean = 0.0f;
    if (state_ != BossState::ArmorFalling && state_ != BossState::ArmorImpact &&
        state_ != BossState::ArmorStun && state_ != BossState::WakeBlast) {
        if (!armorAttached_[1]) bodyLean -= 0.055f;
        if (!armorAttached_[2]) bodyLean += 0.045f;
        if (!armorAttached_[3]) bodyLean += 0.065f;
    }


    // ステージ2は専用機体PNGを使用。第1ステージの機械パーツを流用しない。
    // GP・ネジの描画はこの後の共通処理を使うため、当たり判定は変更しない。
    if (BossPartsEditor::Active(coasterStage_ ? 1 : 0)) {
        // カスタムパーツはボスの描画だけ置き換える。GP・装甲・物理は既存を使用。
        const Vector2i point = PortalDraw::ToScreen(position_);
        const bool flip = coasterStage_ &&
            (state_ == BossState::Rush ? rushDirection_ > 0 : !stage2FacingLeft_);
        BossPartsEditor::DrawWorld(coasterStage_ ? 1 : 0, float(point.x), float(point.y),
            GetCameraZoom(), flip, [&](int link) {
                if (link >= 0 && link <= 3) return IsArmorAttached(link);
                if (link >= 10 && link <= 15) return IsAccessoryGripEnabled(link - 10);
                return true;
            });
    }
    else if (coasterStage_ && AssetEditor::Handle(AssetEditor::BossForm2) >= 0) {
        DrawCoasterArt(bodyLean);
    }
    else if (!coasterStage_) {
        DrawCopterArt(bodyLean);
    }
    else {
        // ステージ2でPNGが無い場合にも第1形態へ戻さず、仮の機体を表示。
        PortalDraw::RotatedBox(position_, 145.0f, 52.0f, 0.0f, 0xC8D7E8FF);
    }

    //ネジの描画
    DrawBolts(arena);

    //// ボス側のグリップポイントを画像より手前に表示する。
    //for (int i = 0; i < kAccessoryGripCount; ++i) {
    //    if (!IsAccessoryGripEnabled(i)) continue;
    //    const Vector2 point = GetAccessoryGripPoint(i);
    //    PortalDraw::Box(point, 11.0f, 3.0f, 0xC6ECFFFF);
    //    PortalDraw::Box(point, 3.0f, 11.0f, 0xC6ECFFFF);

    //    DrawTextureAsObj(boltTexture, point, { 32, 32 }, 0.0f, false);

    //}
    // 【連打演出】本体の白BOXを再利用。色を重ねるだけで追加描画は最大1BOX。
    // 256x256の元サイズは変えず、連打で一瞬赤く点滅する。
    if (state_ == BossState::ArmorStun && mashCount_ > 0 && stateFrame_ % 6 < 2) {
        PortalDraw::Box(position_, 256.0f, 256.0f, 0xFF45664B);
    }
    // 【外装シート】256x256の本体1倍はそのまま。上に薄い外装BOXを1枚重ねる。
    // ネジ4つを外すと、板が下端を支点に回転してペロッとめくれる。
    if (state_ == BossState::ArmorCurl) {
        const float t = state_ == BossState::ArmorCurl ?
            Clamp(stateFrame_ / static_cast<float>(BossStage1::kArmorCurlFrames), 0.0f, 1.0f) : 0.0f;
        const float angle = t * t * 1.45f;
        const Vector2 shellCenter = { position_.x + std::sin(angle) * 70.0f,
            position_.y - 85.0f + std::cos(angle) * 84.0f };
        PortalDraw::RotatedBox(shellCenter, 226.0f, 22.0f, angle, 0xB7C4D1CC);
    }
    //// プロペラ3本が破壊対象。本体はグリップ可能でも剥がれない。
    //// 各板の上に小さな黄色の十字を描き、そこへ触手を当てて引き剥がす。
    //// 表示1枚あたりBOX3個。板が減れば描画も減る。
    //for (int i = 0;i < BossStage1::kArmorPlates;++i) {
    //    if (IsArmorAttached(i)) {
    //        const Vector2 p = GetArmorGripPoint(i);
    //        // 本体のポイントは移動用。プロペラだけが外れる。
    //        const float tension = i == armorStrainPlate_ ? armorStrain_ : 0.0f;
    //        PortalDraw::RotatedBox({ p.x,p.y - 6.0f + tension * 4.0f },
    //            34.0f + tension * 12.0f, 16.0f - tension * 4.0f,
    //            tension * (i == 0 ? -0.24f : 0.24f), tension > 0.55f ? 0xFFF0B0FF : 0xA7B1C1FF);
    //        PortalDraw::Box(p, 13.0f, 3.0f, 0xFFE16AFF);
    //        PortalDraw::Box(p, 3.0f, 13.0f, 0xFFE16AFF);

    //        DrawTextureAsObj(boltTexture, p, { 32, 32 }, 0.0f, false);
    //    }

    //}
    if (state_ == BossState::Dizzy || state_ == BossState::ArmorStun || state_ == BossState::ArmorImpact) {
        // 回避成功の合図：本体の上に黄色BOXを1枚。新規パーティクル不要。
        PortalDraw::Box({ position_.x,position_.y + 53.0f }, 49.0f, 7.0f, 0xFFEA73FF);
    }
    if (IsCoreExposed() && !IsDefeated() && state_ != BossState::ArmorCurl) {
        const float pulse = 1.0f + 0.17f * std::sin(stateFrame_ * 0.21f);
        PortalDraw::Box(GetCoreHitPoint(), 20.0f * pulse, 22.0f * pulse,
            IsWeakPointOpen() ? 0xFF3030FF : 0xC73A48FF);
    }
    // コマ256px・ヨーヨー128pxの画像を使い、攻撃の差が一目で分かるようにする。
    if (state_ == BossState::WakeBlast && stateFrame_ < 12) {
        // 起き上がり：描画BOX1枚で衝撃を表す。Player側へ同じ瞬間に速度を与える。
        PortalDraw::Box(position_, 300.0f + stateFrame_ * 17.0f, 8.0f, 0xFFE9BCCC);
    }
    if (state_ == BossState::Toy) {
        static int yoyoHandle = -2;
        if (nextAttack_ == BossAttack::Top) {
            DrawSpinningTop(toyPosition_, stateFrame_);
        }
        else {
            PortalDraw::Line(position_, toyPosition_, 0xF0D3FFFF);
            DrawToySprite("./resources/texture/boss/form1/yo-yo.png",
                toyPosition_, 128, stateFrame_ * 0.17f, yoyoHandle);
        }
    }
    for (const auto& b : bullets_) if (b.active) visual_.DrawBullet(b.position, b.velocity);
    if (!coasterStage_) visual_.DrawFront();
    if (IsDefeated()) PortalDraw::Box(position_, 72.0f, 13.0f, 0x777777FF);
}
