#pragma once
#include "AssetEditor.h"
#include "BossStage1Tuning.h"
#include "PortalMath.h"
namespace BossHelpers {
    inline constexpr float kTopRadius = 128.0f;
    // PortalArena::DrawWorld で描く床の左右端。
    inline constexpr float kTopFloorLeft = 5.0f;
    inline constexpr float kTopFloorRight = 1275.0f;
    inline constexpr float kTopVisibleHalfWidth = 78.0f; // gyro.png の不透明部分は X=50〜205

    inline float TopWidthFactor(int frame) {
        return 0.91f + 0.09f * std::cos(frame * 0.28f);
    }

    // 第2ステージの機体画像が512pxなど横長の場合も、完全に画面外まで飛び抜ける。
    // 判定用のボス幅は変更せず、描画上の幅だけで画面外位置を決める。
    inline float Stage2VisualHalfWidth() {
        const auto& image = AssetEditor::assets[AssetEditor::BossForm2];
        return image.handle >= 0 && image.width > 0
            ? image.width * AssetEditor::Scale(AssetEditor::BossForm2) * 0.5f
            : BossStage1::kBossWidth * 0.5f;
    }
    inline float Stage2VisualHalfHeight() {
        const auto& image = AssetEditor::assets[AssetEditor::BossForm2];
        return image.handle >= 0 && image.height > 0
            ? image.height * AssetEditor::Scale(AssetEditor::BossForm2) * 0.5f
            : BossStage1::kBossHeight * 0.5f;
    }
}
