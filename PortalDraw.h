#pragma once
#include "PortalMath.h"
#include "Camera.h"
#include "Tool.h"
#include <Novice.h>
#include <cmath>

// ポータルのワールド座標を既存のCameraに合わせて描画する。
// オリジナル版の「720-y」固定スクリーン変換から変更。
namespace PortalDraw {
    inline Vector2i ToScreen(Vector2 p) {
        return ConvertVector2(Transform(p, GetWorldToScreenMatrix()));
    }
    inline int Scale(float value) {
        int px = static_cast<int>(std::round(value * GetCameraZoom()));
        return px > 0 ? px : 1;
    }
    inline void Circle(Vector2 p, float r, unsigned int c, bool fill = true) {
        Vector2i s = ToScreen(p);
        Novice::DrawEllipse(s.x, s.y, Scale(r), Scale(r), 0.0f, c,
            fill ? kFillModeSolid : kFillModeWireFrame);
    }
    inline void Line(Vector2 a, Vector2 b, unsigned int c) {
        Vector2i p = ToScreen(a);
        Vector2i q = ToScreen(b);
        Novice::DrawLine(p.x, p.y, q.x, q.y, c);
    }
    inline void Box(Vector2 center, float w, float h, unsigned int c) {
        Vector2i s = ToScreen(center);
        int width = Scale(w);
        int height = Scale(h);
        Novice::DrawBox(s.x - width / 2, s.y - height / 2,
            width, height, 0.0f, c, kFillModeSolid);
    }
    // 【空中回転攻撃の描画】BOXのみ回転させる。物理判定は回転させない。
    // Noviceの描画角はスクリーン基準なので、+Y上向きの角度を反転する。
    inline void RotatedBox(Vector2 center, float w, float h, float worldAngle, unsigned int c) {
        Vector2i s = ToScreen(center);
        const int width = Scale(w);
        const int height = Scale(h);
        Novice::DrawBox(s.x - width / 2, s.y - height / 2, width, height,
            -worldAngle, c, kFillModeSolid);
    }
}
