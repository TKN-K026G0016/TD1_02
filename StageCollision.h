#pragma once
// ステージ／ボスパーツ共通の矩形物理当たり判定。
// ゲーム世界座標（Y上向き）。画像とは独立して有効・無効を設定できる。
#include "Vector2.h"
#include <cmath>
#include <algorithm>

namespace StageCollision {
    enum class Type : int { None = 0, Solid = 1, OneWay = 2, Damage = 3 };
    struct Rect {
        float left = 0.0f, bottom = 0.0f, right = 0.0f, top = 0.0f;
        Type type = Type::None;
        int damage = 0;
    };
    constexpr int kMaxRects = 256;
    inline const char* TypeName(Type t) {
        switch (t) {
        case Type::Solid: return "床・壁（通り抜け不可）";
        case Type::OneWay: return "すり抜け床（上からのみ着地）";
        case Type::Damage: return "接触ダメージ";
        default: return "判定なし";
        }
    }
    inline bool Valid(const Rect& r) {
        return std::isfinite(r.left) && std::isfinite(r.bottom) &&
            std::isfinite(r.right) && std::isfinite(r.top) &&
            r.right > r.left && r.top > r.bottom;
    }
    inline bool Intersect(float a0, float a1, float b0, float b1) {
        return a1 > b0 + 0.001f && a0 < b1 - 0.001f;
    }
    // キャラクターの中心点と現在位置を入力。横→縦の順に分離し、ワイヤーの速度は衝突した軸だけ0にする。
    inline void Resolve(Vector2 oldPos, Vector2& pos, Vector2& velocity,
        bool& grounded, const Rect* rects, int count, float halfWidth = 18.f, float halfHeight = 30.f) {
        if (!rects || count <= 0) return;
        // 水平方向：前フレームで左側・右側にいたときのみ壁として停止。
        for (int i = 0;i < count;++i) {
            const Rect& r = rects[i];
            if (r.type != Type::Solid || !Valid(r)) continue;
            if (!Intersect(oldPos.y - halfHeight, oldPos.y + halfHeight, r.bottom, r.top) &&
                !Intersect(pos.y - halfHeight, pos.y + halfHeight, r.bottom, r.top)) continue;
            if (oldPos.x + halfWidth <= r.left + 0.01f && pos.x + halfWidth > r.left) {
                pos.x = (std::min)(pos.x, r.left - halfWidth);
                if (velocity.x > 0) velocity.x = 0;
            }
            else if (oldPos.x - halfWidth >= r.right - 0.01f && pos.x - halfWidth < r.right) {
                pos.x = (std::max)(pos.x, r.right + halfWidth);
                if (velocity.x < 0) velocity.x = 0;
            }
        }
        // 垂直方向：上から降りた場合は足元で着地。下からの衝突はSolidだけ。
        for (int i = 0;i < count;++i) {
            const Rect& r = rects[i];
            if ((r.type != Type::Solid && r.type != Type::OneWay) || !Valid(r)) continue;
            if (!Intersect(pos.x - halfWidth, pos.x + halfWidth, r.left, r.right)) continue;
            if (velocity.y <= 0 && oldPos.y - halfHeight >= r.top - 0.01f && pos.y - halfHeight <= r.top) {
                pos.y = (std::max)(pos.y, r.top + halfHeight);
                velocity.y = 0; grounded = true;
            }
            else if (r.type == Type::Solid && velocity.y > 0 &&
                oldPos.y + halfHeight <= r.bottom + 0.01f && pos.y + halfHeight >= r.bottom) {
                pos.y = (std::min)(pos.y, r.bottom - halfHeight); velocity.y = 0;
            }
        }
        // すでに床に接している場合は、重力でわずかに沈む次フレームも着地状態を保持。
        if (!grounded && velocity.y <= 0) for (int i = 0;i < count;++i) {
            const Rect& r = rects[i];
            if ((r.type != Type::Solid && r.type != Type::OneWay) || !Valid(r)) continue;
            if (std::fabs((pos.y - halfHeight) - r.top) <= 0.15f &&
                Intersect(pos.x - halfWidth, pos.x + halfWidth, r.left, r.right)) {
                grounded = true; velocity.y = 0; break;
            }
        }
    }
} // namespace StageCollision
