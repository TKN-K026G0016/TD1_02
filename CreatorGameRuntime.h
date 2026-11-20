#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include "PortalDraw.h"
#include "CreatorRuntimeData.h" // エディターで「ゲーム用出力」した後に再ビルドすると反映

// エディターで作った配置をボス戦に重ねるための軽量ランタイム。
// +Y上向きの戦闘座標と、エディターの画面Y下向きをここで変換する。
namespace CreatorGameRuntime {
    struct Tile { int x = 0, y = 0, tile = 0, collision = 0, layer = 0; };
    struct HitShape { int cellX = 0, cellY = 0, type = 0, effect = 0; std::vector<Vector2> points; };
    class Runtime {
    public:
        bool showStage = false, showCollision = false, showCreatorUI = false, showEffects = false, showAnimation = false, showGimmicks = false;
        std::vector<Tile> tiles;
        std::vector<HitShape> hitboxes;
        int frame = 0;
        bool Reload() {
            std::vector<Tile> loadedTiles;
            std::vector<HitShape> loadedHits;
            bool a = ReadStage("StageData.stage", loadedTiles);
            bool b = ReadHitboxes("StageData.hitboxes", loadedHits);
            if (a) tiles.swap(loadedTiles);
            if (b) hitboxes.swap(loadedHits);
            return a || b;
        }
        void Update() { ++frame; }
        void DrawWorld() const {
            if (showStage) for (const auto& t : tiles) {
                if (t.tile == 0) continue;
                static const unsigned colors[] = { 0x5ABF6FFF,0x986742FF,0x8F9AA7FF,0x46A5C7FF,0xE3AD49FF,0xB86D9FFF,0xDDD5B5FF };
                PortalDraw::Box({ t.x * 32.0f + 16.0f,720.0f - (t.y * 32.0f + 16.0f) }, 32, 32, colors[(t.tile - 1) % 7]);
            }
            if (showCollision) for (const auto& shape : hitboxes) {
                const unsigned c = shape.effect == 3 ? 0xF04B50FF : shape.effect == 4 ? 0x3FA8EFFF : 0xFFD36AFF;
                if (shape.points.size() < 2)continue;
                if (shape.type == 1) {
                    Vector2 a = ToWorld(shape, shape.points[0]);
                    const float dx = shape.points[1].x - shape.points[0].x;
                    const float dy = shape.points[1].y - shape.points[0].y;
                    PortalDraw::Circle(a, std::sqrt(dx * dx + dy * dy), c, false);
                }
                else {
                    const int count = (int)shape.points.size();
                    if (shape.type == 0) {
                        Vector2 a = shape.points[0], b = shape.points[1];
                        const Vector2 q[4] = { {a.x,a.y},{b.x,a.y},{b.x,b.y},{a.x,b.y} };
                        for (int i = 0;i < 4;i++)PortalDraw::Line(ToWorld(shape, q[i]), ToWorld(shape, q[(i + 1) % 4]), c);
                    }
                    else for (int i = 0;i < count;i++)
                        PortalDraw::Line(ToWorld(shape, shape.points[i]), ToWorld(shape, shape.points[(i + 1) % count]), c);
                }
            }
            if (showGimmicks) for (int i = 0;i < CreatorRuntime::kGimmickCount;i++) {
                const auto& g = CreatorRuntime::kGimmicks[i];
                if (g.enabled) PortalDraw::Box({ g.x,720.0f - g.y }, 24.0f, 24.0f, 0xFFCB7D99);
            }
            if (showEffects) {
                const auto& fx = CreatorRuntime::kEffect;
                // パーティクルは負荷を抑えるため最大32個のプレビュー
                const int count = (std::min)(fx.count, 32);
                for (int i = 0;i < count;++i) {
                    const float t = static_cast<float>((frame + i * 7) % (std::max)(fx.life, 1));
                    const float a = i * 2.3999632f;
                    const float x = fx.x + static_cast<float>(std::cos(a)) * fx.speed * t * 0.30f;
                    const float y = 720.0f - fx.y + static_cast<float>(std::sin(a)) * fx.speed * t * 0.30f - fx.gravity * t * t * 0.06f;
                    PortalDraw::Circle({ x,y }, (std::max)(1.0f, fx.size * (1.0f - t / (std::max)(fx.life, 1))), 0xFFAA69AA);
                }
            }
            if (showAnimation && CreatorRuntime::kAnimationKeyCount > 0) {
                const int n = CreatorRuntime::kAnimationKeyCount;
                const auto& k = CreatorRuntime::kAnimationKeys[(frame / 16) % n];
                PortalDraw::Box({ k.x,720.0f - k.y }, 24.0f * k.scale, 24.0f * k.scale, 0x96DBE7CC);
            }
        }
        void DrawHUD() const {
            if (!showCreatorUI)return;
            // 生成ヘッダーは0件の場合もダミー要素を1つ持つため、実際の件数だけ描画する。
            int widgetIndex = 0;
            for (const auto& w : CreatorRuntime::kUIWidgets) {
                if (widgetIndex >= CreatorRuntime::kUIWidgetCount) break;
                ++widgetIndex;
                if (!w.visible)continue;
                const int x = (int)w.x, y = (int)w.y;
                const unsigned int color = 0x4CADD8FF;
                Novice::DrawBox(x, y, w.width, w.height, 0, color, kFillModeWireFrame);
                if (w.type == 2)Novice::ScreenPrintf(x + 4, y + 4, "%s", w.text);
                if (w.type == 0 && w.maxValue > 0) Novice::DrawBox(x, y, (int)((float)w.width * std::clamp(w.value, 0, w.maxValue) / w.maxValue), w.height, 0, color, kFillModeSolid);
            }
        }
        // この判定はエディターの「危険」エリアの当たり確認に使う。
        // 固体壁の本格的な押し戻しは既存PortalPlayerの専用物理とは別扱い。
        bool PlayerInHazard(Vector2 center, float r = 20.0f) const {
            for (const auto& h : hitboxes) if (h.effect == 3) {
                if (h.type == 1 && h.points.size() == 2) {
                    Vector2 a = ToWorld(h, h.points[0]);
                    float dx = h.points[0].x - h.points[1].x, dy = h.points[0].y - h.points[1].y;
                    float radius = std::sqrt(dx * dx + dy * dy) + r;
                    float x = center.x - a.x, y = center.y - a.y;
                    if (x * x + y * y <= radius * radius)return true;
                }
                else if (h.points.size() >= 2) {
                    float loX = 1e9f, hiX = -1e9f, loY = 1e9f, hiY = -1e9f;
                    for (const auto& q : h.points) { Vector2 p = ToWorld(h, q);loX = (std::min)(loX, p.x);hiX = (std::max)(hiX, p.x);loY = (std::min)(loY, p.y);hiY = (std::max)(hiY, p.y); }
                    if (center.x + r >= loX && center.x - r <= hiX && center.y + r >= loY && center.y - r <= hiY)return true;
                }
            }
            return false;
        }
    private:
        static Vector2 ToWorld(const HitShape& h, Vector2 p) { return { h.cellX * 32.0f + p.x,720.0f - (h.cellY * 32.0f + p.y) }; }
        static bool ReadStage(const char* path, std::vector<Tile>& out) {
            std::ifstream f(path); if (!f)return false;
            std::string magic;int w = 0, h = 0, layers = 0, objects = 0;
            if (!(f >> magic >> w >> h >> layers >> objects) || magic != "NOVICE_STAGE_V2" || w < 1 || w>256 || h < 1 || h>256 || layers < 1 || layers>12 || objects < 0 || objects>20000)return false;
            for (int l = 0;l < layers;l++) {
                int len = 0, visible = 0;if (!(f >> len) || len < 1 || len>80)return false;
                f.get();std::string name((size_t)len, ' ');if (!f.read(&name[0], len) || !(f >> visible))return false;
                for (int y = 0;y < h;y++)for (int x = 0;x < w;x++) {
                    int tile = 0, coll = 0;if (!(f >> tile >> coll) || tile < 0 || tile>1024 || coll < 0 || coll>4)return false;
                    if (visible && (tile || coll))out.push_back({ x,y,tile,coll,l });
                }
            }
            return true;
        }
        static bool ReadHitboxes(const char* path, std::vector<HitShape>& out) {
            std::ifstream f(path);if (!f)return false;
            std::string magic;int count = 0;
            if (!(f >> magic >> count) || magic != "HITBOXES_V1" || count < 0 || count>10000)return false;
            for (int i = 0;i < count;i++) {
                HitShape s;int n = 0;
                if (!(f >> s.cellX >> s.cellY >> s.type >> s.effect >> n) || s.type < 0 || s.type>3 || s.effect < 1 || s.effect>4 || n < 2 || n>16)return false;
                for (int j = 0;j < n;j++) { Vector2 p;if (!(f >> p.x >> p.y) || !std::isfinite(p.x) || !std::isfinite(p.y))return false;s.points.push_back(p); }
                out.push_back(s);
            }return true;
        }
    };
}
