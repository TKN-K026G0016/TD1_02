#pragma once
// マリオメーカー風・簡易マップチップエディター
// ワールド座標はY上向き。編集キャンバスのみY下向き。
// 1マス32px × 40列 × 23行 / ステージ1～4を独立保存。
#include "AssetEditor.h"
#include "PortalDraw.h"
#include "StageCollision.h"
#include <Novice.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace TileMapEditor {
    constexpr int kTileSize = 32;
    constexpr int kCols = 40;
    constexpr int kRows = 23;
    constexpr int kStageCount = 4;
    constexpr int kCells = kCols * kRows;
    // タイル番号：0は空、1以上は左上から並んだマップチップ画像内のセル番号＋1。
    struct Cell { std::uint16_t tile = 0; std::uint8_t collision = 0; }; // 判定：0無 1床・壁 2すり抜け
    using Map = std::array<Cell, kCells>;
    inline std::array<Map, kStageCount> maps{};
    inline int tileSelected = 1;
    inline int collisionSelected = 1;
    inline bool enabled = true;
    inline bool dirty = false;
    inline const char* status = "マップチップを編集できます";
    inline int lastX = -1, lastY = -1;
    inline bool strokeActive = false;
    inline Map undoMap{}, redoMap{};
    inline int undoStage = -1, redoStage = -1;

    inline int StageIndex(int stage) { return (std::max)(0, (std::min)(kStageCount - 1, stage - 1)); }
    inline bool InBounds(int x, int y) { return x >= 0 && x < kCols && y >= 0 && y < kRows; }
    inline Cell& At(int stage, int x, int y) { return maps[StageIndex(stage)][y * kCols + x]; }
    inline const Cell& Read(int stage, int x, int y) { return maps[StageIndex(stage)][y * kCols + x]; }
    inline void BeginStroke(int stage) { undoMap = maps[StageIndex(stage)];undoStage = stage;redoStage = -1;strokeActive = true;lastX = -1;lastY = -1; }
    inline void EndStroke() { strokeActive = false;lastX = -1;lastY = -1; }
    inline void Undo(int stage) { if (undoStage != stage)return;redoMap = maps[StageIndex(stage)];redoStage = stage;maps[StageIndex(stage)] = undoMap;undoStage = -1;dirty = true; }
    inline void Redo(int stage) { if (redoStage != stage)return;undoMap = maps[StageIndex(stage)];undoStage = stage;maps[StageIndex(stage)] = redoMap;redoStage = -1;dirty = true; }
    inline void Paint(int stage, int x, int y, bool erase) {
        if (!InBounds(x, y) || (lastX == x && lastY == y))return;
        lastX = x;lastY = y;
        Cell next{};
        if (!erase) { next.tile = static_cast<std::uint16_t>((std::max)(1, tileSelected));next.collision = static_cast<std::uint8_t>(collisionSelected); }
        Cell& c = At(stage, x, y);
        if (c.tile == next.tile && c.collision == next.collision)return;
        c = next;dirty = true;
    }
    inline bool Save() {
        AssetEditor::FixSettingsLocation();
        const std::string path = AssetEditor::resourceRoot + "\\tilemap_layout.txt";
        std::ofstream out(path, std::ios::trunc);
        if (!out) { status = "マップ保存失敗";return false; }
        out << "TileMapV1 " << kCols << ' ' << kRows << ' ' << kTileSize << '\n';
        for (int s = 0;s < kStageCount;++s) {
            out << "STAGE " << s + 1 << '\n';
            for (int y = 0;y < kRows;++y) {
                for (int x = 0;x < kCols;++x) {
                    const Cell& c = maps[s][y * kCols + x];
                    out << c.tile << ':' << int(c.collision) << (x + 1 == kCols ? '\n' : ' ');
                }
            }
        }
        out.flush();const bool ok = out.good();
        if (ok) { dirty = false;status = "マップチップを保存しました"; }
        else status = "マップ保存失敗";
        return ok;
    }
    inline bool Load() {
        AssetEditor::FixSettingsLocation();
        std::ifstream in(AssetEditor::resourceRoot + "\\tilemap_layout.txt");
        if (!in) { status = "マップファイルなし（新規）";return false; }
        std::string signature, mark;int cols = 0, rows = 0, size = 0, number = 0;
        if (!(in >> signature >> cols >> rows >> size) || signature != "TileMapV1" || cols != kCols || rows != kRows || size != kTileSize) { status = "マップ形式エラー";return false; }
        std::array<Map, kStageCount> parsed{};
        for (int s = 0;s < kStageCount;++s) {
            if (!(in >> mark >> number) || mark != "STAGE" || number != s + 1) { status = "マップ読込エラー";return false; }
            for (Cell& cell : parsed[s]) {
                unsigned int tile = 0;int col = 0;char colon = 0;
                if (!(in >> tile >> colon >> col) || colon != ':' || tile > 65535 || col < 0 || col>2) { status = "マップのセルが不正です";return false; }
                cell.tile = static_cast<std::uint16_t>(tile);
                cell.collision = static_cast<std::uint8_t>(col);
            }
        }
        maps = std::move(parsed);undoStage = -1;redoStage = -1;dirty = false;status = "マップチップを読み込みました";
        return true;
    }
    inline int SheetCols() { const auto& a = AssetEditor::assets[AssetEditor::MapChip];return (std::max)(1, a.width / kTileSize); }
    inline int SheetRows() { const auto& a = AssetEditor::assets[AssetEditor::MapChip];return (std::max)(1, a.height / kTileSize); }
    inline int SheetTileCount() { return (std::max)(1, SheetCols() * SheetRows()); }
    inline void DrawWorld(int stage) {
        if (!enabled)return;
        const auto& a = AssetEditor::assets[AssetEditor::MapChip];
        const int cols = SheetCols();const int rows = SheetRows();
        for (int y = 0;y < kRows;++y)for (int x = 0;x < kCols;++x) {
            const Cell& cell = Read(stage, x, y);
            if (cell.tile == 0)continue;
            const int index = int(cell.tile) - 1;
            const Vector2i tl = PortalDraw::ToScreen({ float(x * kTileSize),float(720 - y * kTileSize) });
            const Vector2i br = PortalDraw::ToScreen({ float((x + 1) * kTileSize),float(720 - (y + 1) * kTileSize) });
            if (a.handle >= 0 && index < cols * rows) {
                const int srcX = (index % cols) * kTileSize, srcY = (index / cols) * kTileSize;
                Novice::DrawQuad(tl.x, tl.y, br.x, tl.y, tl.x, br.y, br.x, br.y, srcX, srcY, kTileSize, kTileSize, a.handle, WHITE);
            }
            else {
                Novice::DrawBox(tl.x, tl.y, br.x - tl.x, br.y - tl.y, 0, cell.collision == 1 ? 0x70936AFF : 0x8E9EADFF, kFillModeSolid);
            }
        }
    }
    // 連続する同種のタイルを水平方向にまとめて衝突矩形にする。
    // Y上向きゲーム座標に変換して既存のプレイヤー衝突解決へ渡す。
    inline int CollectColliders(int stage, StageCollision::Rect* output, int capacity) {
        if (!enabled || !output || capacity <= 0)return 0;
        int count = 0;
        for (int y = 0;y < kRows;++y) {
            for (int x = 0;x < kCols;) {
                const Cell& c = Read(stage, x, y);
                if (c.collision == 0) { ++x;continue; }
                const int start = x;const int t = int(c.collision);
                do { ++x; } while (x < kCols && Read(stage, x, y).collision == t);
                if (count >= capacity)return count;
                StageCollision::Rect r{};
                r.left = float(start * kTileSize);r.right = float(x * kTileSize);
                r.top = float(720 - y * kTileSize);r.bottom = r.top - float(kTileSize);
                r.type = t == 2 ? StageCollision::Type::OneWay : StageCollision::Type::Solid;
                output[count++] = r;
            }
        }
        return count;
    }
#ifdef USE_IMGUI
    inline void DrawPalette(int stage) {
        ImGui::TextUnformatted("マップチップ / TILESET");
        if (ImGui::Button("PNGを読み込む", ImVec2(-1, 0))) {
            if (AssetEditor::ChooseFile(AssetEditor::MapChip)) { tileSelected = 1; status = "画像を読み込みました"; }
        }
        const auto& a = AssetEditor::assets[AssetEditor::MapChip];
        ImGui::Text("%d x %d px", a.width, a.height);
        ImGui::Text("1マス %d px", kTileSize);
        ImGui::Separator();
        const char* colliders[] = { "なし","床・壁","すり抜け床" };
        ImGui::Combo("タイル判定", &collisionSelected, colliders, 3);
        const int maxCount = (std::min)(256, SheetTileCount());
        ImGui::TextUnformatted("チップ番号を選択");
        ImGui::BeginChild("##TilePaletteGrid", ImVec2(0, 165), true);
        for (int i = 0;i < maxCount;++i) {
            ImGui::PushID(i);
            char label[16]{};std::snprintf(label, sizeof(label), "%d", i + 1);
            if (ImGui::Selectable(label, tileSelected == i + 1, 0, ImVec2(42, 24)))tileSelected = i + 1;
            ImGui::PopID();
            if ((i + 1) % 3 != 0)ImGui::SameLine();
        }
        ImGui::EndChild();
        ImGui::Text("選択: %d", tileSelected);
        ImGui::TextUnformatted("※画像の左上から横方向に採番");
        if (ImGui::Button("タイルUndo"))Undo(stage);
        ImGui::SameLine();if (ImGui::Button("Redo"))Redo(stage);
        if (ImGui::Button("タイル保存"))Save();
        ImGui::SameLine();if (ImGui::Button("読込"))Load();
        ImGui::TextWrapped("%s", status);
    }
    template <class ToScreen>
    inline void DrawEditorOverlay(int stage, ToScreen toScreen, ImDrawList* draw) {
        for (int y = 0;y < kRows;++y)for (int x = 0;x < kCols;++x) {
            const Cell& c = Read(stage, x, y);
            if (c.tile == 0 && c.collision == 0)continue;
            const ImVec2 a = toScreen(float(x * kTileSize), float(y * kTileSize));
            const ImVec2 b = toScreen(float((x + 1) * kTileSize), float((y + 1) * kTileSize));
            const ImU32 color = c.collision == 1 ? IM_COL32(90, 166, 108, 210) : c.collision == 2 ? IM_COL32(213, 181, 95, 210) : IM_COL32(125, 153, 186, 210);
            draw->AddRectFilled(a, b, color);
            draw->AddRect(a, b, IM_COL32(224, 232, 240, 130));
            if (b.x - a.x >= 21) {
                char label[16]{};std::snprintf(label, sizeof(label), "%d", int(c.tile));
                draw->AddText(ImVec2(a.x + 2, a.y + 2), IM_COL32(18, 25, 32, 255), label);
            }
        }
    }
#endif
}
