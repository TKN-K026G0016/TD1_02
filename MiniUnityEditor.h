#pragma once
// 第3段階：GP・接触判定・簡易敵・Prefabをシーン編集へ追加。
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
// ============================================================================
// Mini Unity (1st step) : 2D scene object editor for Novice / ImGui.
// - Hierarchy / scene view / inspector / save and reload.
// - Only newly placed visual objects are changed. NO edits to player physics/AI.
// - Screen coordinates: (0, 0) = upper-left; separate from +Y-up physics.
// ============================================================================
#include "AssetEditor.h"
#include "Vector2.h"
#include "PortalDraw.h"
#include "StageCollision.h"
#include "TileMapEditor.h"
#include <Novice.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace MiniUnityEditor {
    constexpr int kMaxObjects = 128;
    constexpr int kCanvasW = 1280;
    constexpr int kCanvasH = 720;

    // 装飾以外のオブジェクトも同じTransformで編集する。
    // Collider はプレイヤーとの接触ダメージを扱うトリガー。新規PNG/装飾にも設定可能。
    // 床や壁の押し戻しとは別（プレイヤー物理を変更しない）。
    enum class Kind : int { Decoration = 0, GripPoint = 1, Enemy = 2, DamageZone = 3 };
    inline const char* KindName(Kind k) {
        switch (k) {
        case Kind::Decoration: return "装飾";
        case Kind::GripPoint: return "GP（触手の接続先）";
        case Kind::Enemy: return "巡回する敵";
        case Kind::DamageZone: return "ダメージ領域";
        }
        return "不明";
    }
    struct Object {
        int id = 0;
        Kind kind = Kind::Decoration;
        bool collider = false;
        int collisionType = 0; // 0=なし 1=床壁 2=すり抜け床 3=ダメージ
        float patrolRange = 100.0f; // 敵の左右移動幅（px）
        float patrolSpeed = 0.025f; // 敵の揺れの速さ（rad/frame）
        int touchDamage = 5; // 接触時のダメージ。装飾は初期状態で collider=false
        char name[64] = "新しいオブジェクト";
        float x = 480.0f;
        float y = 270.0f;
        float width = 80.0f;
        float height = 80.0f;
        float angle = 0.0f;  // degree: visual rotation only
        int stage = 1;        // 1: stage 1, 2: stage 2, 3: both
        int layer = 0;        // 0: behind boss, 1: before player, 2: in front of player
        int resourceSlot = -1; // -1: placeholder colored rectangle
        // 名前付き追加PNG（AssetEditorの固定スロットとは別に管理）
        char textureName[64] = {};
        char texturePath[512] = {};
        // 0=カメラ移動・ズーム連動 1=位置だけ連動（通常サイズ） 2=画面固定
        int cameraMode = 0;
        bool visible = true;
        bool locked = false;
        unsigned char r = 95, g = 182, b = 220, a = 190;
    };

    struct LoadedTexture { std::string path; int handle = -1; int width = 0; int height = 0; };
    inline std::vector<LoadedTexture> loadedTextures;
    inline LoadedTexture* FindTexture(const char* path) {
        if (!path || !path[0]) return nullptr;
        for (auto& t : loadedTextures) if (t.path == path) return &t;
        LoadedTexture t{};
        t.path = path;
        // シーンファイルにはresources以下の相対パスを保存する（プロジェクト移動に対応）。
        AssetEditor::FixSettingsLocation();
        const bool isAbsolute = (t.path.size() > 1 && t.path[1] == ':') ||
            (!t.path.empty() && (t.path[0] == '/' || t.path[0] == '\\'));
        const std::string full = isAbsolute ? t.path : AssetEditor::resourceRoot + "\\" + t.path;
        t.handle = Novice::LoadTexture(full.c_str());
        AssetEditor::ReadPNGSize(full.c_str(), t.width, t.height);
        loadedTextures.push_back(t);
        return &loadedTextures.back();
    }
    inline bool ChooseObjectTexture(Object& o) {
        AssetEditor::FixSettingsLocation();
        char selected[MAX_PATH]{};
        OPENFILENAMEA dlg{};
        dlg.lStructSize = sizeof(dlg);
        dlg.lpstrFilter = "PNG image (*.png)\0*.png\0\0";
        dlg.lpstrFile = selected;
        dlg.nMaxFile = MAX_PATH;
        dlg.lpstrTitle = "オブジェクトのPNGを選択";
        dlg.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameA(&dlg)) return false;
        int w = 0, h = 0;
        if (!AssetEditor::ReadPNGSize(selected, w, h)) {
            return false;
        }
        const std::string directory = AssetEditor::resourceRoot + "\\texture\\editor_objects";
        CreateDirectoryA(AssetEditor::resourceRoot.c_str(), nullptr);
        CreateDirectoryA((AssetEditor::resourceRoot + "\\texture").c_str(), nullptr);
        CreateDirectoryA(directory.c_str(), nullptr);
        // ID別のコピーにして、同名の素材が衝突しないようにする。
        static int sequence = 1;
        std::string target;
        do {
            target = directory + "\\object_" + std::to_string(o.id) + "_" + std::to_string(sequence++) + ".png";
        } while (GetFileAttributesA(target.c_str()) != INVALID_FILE_ATTRIBUTES);
        if (!CopyFileA(selected, target.c_str(), TRUE)) return false;
        const size_t split = target.find_last_of("/\\");
        const std::string fileName = split == std::string::npos ? target : target.substr(split + 1);
        const std::string relative = std::string("texture\\editor_objects\\") + fileName;
        std::snprintf(o.texturePath, sizeof(o.texturePath), "%s", relative.c_str());
        if (!o.textureName[0]) std::snprintf(o.textureName, sizeof(o.textureName), "%s", o.name);
        LoadedTexture* t = FindTexture(o.texturePath);
        if (!t || t->handle < 0) return false;
        o.resourceSlot = -1;
        o.width = float(w);
        o.height = float(h);
        return true;
    }
    inline std::vector<Object> objects;
    // 保存できるユーザーPrefab（作成時のTransform / 画像 / 当たり判定を記録）。
    struct Prefab { char name[64] = {}; Object sample{}; };
    inline std::vector<Prefab> prefabs;
    constexpr int kMaxPrefabs = 16;
    constexpr int kMaxEditorGripPoints = 4; // 既存24本のサンプル杭配列を超えない
    inline int runtimeFrame = 0; // プレイヤー物理には影響しない描画・接触判定用タイマー
    inline bool showColliders = true;
    inline void ResetRuntime() { runtimeFrame = 0; }
    inline void TickRuntime() { ++runtimeFrame; }

    inline int nextId = 1;
    inline int selectedId = -1;
    inline bool snapToGrid = true;
    inline int gridPixels = 16;
    inline bool showGrid = true;
    inline bool showBounds = false;
    inline bool drawingEnabled = true;
    inline bool dirty = false;
    inline std::string lastStatus = "シーンの準備ができました";
    // Undo/redo for object creation, removal, duplication and scene dragging.
    struct Snapshot {
        std::vector<Object> data;
        int selected = -1;
        int next = 1;
    };
    inline std::vector<Snapshot> undoStack, redoStack;
    inline void Remember() {
        if (undoStack.size() >= 32) undoStack.erase(undoStack.begin());
        undoStack.push_back({ objects, selectedId, nextId });
        redoStack.clear();
    }
    inline void Undo() {
        if (undoStack.empty()) return;
        redoStack.push_back({ objects, selectedId, nextId });
        Snapshot s = std::move(undoStack.back());
        undoStack.pop_back();
        objects = std::move(s.data); selectedId = s.selected; nextId = s.next;
        dirty = true;
    }
    inline void Redo() {
        if (redoStack.empty()) return;
        undoStack.push_back({ objects, selectedId, nextId });
        Snapshot s = std::move(redoStack.back());
        redoStack.pop_back();
        objects = std::move(s.data); selectedId = s.selected; nextId = s.next;
        dirty = true;
    }
    // Editor-only scene camera: never changes gameplay Camera or physics.
    inline float sceneZoom = 1.0f;        // 0.25..4.0
    inline float sceneCenterX = kCanvasW * 0.5f;
    inline float sceneCenterY = kCanvasH * 0.5f;
    enum class GizmoMode { Move = 0, Rotate = 1, Scale = 2 };
    inline GizmoMode gizmoMode = GizmoMode::Move;
    enum class DragMode { None = 0, Move, Rotate, Scale };
    inline DragMode dragMode = DragMode::None;
    inline bool dragging = false;
    inline float dragOffsetX = 0.0f, dragOffsetY = 0.0f;
    inline float dragStartMouseX = 0.0f, dragStartMouseY = 0.0f;
    inline Object dragInitial{};
    inline void ResetSceneView() {
        sceneZoom = 1.0f;
        sceneCenterX = kCanvasW * 0.5f;
        sceneCenterY = kCanvasH * 0.5f;
        dirty = true;
    }

    inline std::string SceneFilePath() {
        AssetEditor::FixSettingsLocation();
        return AssetEditor::resourceRoot + "\\mini_unity_scene.txt";
    }

    inline bool InStage(const Object& o, int stage) {
        // 3は以前の「両方」を保持。4=ステージ3、5=ステージ4。
        return o.stage == 3 || o.stage == (stage <= 2 ? stage : stage + 1);
    }
    // スクリーン座標はY下向き。ゲーム/触手はY上向きなので、ここでだけ変換。
    inline Vector2 CenterWorld(const Object& o) {
        return { o.x + o.width * 0.5f, float(kCanvasH) - (o.y + o.height * 0.5f) };
    }
    inline float RuntimeX(const Object& o) {
        if (o.kind != Kind::Enemy) return o.x;
        // 配置座標は固定。動いている敵の表示位置・判定を同じ計算で共有する。
        return o.x + std::sin(float(runtimeFrame) * o.patrolSpeed + float(o.id) * 0.13f) * o.patrolRange;
    }
    inline bool IsActive(const Object& o, int stage) {
        return o.visible && InStage(o, stage);
    }
    inline StageCollision::Rect MakeRect(const Object& o) {
        StageCollision::Rect r;
        // 物理の矩形はワールド座標。回転は描画/ダメージ専用で、物理はAABB。
        const float x = RuntimeX(o);
        r.left = x; r.right = x + o.width;
        r.top = float(kCanvasH) - o.y;
        r.bottom = r.top - o.height;
        r.type = static_cast<StageCollision::Type>(o.collisionType);
        r.damage = o.touchDamage;
        return r;
    }
    inline int CollectSolidColliders(int stage, StageCollision::Rect* output, int capacity) {
        if (!output || capacity <= 0) return 0;
        int n = 0;
        for (const auto& o : objects) {
            if (!IsActive(o, stage) || o.kind == Kind::GripPoint ||
                (o.collisionType != 1 && o.collisionType != 2)) continue;
            if (n >= capacity) break;
            output[n++] = MakeRect(o);
        }
        return n;
    }

    // 編集中GPを最大4つプレイヤーへ渡す。既存杭・ボスネジの位置は動かさない。
    inline int CollectGripPoints(int stage, Vector2* target, int capacity) {
        if (!target || capacity <= 0) return 0;
        int count = 0;
        for (const auto& o : objects) {
            if (o.kind != Kind::GripPoint || !IsActive(o, stage)) continue;
            if (count >= capacity || count >= kMaxEditorGripPoints) break;
            target[count++] = CenterWorld(o);
        }
        return count;
    }
    // Circle(player) vs rotated rectangle(trigger). 新規のPNGや四角も判定対象。
    // 床としての押し戻しは行わず、既存の移動・ジャンプ物理を変更しない。
    inline bool CheckPlayerHit(int stage, Vector2 worldPosition, float radius,
        int* damage, Vector2* source) {
        const float px = worldPosition.x, py = float(kCanvasH) - worldPosition.y;
        for (const auto& o : objects) {
            // GP以外の全オブジェクトで接触判定を許可する。
            // 新規PNG、四角、Prefabから作った装飾も、ColliderがONなら対象。
            if (!IsActive(o, stage) || !(o.collisionType == 3 || (o.collisionType == 0 && o.collider)) || o.touchDamage <= 0 ||
                o.kind == Kind::GripPoint) continue;
            const float x = RuntimeX(o);
            const float cx = x + o.width * .5f, cy = o.y + o.height * .5f;
            const float theta = o.angle * .017453292519943295f;
            const float c = std::cos(theta), sine = std::sin(theta);
            const float dx = px - cx, dy = py - cy;
            // 回転四角形のローカル座標へ変換して最短距離を測定。
            const float localX = dx * c + dy * sine, localY = -dx * sine + dy * c;
            const float nearestX = (std::min)((std::max)(localX, -o.width * .5f), o.width * .5f);
            const float nearestY = (std::min)((std::max)(localY, -o.height * .5f), o.height * .5f);
            const float deltaX = localX - nearestX, deltaY = localY - nearestY;
            if (deltaX * deltaX + deltaY * deltaY > radius * radius) continue;
            if (damage) *damage = o.touchDamage;
            if (source) *source = { cx, float(kCanvasH) - cy };
            return true;
        }
        return false;
    }
    inline float ClampFloat(float v, float lo, float hi) {
        return (std::min)((std::max)(v, lo), hi);
    }
    inline float Snap(float v) {
        return snapToGrid ? float(std::round(v / float((std::max)(1, gridPixels))) * (std::max)(1, gridPixels)) : v;
    }
    inline Object* Selected() {
        for (auto& o : objects) if (o.id == selectedId) return &o;
        return nullptr;
    }
    inline int Add(int stage, int slot = -1) {
        if ((int)objects.size() >= kMaxObjects) {
            lastStatus = "上限128個です";
            return -1;
        }
        Remember();
        Object o;
        o.id = nextId++;
        o.stage = (stage <= 2 ? stage : stage + 1);
        o.resourceSlot = slot;
        if (slot >= 0 && slot < AssetEditor::SlotCount) {
            const auto& asset = AssetEditor::assets[slot];
            o.width = float((std::max)(1, asset.cellW));
            o.height = float((std::max)(1, asset.cellH));
            std::snprintf(o.name, sizeof(o.name), "Sprite_%d", o.id);
        }
        else {
            std::snprintf(o.name, sizeof(o.name), "Object_%d", o.id);
        }
        objects.push_back(o);
        selectedId = o.id;
        dirty = true;
        lastStatus = "オブジェクトを追加しました";
        return o.id;
    }
    inline int AddNamedObject(int stage, const char* name) {
        const int id = Add(stage);
        if (id < 0) return id;
        Object* o = Selected();
        if (o && name && name[0]) {
            std::snprintf(o->name, sizeof(o->name), "%s", name);
            std::snprintf(o->textureName, sizeof(o->textureName), "%s", name);
        }
        return id;
    }
    inline void DuplicateSelected() {
        const Object* src = Selected();
        if (!src || (int)objects.size() >= kMaxObjects) return;
        Remember();
        Object copy = *src;
        copy.id = nextId++;
        copy.x += 32;
        copy.y += 32;
        std::snprintf(copy.name, sizeof(copy.name), "Copy_%d", copy.id);
        objects.push_back(copy);
        selectedId = copy.id;
        dirty = true;
    }
    inline bool CanAddGripPoint(int stage) {
        Vector2 points[kMaxEditorGripPoints]{};
        // 「両方」指定なら両ステージとも上限を確認する。
        if (stage == 3)
            return CollectGripPoints(1, points, kMaxEditorGripPoints) < kMaxEditorGripPoints &&
            CollectGripPoints(2, points, kMaxEditorGripPoints) < kMaxEditorGripPoints;
        return CollectGripPoints(stage > 3 ? stage - 1 : stage, points, kMaxEditorGripPoints) < kMaxEditorGripPoints;
    }
    inline int AddType(int stage, Kind kind) {
        if (kind == Kind::GripPoint && !CanAddGripPoint(stage <= 2 ? stage : stage + 1)) {
            lastStatus = "このステージのGPは最大4個です";
            return -1;
        }
        const int id = Add(stage);
        Object* o = Selected();
        if (!o || id < 0) return -1;
        o->kind = kind;
        o->collider = (kind == Kind::Enemy || kind == Kind::DamageZone);
        o->collisionType = o->collider ? 3 : 0;
        o->width = (kind == Kind::GripPoint) ? 32.0f : ((kind == Kind::Enemy) ? 48.0f : 96.0f);
        o->height = (kind == Kind::GripPoint) ? 32.0f : ((kind == Kind::Enemy) ? 48.0f : 64.0f);
        if (kind == Kind::GripPoint) {
            o->r = 166; o->g = 113; o->b = 255;
        }
        else if (kind == Kind::Enemy) {
            o->r = 237; o->g = 106; o->b = 115;
        }
        else if (kind == Kind::DamageZone) {
            o->r = 238; o->g = 170; o->b = 66; o->a = 115;
        }
        std::snprintf(o->name, sizeof(o->name), "%s_%d",
            kind == Kind::GripPoint ? "GP" : (kind == Kind::Enemy ? "Enemy" : (kind == Kind::DamageZone ? "Hazard" : "Object")), id);
        return id;
    }
    inline bool RegisterPrefabFromSelected(const char* name) {
        Object* o = Selected();
        if (!o || !name || !name[0] || (int)prefabs.size() >= kMaxPrefabs) return false;
        Prefab p{};
        std::snprintf(p.name, sizeof(p.name), "%s", name);
        p.sample = *o;
        prefabs.push_back(p);
        dirty = true;
        lastStatus = "Prefabに登録しました（シーン保存で確定）";
        return true;
    }
    inline int InstantiatePrefab(int index, int stage) {
        if (index < 0 || index >= (int)prefabs.size() || (int)objects.size() >= kMaxObjects) return -1;
        if (prefabs[index].sample.kind == Kind::GripPoint && !CanAddGripPoint(stage)) {
            lastStatus = "GPを配置できません：各ステージ上限4個";
            return -1;
        }
        Remember();
        Object instance = prefabs[index].sample;
        instance.id = nextId++;
        instance.stage = stage;
        instance.x += 32.0f;
        instance.y += 32.0f;
        objects.push_back(instance);
        selectedId = instance.id;
        dirty = true;
        lastStatus = "Prefabからオブジェクトを配置しました";
        return instance.id;
    }
    inline void RemoveSelected() {
        const Object* candidate = Selected();
        if (!candidate || candidate->locked) return;
        Remember();
        const int before = (int)objects.size();
        objects.erase(std::remove_if(objects.begin(), objects.end(), [](const Object& o) {
            return o.id == selectedId && !o.locked;
            }), objects.end());
        if ((int)objects.size() < before) {
            selectedId = -1;
            dirty = true;
            lastStatus = "オブジェクトを削除しました";
        }
    }

    // V3: 全オブジェクトとPrefabを1つのファイルへ保存。
    // 読込時は完全解析できた時だけ旧データと交換（壊れたファイルで消さない）。
    inline void WriteObject(std::ostream& out, const char* tag, const Object& o) {
        out << tag << ' ' << o.id << ' ' << o.stage << ' ' << o.layer << ' ' << o.resourceSlot << ' '
            << o.visible << ' ' << o.locked << ' ' << o.x << ' ' << o.y << ' '
            << o.width << ' ' << o.height << ' ' << o.angle << ' '
            << int(o.r) << ' ' << int(o.g) << ' ' << int(o.b) << ' ' << int(o.a) << ' '
            << std::quoted(std::string(o.name)) << ' ' << int(o.kind) << ' '
            << o.collider << ' ' << o.patrolRange << ' ' << o.patrolSpeed << ' ' << o.touchDamage << ' ' << o.collisionType << ' '
            << o.cameraMode << ' ' << std::quoted(std::string(o.textureName)) << ' '
            << std::quoted(std::string(o.texturePath)) << '\n';
    }
    inline bool ParseObject(std::istringstream& is, Object& o, bool v3, bool v4 = false) {
        int visible = 1, locked = 0, r = 255, g = 255, b = 255, a = 255;
        std::string name;
        if (!(is >> o.id >> o.stage >> o.layer >> o.resourceSlot >> visible >> locked
            >> o.x >> o.y >> o.width >> o.height >> o.angle >> r >> g >> b >> a
            >> std::quoted(name))) return false;
        if (v3) {
            int kind = 0, collider = 0;
            if (!(is >> kind >> collider >> o.patrolRange >> o.patrolSpeed >> o.touchDamage) ||
                kind < 0 || kind > 3 || (collider != 0 && collider != 1)) return false;
            o.kind = static_cast<Kind>(kind); o.collider = collider != 0;
            // V3以前のデータには collisionType が無い。あれば読み込み、無ければ旧ダメージ判定へ。
            if (!(is >> o.collisionType)) {
                is.clear();
                o.collisionType = o.collider ? 3 : 0;
            }
            if (o.collisionType < 0 || o.collisionType>3) return false;
            if (v4) {
                std::string nameValue, pathValue;
                if (!(is >> o.cameraMode >> std::quoted(nameValue) >> std::quoted(pathValue))) return false;
                if (o.cameraMode < 0 || o.cameraMode > 2 ||
                    nameValue.size() >= sizeof(o.textureName) || pathValue.size() >= sizeof(o.texturePath)) return false;
                std::snprintf(o.textureName, sizeof(o.textureName), "%s", nameValue.c_str());
                std::snprintf(o.texturePath, sizeof(o.texturePath), "%s", pathValue.c_str());
            }
        }
        if (o.id <= 0 || o.stage < 1 || o.stage > 5 || o.layer < 0 || o.layer > 2 ||
            o.resourceSlot < -1 || o.resourceSlot >= AssetEditor::SlotCount ||
            !std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.width) || !std::isfinite(o.height) ||
            !std::isfinite(o.angle) || !std::isfinite(o.patrolRange) || !std::isfinite(o.patrolSpeed) ||
            o.width <= 0 || o.height <= 0 || o.width > 4096 || o.height > 4096 ||
            o.patrolRange < 0 || o.patrolRange > 1024 || o.patrolSpeed < 0 || o.patrolSpeed > 1.0f ||
            o.touchDamage < 0 || o.touchDamage > 100 || name.size() >= sizeof(o.name) ||
            r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) return false;
        o.visible = visible != 0; o.locked = locked != 0;
        o.r = static_cast<unsigned char>(r); o.g = static_cast<unsigned char>(g);
        o.b = static_cast<unsigned char>(b); o.a = static_cast<unsigned char>(a);
        std::snprintf(o.name, sizeof(o.name), "%s", name.c_str());
        return true;
    }
    inline bool Save() {
        const std::string target = SceneFilePath(), temp = target + ".tmp";
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) { lastStatus = "シーン保存失敗: ファイルを開けません"; return false; }
        out << "MiniUnitySceneV4\n";
        out << nextId << ' ' << snapToGrid << ' ' << gridPixels << ' ' << showGrid << ' '
            << showBounds << ' ' << drawingEnabled << ' ' << sceneZoom << ' '
            << sceneCenterX << ' ' << sceneCenterY << ' ' << showColliders << '\n';
        out << std::fixed << std::setprecision(5);
        for (const Object& o : objects) WriteObject(out, "O", o);
        for (const Prefab& p : prefabs) {
            out << "P " << std::quoted(std::string(p.name)) << ' ';
            // Prefix is written as a separate field, the O tag is omitted for prefab records.
            WriteObject(out, "", p.sample);
        }
        out.flush(); const bool ok = out.good(); out.close();
        if (!ok) { std::remove(temp.c_str()); lastStatus = "シーン保存失敗: 書込エラー"; return false; }
        if (!MoveFileExA(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::remove(temp.c_str()); lastStatus = "シーン保存失敗: ファイル交換エラー"; return false;
        }
        dirty = false; lastStatus = "シーン・Prefabを保存しました"; return true;
    }
    inline bool Load() {
        std::ifstream in(SceneFilePath(), std::ios::binary);
        if (!in) { lastStatus = "シーンファイルなし: 空のシーンから開始"; return false; }
        std::string sig;
        if (!std::getline(in, sig) ||
            (sig != "MiniUnitySceneV1" && sig != "MiniUnitySceneV2" && sig != "MiniUnitySceneV3" && sig != "MiniUnitySceneV4")) {
            lastStatus = "シーン形式が不正です（配置はそのまま）"; return false;
        }
        const bool v4 = sig == "MiniUnitySceneV4";
        const bool v3 = sig == "MiniUnitySceneV3" || v4, v2 = sig == "MiniUnitySceneV2";
        int savedNext = 1, sn = 1, grid = 16, sg = 1, sb = 0, enable = 1, colliderView = 1;
        float zoom = 1.0f, cx = kCanvasW * .5f, cy = kCanvasH * .5f;
        if (!(in >> savedNext >> sn >> grid >> sg >> sb >> enable)) return false;
        if ((v2 || v3) && !(in >> zoom >> cx >> cy)) return false;
        if (v3 && !(in >> colliderView)) return false;
        if (!std::isfinite(zoom) || zoom < .25f || zoom > 4.0f ||
            !std::isfinite(cx) || !std::isfinite(cy) || std::fabs(cx) > 20000 || std::fabs(cy) > 20000) return false;
        std::string junk; std::getline(in, junk);
        std::vector<Object> parsed;
        std::vector<Prefab> parsedPrefabs;
        int maxId = 0;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::istringstream is(line);
            std::string tag;
            if (v3 && !(is >> tag)) return false;
            if (v3 && tag == "P") {
                Prefab p{};
                std::string name;
                if (!(is >> std::quoted(name)) || name.empty() || name.size() >= sizeof(p.name) ||
                    (int)parsedPrefabs.size() >= kMaxPrefabs || !ParseObject(is, p.sample, true, v4)) return false;
                std::snprintf(p.name, sizeof(p.name), "%s", name.c_str());
                parsedPrefabs.push_back(p);
            }
            else if ((!v3 || tag == "O") && (int)parsed.size() < kMaxObjects) {
                Object o{};
                if (!ParseObject(is, o, v3, v4)) return false;
                for (const auto& q : parsed) if (q.id == o.id) return false;
                maxId = (std::max)(maxId, o.id);
                parsed.push_back(o);
            }
            else { lastStatus = "シーン読込失敗: 無効な行があります"; return false; }
        }
        if (in.bad()) return false;
        // 解析完了してから一度に反映。
        objects.swap(parsed); prefabs.swap(parsedPrefabs);
        undoStack.clear(); redoStack.clear();
        nextId = (std::max)((std::max)(savedNext, maxId + 1), 1);
        selectedId = -1; dragging = false; dragMode = DragMode::None;
        snapToGrid = sn != 0; gridPixels = std::clamp(grid, 4, 128);
        showGrid = sg != 0; showBounds = sb != 0; drawingEnabled = enable != 0;
        sceneZoom = zoom; sceneCenterX = cx; sceneCenterY = cy;
        showColliders = colliderView != 0;
        dirty = false; lastStatus = "シーン・Prefabを読み込みました"; return true;
    }

    inline unsigned int Color(const Object& o) {
        // Novice color is RRGGBBAA.
        return (unsigned(o.r) << 24) | (unsigned(o.g) << 16) | (unsigned(o.b) << 8) | unsigned(o.a);
    }
    struct Point { float x, y; };
    inline void Corners(const Object& o, Point(&p)[4]) {
        const float cx = o.x + o.width * 0.5f, cy = o.y + o.height * 0.5f;
        const float rad = o.angle * 0.017453292519943295f;
        const float c = std::cos(rad), s = std::sin(rad);
        const float lx[4] = { -o.width * .5f, o.width * .5f, -o.width * .5f, o.width * .5f };
        const float ly[4] = { -o.height * .5f, -o.height * .5f, o.height * .5f, o.height * .5f };
        for (int k = 0; k < 4; ++k) {
            p[k] = { cx + lx[k] * c - ly[k] * s, cy + lx[k] * s + ly[k] * c };
        }
    }
    inline void DrawWorld(int stage, int layer) {
        if (!drawingEnabled) return;
        for (const Object& o : objects) {
            if (!o.visible || !InStage(o, stage) || o.layer != layer) continue;
            Object shown = o;
            shown.x = RuntimeX(o);
            Point p[4]{};
            Corners(shown, p);
            // 【重要】エディター座標（Y下向き）をゲームワールド（Y上向き）に
            // 揃えてから、ボスやGPと同じカメラ変換で描画する。
            const float centerX = shown.x + shown.width * 0.5f;
            const float centerY = shown.y + shown.height * 0.5f;
            const Vector2i center = PortalDraw::ToScreen({ centerX, float(kCanvasH) - centerY });
            // 「位置のみ連動」はワールド座標に置くが大きさはズームしない。
            // 「画面固定」はエディターで設定したx/yをそのまま画面座標として描く。
            const float scale = o.cameraMode == 0 ? GetCameraZoom() : 1.0f;
            for (Point& point : p) {
                if (o.cameraMode == 2) {
                    // pの値はすでに画面左上原点の値
                }
                else {
                    point.x = float(center.x) + (point.x - centerX) * scale;
                    point.y = float(center.y) + (point.y - centerY) * scale;
                }
            }
            // GPはPortalArenaが触手ターゲットと同じ位置に描画する。
            if (o.kind == Kind::GripPoint) continue;
            if (o.texturePath[0]) {
                LoadedTexture* texture = FindTexture(o.texturePath);
                if (texture && texture->handle >= 0 && texture->width > 0 && texture->height > 0) {
                    Novice::DrawQuad((int)p[0].x, (int)p[0].y, (int)p[1].x, (int)p[1].y,
                        (int)p[2].x, (int)p[2].y, (int)p[3].x, (int)p[3].y,
                        0, 0, texture->width, texture->height, texture->handle, WHITE);
                    continue;
                }
            }
            if (o.resourceSlot >= 0 && o.resourceSlot < AssetEditor::SlotCount) {
                const AssetEditor::Slot slot = (AssetEditor::Slot)o.resourceSlot;
                const AssetEditor::Data& a = AssetEditor::assets[slot];
                if (a.handle >= 0 && a.width > 0 && a.height > 0) {
                    const AssetEditor::FrameSource f = AssetEditor::GetFrame(slot, a.frame);
                    Novice::DrawQuad((int)p[0].x, (int)p[0].y, (int)p[1].x, (int)p[1].y,
                        (int)p[2].x, (int)p[2].y, (int)p[3].x, (int)p[3].y,
                        f.x, f.y, f.w, f.h, a.handle, WHITE);
                    // 読み込み済みPNGの当たり判定も確認できるようにする。
                    if (showColliders && (o.collider || o.collisionType != 0)) {
                        const unsigned int line = 0xFFCA43FF;
                        Novice::DrawLine(int(p[0].x), int(p[0].y), int(p[1].x), int(p[1].y), line);
                        Novice::DrawLine(int(p[1].x), int(p[1].y), int(p[3].x), int(p[3].y), line);
                        Novice::DrawLine(int(p[3].x), int(p[3].y), int(p[2].x), int(p[2].y), line);
                        Novice::DrawLine(int(p[2].x), int(p[2].y), int(p[0].x), int(p[0].y), line);
                    }
                    continue;
                }
            }
            // 画像がない場合もカメラを通した四隅を使う（座標ずれ防止）。
            {
                const unsigned int color = Color(o);
                Novice::DrawLine((int)p[0].x, (int)p[0].y, (int)p[1].x, (int)p[1].y, color);
                Novice::DrawLine((int)p[1].x, (int)p[1].y, (int)p[3].x, (int)p[3].y, color);
                Novice::DrawLine((int)p[3].x, (int)p[3].y, (int)p[2].x, (int)p[2].y, color);
                Novice::DrawLine((int)p[2].x, (int)p[2].y, (int)p[0].x, (int)p[0].y, color);
            }
            if (showColliders && (o.collider || o.collisionType != 0) && o.kind != Kind::GripPoint) {
                const unsigned int outline = 0xFFCA43FF;
                Novice::DrawLine(int(p[0].x), int(p[0].y), int(p[1].x), int(p[1].y), outline);
                Novice::DrawLine(int(p[1].x), int(p[1].y), int(p[3].x), int(p[3].y), outline);
                Novice::DrawLine(int(p[3].x), int(p[3].y), int(p[2].x), int(p[2].y), outline);
                Novice::DrawLine(int(p[2].x), int(p[2].y), int(p[0].x), int(p[0].y), outline);
            }
        }
    }

#ifdef USE_IMGUI
    // Scene view transforms only. Game objects are still in the original 1280x720
    // screen coordinate system; zooming this preview does NOT scale the actual game.
    struct SceneViewport {
        ImVec2 topLeft{};
        float width = 1.0f;
        float height = 1.0f;
        float pixelsPerWorld = 1.0f;
        ImVec2 ToScreen(float x, float y) const {
            return ImVec2(topLeft.x + width * .5f + (x - sceneCenterX) * pixelsPerWorld,
                topLeft.y + height * .5f + (y - sceneCenterY) * pixelsPerWorld);
        }
        Point ToWorld(ImVec2 screen) const {
            return { sceneCenterX + (screen.x - topLeft.x - width * .5f) / pixelsPerWorld,
                sceneCenterY + (screen.y - topLeft.y - height * .5f) / pixelsPerWorld };
        }
    };

    inline bool HitObject(const Object& o, Point mouse) {
        const float cx = o.x + o.width * .5f, cy = o.y + o.height * .5f;
        const float rad = o.angle * .017453292519943295f;
        const float dx = mouse.x - cx, dy = mouse.y - cy;
        const float lx = dx * std::cos(rad) + dy * std::sin(rad);
        const float ly = -dx * std::sin(rad) + dy * std::cos(rad);
        return std::fabs(lx) <= o.width * .5f && std::fabs(ly) <= o.height * .5f;
    }
    inline Point RotationHandle(const Object& o, float screenOffsetWorld) {
        const float radians = o.angle * .017453292519943295f;
        return { o.x + o.width * .5f + std::sin(radians) * (o.height * .5f + screenOffsetWorld),
            o.y + o.height * .5f - std::cos(radians) * (o.height * .5f + screenOffsetWorld) };
    }
    inline bool CloseTo(ImVec2 a, ImVec2 b, float radius) {
        const float dx = a.x - b.x, dy = a.y - b.y;
        return dx * dx + dy * dy <= radius * radius;
    }
    inline float NormalizeDegrees(float degrees) {
        while (degrees > 180.0f) degrees -= 360.0f;
        while (degrees < -180.0f) degrees += 360.0f;
        return degrees;
    }
    inline void StartGizmoDrag(DragMode mode, Point mouse, Object& o) {
        Remember(); // one Undo entry for the whole mouse gesture
        dragInitial = o;
        dragMode = mode;
        dragging = true;
        dragStartMouseX = mouse.x;
        dragStartMouseY = mouse.y;
        dragOffsetX = mouse.x - o.x;
        dragOffsetY = mouse.y - o.y;
    }
    // Pure transform calculations: separated from mouse input to allow tests.
    inline void UpdateGizmoDrag(Point mouse) {
        Object* o = Selected();
        if (!o || o->locked || !dragging) return;
        const float cx = dragInitial.x + dragInitial.width * .5f;
        const float cy = dragInitial.y + dragInitial.height * .5f;
        if (dragMode == DragMode::Move) {
            o->x = ClampFloat(Snap(mouse.x - dragOffsetX), -2048.0f, 4096.0f);
            o->y = ClampFloat(Snap(mouse.y - dragOffsetY), -2048.0f, 4096.0f);
        }
        else if (dragMode == DragMode::Rotate) {
            constexpr float kDegrees = 57.29577951308232f;
            const float before = std::atan2(dragStartMouseY - cy, dragStartMouseX - cx);
            const float now = std::atan2(mouse.y - cy, mouse.x - cx);
            float degrees = dragInitial.angle + (now - before) * kDegrees;
            if (ImGui::GetIO().KeyShift) degrees = std::round(degrees / 15.0f) * 15.0f;
            o->angle = NormalizeDegrees(degrees);
        }
        else if (dragMode == DragMode::Scale) {
            const float startDistance = (std::max)(8.0f,
                std::hypot(dragStartMouseX - cx, dragStartMouseY - cy));
            const float distance = std::hypot(mouse.x - cx, mouse.y - cy);
            const float factor = ClampFloat(distance / startDistance, .05f, 20.0f);
            o->width = ClampFloat(dragInitial.width * factor, 1.0f, 2000.0f);
            o->height = ClampFloat(dragInitial.height * factor, 1.0f, 2000.0f);
            // Scale around the center to avoid a sudden jump when dragging.
            o->x = cx - o->width * .5f;
            o->y = cy - o->height * .5f;
        }
        dirty = true;
    }

    // マリオメーカー風の配置ツール。ステージの配置データに保存されるObjectを直接扱う。
    // 操作モードはUI専用で、プレイヤーの物理・触手の更新には触れない。
    enum class PaintTool { Select, Ground, Wall, Platform, Erase, Tile, TileErase };
    inline PaintTool paintTool = PaintTool::Select;
    inline bool marioShowPalette = true; // F5上部から再表示可能な左パレット
    inline bool paintStrokeActive = false;
    inline int paintLastCellX = -999999, paintLastCellY = -999999;

    inline void PaintAt(int stage, Point p) {
        const int cell = (std::max)(4, gridPixels);
        const int gx = int(std::floor(p.x / float(cell)));
        const int gy = int(std::floor(p.y / float(cell)));
        if (gx == paintLastCellX && gy == paintLastCellY) return;
        paintLastCellX = gx; paintLastCellY = gy;
        const float x = float(gx * cell), y = float(gy * cell);
        const int encodedStage = stage <= 2 ? stage : stage + 1;
        if (paintTool == PaintTool::Erase) {
            for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
                if (it->stage != encodedStage || it->locked || it->kind != Kind::Decoration) continue;
                if (x >= it->x && x < it->x + it->width && y >= it->y && y < it->y + it->height) {
                    const int id = it->id;
                    objects.erase(std::remove_if(objects.begin(), objects.end(), [id](const Object& o) {return o.id == id;}), objects.end());
                    if (selectedId == id) selectedId = -1;
                    dirty = true;
                    break;
                }
            }
            return;
        }
        if (paintTool == PaintTool::Select || int(objects.size()) >= kMaxObjects) return;
        for (const Object& obj : objects) {
            if (obj.stage == encodedStage && obj.kind == Kind::Decoration && obj.collisionType != 0 &&
                obj.x == x && obj.y == y && obj.width == cell && obj.height == cell) return;
        }
        Object o{};
        o.id = nextId++;
        o.stage = encodedStage;
        o.x = x; o.y = y;
        o.width = float(cell); o.height = float(cell);
        o.collisionType = paintTool == PaintTool::Platform ? 2 : 1;
        o.r = paintTool == PaintTool::Ground ? 106 : paintTool == PaintTool::Wall ? 129 : 206;
        o.g = paintTool == PaintTool::Ground ? 166 : paintTool == PaintTool::Wall ? 144 : 178;
        o.b = paintTool == PaintTool::Ground ? 112 : paintTool == PaintTool::Wall ? 166 : 111;
        o.a = 255;
        std::snprintf(o.name, sizeof(o.name), "%s_%d",
            paintTool == PaintTool::Ground ? "Ground" : paintTool == PaintTool::Wall ? "Wall" : "Platform", o.id);
        objects.push_back(o);
        dirty = true;
    }

    inline void DrawCanvas(int stage, Vector2 player, Vector2 boss) {
        const ImVec2 remaining = ImGui::GetContentRegionAvail();
        const float width = ClampFloat(remaining.x, 160.0f, 1600.0f);
        // 横長の小さなプレビューではなく、残っている縦領域を配置画面に使う。
        // ビュー座標変換は従来通り等倍比率なので、当たり判定・配置座標は変わらない。
        const float height = (std::max)(width * (float(kCanvasH) / kCanvasW), remaining.y - 58.0f);
        const ImVec2 begin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##MiniUnitySceneCanvas", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 mouse = ImGui::GetMousePos();
        const float baseScale = width / float(kCanvasW);
        SceneViewport view{ begin, width, height, baseScale * sceneZoom };

        // Zoom toward the cursor rather than toward (0,0).
        if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
            const Point before = view.ToWorld(mouse);
            const float next = ClampFloat(sceneZoom * std::pow(1.15f, ImGui::GetIO().MouseWheel), .25f, 4.0f);
            if (next != sceneZoom) {
                sceneZoom = next;
                view.pixelsPerWorld = baseScale * sceneZoom;
                const Point after = view.ToWorld(mouse);
                sceneCenterX += before.x - after.x;
                sceneCenterY += before.y - after.y;
                dirty = true;
            }
        }
        // Middle mouse: pan the editor's scene camera. This does not affect gameplay.
        if (hovered && (ImGui::IsMouseDown(2) || ImGui::IsMouseDown(1))) {
            sceneCenterX -= ImGui::GetIO().MouseDelta.x / view.pixelsPerWorld;
            sceneCenterY -= ImGui::GetIO().MouseDelta.y / view.pixelsPerWorld;
            dirty = true;
        }
        auto pos = [&](float x, float y) { return view.ToScreen(x, y); };
        ImDrawList* d = ImGui::GetWindowDrawList();
        d->AddRectFilled(begin, ImVec2(begin.x + width, begin.y + height), IM_COL32(27, 39, 53, 255));
        d->PushClipRect(begin, ImVec2(begin.x + width, begin.y + height), true);

        const Point tl = view.ToWorld(begin);
        const Point br = view.ToWorld(ImVec2(begin.x + width, begin.y + height));
        if (showGrid) {
            // At small zoom levels, skip grid lines too close together.
            int step = (std::max)(4, gridPixels);
            while (step * view.pixelsPerWorld < 7.0f) step *= 2;
            const int minX = int(std::floor(tl.x / step)) * step;
            const int minY = int(std::floor(tl.y / step)) * step;
            for (int x = minX, n = 0; x <= br.x && n < 500; x += step, ++n)
                d->AddLine(pos(float(x), tl.y), pos(float(x), br.y), IM_COL32(64, 77, 90, 150));
            for (int y = minY, n = 0; y <= br.y && n < 500; y += step, ++n)
                d->AddLine(pos(tl.x, float(y)), pos(br.x, float(y)), IM_COL32(64, 77, 90, 150));
        }
        // Screen boundary is visible even after panning / zooming away.
        d->AddRect(pos(0, 0), pos(float(kCanvasW), float(kCanvasH)), IM_COL32(155, 184, 211, 210));
        // マップチップの配置プレビュー。描画はEditor座標、物理はワールド座標。
        TileMapEditor::DrawEditorOverlay(stage, pos, d);
        // Player and boss are reference markers only; never editable here.
        const ImVec2 pr = pos(player.x, kCanvasH - player.y);
        const ImVec2 brp = pos(boss.x, kCanvasH - boss.y);
        d->AddCircleFilled(pr, 5.0f, IM_COL32(113, 211, 255, 255));
        d->AddCircleFilled(brp, 6.0f, IM_COL32(255, 120, 122, 255));
        d->AddText(ImVec2(pr.x + 7, pr.y), IM_COL32(190, 228, 255, 255), "Player");
        d->AddText(ImVec2(brp.x + 7, brp.y), IM_COL32(255, 192, 192, 255), "Boss");
        for (int layer = 0; layer <= 2; ++layer) {
            for (const Object& o : objects) {
                if (o.layer != layer || !InStage(o, stage) || !o.visible) continue;
                Point corners[4]{}; Corners(o, corners);
                const ImVec2 a = pos(corners[0].x, corners[0].y);
                const ImVec2 b = pos(corners[1].x, corners[1].y);
                const ImVec2 c = pos(corners[3].x, corners[3].y);
                const ImVec2 d0 = pos(corners[2].x, corners[2].y);
                const unsigned int color = IM_COL32(o.r, o.g, o.b, (std::max)(70, int(o.a)));
                d->AddQuadFilled(a, b, c, d0, color);
                d->AddQuad(a, b, c, d0,
                    o.id == selectedId ? IM_COL32(255, 218, 86, 255) : IM_COL32(215, 220, 230, 220), 2.0f);
                d->AddText(ImVec2(a.x + 3, a.y + 2), IM_COL32(255, 255, 255, 255), o.name);
                if (showColliders && (o.collider || o.collisionType != 0)) {
                    d->AddQuad(a, b, c, d0, IM_COL32(255, 195, 62, 255), 2.0f);
                }
            }
        }
        // Scene gizmos are edit-only overlays; they never get drawn in the game.
        Object* selected = Selected();
        ImVec2 scaleHandle{}, rotateHandle{}, centerHandle{};
        if (selected && InStage(*selected, stage) && selected->visible && !selected->locked) {
            Point corners[4]{}; Corners(*selected, corners);
            scaleHandle = pos(corners[3].x, corners[3].y);
            const Point rot = RotationHandle(*selected, 25.0f / view.pixelsPerWorld);
            rotateHandle = pos(rot.x, rot.y);
            centerHandle = pos(selected->x + selected->width * .5f, selected->y + selected->height * .5f);
            if (gizmoMode == GizmoMode::Move) {
                d->AddLine(ImVec2(centerHandle.x - 14, centerHandle.y),
                    ImVec2(centerHandle.x + 14, centerHandle.y), IM_COL32(83, 218, 150, 255), 2.5f);
                d->AddLine(ImVec2(centerHandle.x, centerHandle.y - 14),
                    ImVec2(centerHandle.x, centerHandle.y + 14), IM_COL32(83, 218, 150, 255), 2.5f);
                d->AddCircleFilled(centerHandle, 5.5f, IM_COL32(83, 218, 150, 255));
            }
            else if (gizmoMode == GizmoMode::Rotate) {
                const Point top = { (corners[0].x + corners[1].x) * .5f,
                    (corners[0].y + corners[1].y) * .5f };
                d->AddLine(pos(top.x, top.y), rotateHandle, IM_COL32(255, 194, 70, 255), 2.0f);
                d->AddCircleFilled(rotateHandle, 7.5f, IM_COL32(255, 194, 70, 255));
                d->AddCircle(rotateHandle, 9.0f, IM_COL32(52, 49, 35, 255));
            }
            else if (gizmoMode == GizmoMode::Scale) {
                d->AddRectFilled(ImVec2(scaleHandle.x - 7, scaleHandle.y - 7),
                    ImVec2(scaleHandle.x + 7, scaleHandle.y + 7), IM_COL32(111, 196, 255, 255));
                d->AddRect(ImVec2(scaleHandle.x - 7, scaleHandle.y - 7),
                    ImVec2(scaleHandle.x + 7, scaleHandle.y + 7), IM_COL32(32, 60, 80, 255));
            }
        }
        d->PopClipRect();
        d->AddRect(begin, ImVec2(begin.x + width, begin.y + height), IM_COL32(118, 142, 170, 255));

        const Point worldMouse = view.ToWorld(mouse);
        // ブラシはワールド座標に変換してからセル配置。
        // ドラッグ開始の1回だけUndo履歴に積み、ドラッグ中は同じストロークとして扱う。
        if (paintTool != PaintTool::Select && hovered && ImGui::IsMouseDown(0)) {
            const bool tileBrush = paintTool == PaintTool::Tile || paintTool == PaintTool::TileErase;
            if (tileBrush) {
                if (!TileMapEditor::strokeActive) TileMapEditor::BeginStroke(stage);
                const int tx = int(std::floor(worldMouse.x / float(TileMapEditor::kTileSize)));
                const int ty = int(std::floor(worldMouse.y / float(TileMapEditor::kTileSize)));
                TileMapEditor::Paint(stage, tx, ty, paintTool == PaintTool::TileErase);
            }
            else {
                if (!paintStrokeActive) { Remember(); paintStrokeActive = true; paintLastCellX = -999999; paintLastCellY = -999999; }
                PaintAt(stage, worldMouse);
            }
        }
        if (!ImGui::IsMouseDown(0)) { paintStrokeActive = false; TileMapEditor::EndStroke(); }
        if (paintTool == PaintTool::Select && hovered && ImGui::IsMouseClicked(0)) {
            dragging = false;
            dragMode = DragMode::None;
            // If an existing handle was clicked, keep the selection.
            bool startedHandle = false;
            if (selected && !selected->locked && selected->visible && InStage(*selected, stage)) {
                if (gizmoMode == GizmoMode::Rotate && CloseTo(mouse, rotateHandle, 13.0f)) {
                    StartGizmoDrag(DragMode::Rotate, worldMouse, *selected);
                    startedHandle = true;
                }
                else if (gizmoMode == GizmoMode::Scale && CloseTo(mouse, scaleHandle, 13.0f)) {
                    StartGizmoDrag(DragMode::Scale, worldMouse, *selected);
                    startedHandle = true;
                }
            }
            if (!startedHandle) {
                const int oldSelection = selectedId;
                selectedId = -1;
                // Topmost layer first, and then last object in that layer.
                for (int layer = 2; layer >= 0 && selectedId < 0; --layer) {
                    for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
                        Object& o = *it;
                        if (o.layer != layer || !o.visible || !InStage(o, stage) || !HitObject(o, worldMouse)) continue;
                        selectedId = o.id;
                        if (!o.locked && gizmoMode == GizmoMode::Move)
                            StartGizmoDrag(DragMode::Move, worldMouse, o);
                        break;
                    }
                }
                (void)oldSelection;
            }
        }
        if (dragging && ImGui::IsMouseDown(0)) UpdateGizmoDrag(worldMouse);
        if (!ImGui::IsMouseDown(0)) { dragging = false; dragMode = DragMode::None; }
        ImGui::TextUnformatted("左クリック: 選択  /  右・中ドラッグ: 視点移動  /  ホイール: ズーム  /  ハンドル: 編集");
        ImGui::Text("ズーム %.0f%%  |  視点 (%.0f, %.0f)  |  画面座標 (%.0f, %.0f)",
            sceneZoom * 100.0f, sceneCenterX, sceneCenterY, worldMouse.x, worldMouse.y);
    }

    inline void DrawMarioCanvas(int stage, Vector2 player, Vector2 boss) {
        // 既存の選択・ハンドル・カメラ操作の実装をそのまま使用する。
        DrawCanvas(stage, player, boss);
    }

    inline void DrawPanel(int stage, Vector2 player, Vector2 boss) {
        // 編集対象のステージ選択。0 はゲーム中のステージを表示する。
        static int stageToEdit = 0;
        const char* stageChoices[] = { "プレイ中のステージ", "ステージ1", "ステージ2", "ステージ3", "ステージ4" };
        ImGui::PushItemWidth(155.0f);
        ImGui::Combo("ステージ", &stageToEdit, stageChoices, 5);
        ImGui::PopItemWidth();
        if (stageToEdit > 0) stage = stageToEdit;
        ImGui::SameLine();
        if (ImGui::Button("マップチップを編集")) {
            paintTool = PaintTool::Tile;
            marioShowPalette = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("保存")) { Save(); TileMapEditor::Save(); }
        ImGui::SameLine();
        if (ImGui::Button("読込")) { Load(); TileMapEditor::Load(); }
        ImGui::SameLine();
        if (ImGui::Button("戻す")) Undo();
        ImGui::SameLine();
        if (ImGui::Button("やり直し")) Redo();
        ImGui::SameLine();
        ImGui::TextUnformatted(dirty ? "* 未保存" : "保存済み");
        ImGui::Separator();
        // 左右を折りたたむと中央キャンバスを最大化できる。
        static bool showInspector = true;
        ImGui::Checkbox("左パーツ一覧", &marioShowPalette);
        ImGui::SameLine();
        ImGui::Checkbox("右Inspector", &showInspector);
        ImGui::SameLine();
        ImGui::TextDisabled("広い配置画面が必要なら両方OFF");
        const float available = ImGui::GetContentRegionAvail().x;
        const float leftWidth = marioShowPalette ? (available < 650 ? 126.0f : 158.0f) : 0.0f;
        const float rightWidth = showInspector ? (available < 650 ? 180.0f : 236.0f) : 0.0f;
        if (marioShowPalette) {
            ImGui::BeginChild("##MarioPalette", ImVec2(leftWidth, 0), true);
            ImGui::TextUnformatted("パーツ / PALETTE");
            ImGui::Separator();
            auto toolButton = [](const char* text, PaintTool t) {
                if (ImGui::Selectable(text, paintTool == t, 0, ImVec2(-1, 28))) paintTool = t;
                };
            toolButton("選択・移動", PaintTool::Select);
            toolButton("床を描く", PaintTool::Ground);
            toolButton("壁を描く", PaintTool::Wall);
            toolButton("足場を描く", PaintTool::Platform);
            toolButton("消しゴム", PaintTool::Erase);
            ImGui::Separator();
            toolButton("マップチップ配置", PaintTool::Tile);
            toolButton("マップチップ消去", PaintTool::TileErase);
            if (paintTool == PaintTool::Tile || paintTool == PaintTool::TileErase) {
                TileMapEditor::DrawPalette(stage);
            }
            ImGui::Separator();
            ImGui::TextUnformatted("自由配置");
            static char newObjectName[64] = "NewObject";
            ImGui::InputText("##NewObjectName", newObjectName, sizeof(newObjectName));
            if (ImGui::Button("名前で新規オブジェクト", ImVec2(-1, 0))) {
                if (newObjectName[0]) AddNamedObject(stage, newObjectName);
            }
            if (ImGui::Button("PNG追加", ImVec2(-1, 0))) Add(stage, AssetEditor::Custom1);
            if (ImGui::Button("GP追加", ImVec2(-1, 0))) AddType(stage, Kind::GripPoint);
            if (ImGui::Button("敵追加", ImVec2(-1, 0))) AddType(stage, Kind::Enemy);
            if (ImGui::Button("危険エリア", ImVec2(-1, 0))) AddType(stage, Kind::DamageZone);
            if (ImGui::Button("四角を追加", ImVec2(-1, 0))) Add(stage);
            ImGui::Separator();
            ImGui::TextUnformatted("配置済み");
            ImGui::BeginChild("##MarioHierarchy", ImVec2(0, 0), false);
            for (const auto& o : objects) {
                if (!InStage(o, stage)) continue;
                ImGui::PushID(o.id);
                if (ImGui::Selectable(o.name, selectedId == o.id)) { selectedId = o.id;paintTool = PaintTool::Select; }
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::SameLine();
        } // 左パレット
        const float panelSpacing = (marioShowPalette ? 8.0f : 0.0f) + (showInspector ? 8.0f : 0.0f);
        const float canvasWidth = (std::max)(180.0f, available - leftWidth - rightWidth - panelSpacing);

        ImGui::BeginChild("##MarioStageCanvas", ImVec2(canvasWidth, 0), true);
        ImGui::Text("STAGE %d  /  配置キャンバス", stage);
        ImGui::SameLine();
        if (ImGui::Button("視点リセット")) ResetSceneView();
        ImGui::Checkbox("グリッド", &showGrid);
        ImGui::SameLine();
        ImGui::Checkbox("吸着", &snapToGrid);
        ImGui::SameLine();
        ImGui::Checkbox("判定表示", &showColliders);
        ImGui::PushItemWidth(90.0f);
        ImGui::SliderInt("マス(px)", &gridPixels, 8, 64);
        ImGui::PopItemWidth();
        if (paintTool == PaintTool::Select) {
            if (ImGui::SmallButton("移動")) gizmoMode = GizmoMode::Move;
            ImGui::SameLine();
            if (ImGui::SmallButton("回転")) gizmoMode = GizmoMode::Rotate;
            ImGui::SameLine();
            if (ImGui::SmallButton("拡縮")) gizmoMode = GizmoMode::Scale;
        }
        else {
            ImGui::TextUnformatted("左ドラッグで連続配置 / 削除");
        }
        DrawMarioCanvas(stage, player, boss);
        ImGui::EndChild();
        if (showInspector) {
            ImGui::SameLine();
            ImGui::BeginChild("##MarioInspector", ImVec2(0, 0), true);
            ImGui::TextUnformatted("INSPECTOR / 設定");
            ImGui::Separator();
            if (ImGui::Button("複製")) DuplicateSelected();
            ImGui::SameLine();
            if (ImGui::Button("削除")) RemoveSelected();
            {
                Object* o = Selected();
                if (!o) ImGui::TextUnformatted("オブジェクトを選択してください");
                else {
                    if (ImGui::InputText("名前", o->name, sizeof(o->name))) dirty = true;
                    int k = static_cast<int>(o->kind);
                    const char* kinds[] = { "装飾", "GP", "巡回敵", "ダメージ領域" };
                    if (ImGui::Combo("種類", &k, kinds, 4)) {
                        const Kind nextKind = static_cast<Kind>(k);
                        if (nextKind == Kind::GripPoint && o->kind != Kind::GripPoint &&
                            !CanAddGripPoint(o->stage)) {
                            lastStatus = "GPに変更できません：各ステージ上限4個";
                        }
                        else {
                            o->kind = nextKind;
                            if (o->kind == Kind::Enemy || o->kind == Kind::DamageZone) { o->collider = true; o->collisionType = 3; }
                            dirty = true;
                        }
                    }
                    if (ImGui::Checkbox("表示", &o->visible)) dirty = true;
                    ImGui::SameLine();
                    if (ImGui::Checkbox("ロック", &o->locked)) dirty = true;
                    int stageValue = o->stage - 1;
                    const char* objectStageChoices[] = { "ステージ1", "ステージ2", "ステージ1・2共通", "ステージ3", "ステージ4" };
                    if (ImGui::Combo("表示ステージ", &stageValue, objectStageChoices, 5)) { o->stage = stageValue + 1; dirty = true; }
                    const char* layers[] = { "背景／ボスの後ろ", "ボスの前・プレイヤーの後ろ", "プレイヤーの前" };
                    if (ImGui::Combo("描画レイヤー", &o->layer, layers, 3)) dirty = true;
                    if (!o->locked) {
                        if (ImGui::DragFloat2("位置 X / Y", &o->x, 1.0f)) dirty = true;
                        if (ImGui::DragFloat2("幅 / 高さ", &o->width, 1.0f, 1.0f, 2000.0f)) {
                            o->width = ClampFloat(o->width, 1.0f, 2000.0f);
                            o->height = ClampFloat(o->height, 1.0f, 2000.0f);
                            dirty = true;
                        }
                        if (ImGui::SliderFloat("回転(度)", &o->angle, -180.0f, 180.0f)) dirty = true;
                    }
                    // 新規PNG・四角・Prefabの装飾にも接触判定を設定できる。
                    // GripPointは触手の接続判定を別処理で管理するため対象外。
                    if (o->kind != Kind::GripPoint) {
                        ImGui::Separator();
                        ImGui::TextUnformatted("当たり判定（新しい画像にも設定可能）");
                        const char* collisionNames[] = { "なし", "床・壁（押し戻す）", "すり抜け床（上から着地）", "接触ダメージ" };
                        if (ImGui::Combo("判定タイプ", &o->collisionType, collisionNames, 4)) {
                            o->collider = (o->collisionType == 3); dirty = true;
                        }
                        if (o->collisionType == 3) {
                            if (ImGui::SliderInt("接触ダメージ", &o->touchDamage, 0, 30)) dirty = true;
                            ImGui::TextUnformatted("画像の回転角に沿うダメージ判定です。");
                        }
                        else if (o->collisionType == 1 || o->collisionType == 2) {
                            ImGui::TextUnformatted("物理判定：画像の座標・幅・高さで床と壁を作れます。");
                            ImGui::TextUnformatted("回転画像の物理判定は軸平行矩形です（傾斜面は非対応）。");
                        }
                        if (o->kind == Kind::Enemy) {
                            if (ImGui::SliderFloat("巡回幅(px)", &o->patrolRange, 0.0f, 450.0f)) dirty = true;
                            if (ImGui::SliderFloat("巡回速度", &o->patrolSpeed, 0.0f, 0.1f, "%.3f")) dirty = true;
                        }
                    }
                    else {
                        ImGui::TextUnformatted("GPの接続判定は触手側で管理。各ステージ最大4個。");
                    }
                    ImGui::Separator();
                    ImGui::TextUnformatted("カメラとの連動");
                    const char* cameraChoices[] = {
                        "ワールド連動（位置と倍率）", "位置のみ連動（通常サイズ）", "画面固定（UI向け）"
                    };
                    if (o->kind == Kind::GripPoint || o->collisionType == 1 || o->collisionType == 2) {
                        o->cameraMode = 0; // 物理判定と表示がずれないようにする
                        ImGui::TextDisabled("GP・床・壁はワールド連動固定です");
                    }
                    else if (ImGui::Combo("カメラ方式", &o->cameraMode, cameraChoices, 3)) dirty = true;
                    ImGui::Separator();
                    ImGui::TextUnformatted("このオブジェクトだけのPNGリソース");
                    if (ImGui::InputText("テクスチャ名", o->textureName, sizeof(o->textureName))) dirty = true;
                    if (ImGui::Button("PNGを選択して割り当てる", ImVec2(-1, 0))) {
                        if (ChooseObjectTexture(*o)) { dirty = true; lastStatus = "PNGを割り当てました"; }
                        else lastStatus = "PNGの読込に失敗、またはキャンセルしました";
                    }
                    if (o->texturePath[0]) {
                        ImGui::TextWrapped("画像: %s", o->texturePath);
                        if (ImGui::Button("PNG割り当てを外す")) { o->texturePath[0] = 0; dirty = true; }
                    }
                    int slot = o->resourceSlot + 1;
                    const char* slots[AssetEditor::SlotCount + 1]{};
                    slots[0] = "なし（色つき四角）";
                    for (int i = 0; i < AssetEditor::SlotCount; ++i) slots[i + 1] = AssetEditor::names[i];
                    if (ImGui::Combo("画像リソース", &slot, slots, AssetEditor::SlotCount + 1)) {
                        o->resourceSlot = slot - 1;
                        dirty = true;
                    }
                    if (o->resourceSlot >= 0) {
                        if (ImGui::Button("このスロットにPNGを読み込む")) {
                            if (AssetEditor::ChooseFile((AssetEditor::Slot)o->resourceSlot)) dirty = true;
                        }
                    }
                    float rgb[3] = { o->r / 255.0f, o->g / 255.0f, o->b / 255.0f };
                    if (ImGui::ColorEdit3("四角の色", rgb)) {
                        o->r = (unsigned char)(rgb[0] * 255);
                        o->g = (unsigned char)(rgb[1] * 255);
                        o->b = (unsigned char)(rgb[2] * 255);
                        dirty = true;
                    }
                    ImGui::TextUnformatted("位置は画面左上を(0,0)とする2D表示座標です。");
                }
            }
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Prefab / ひな形", ImGuiTreeNodeFlags_DefaultOpen)) {
                static char prefabName[64] = "MyPrefab";
                ImGui::InputText("Prefab名", prefabName, sizeof(prefabName));
                if (ImGui::Button("選択中をPrefab登録")) RegisterPrefabFromSelected(prefabName);
                for (int i = 0; i < (int)prefabs.size(); ++i) {
                    ImGui::PushID(i + 100000);
                    ImGui::Text("%d : %s", i + 1, prefabs[i].name);
                    ImGui::SameLine();
                    if (ImGui::Button("配置")) InstantiatePrefab(i, stage);
                    ImGui::SameLine();
                    if (ImGui::Button("削除")) { prefabs.erase(prefabs.begin() + i); dirty = true; ImGui::PopID(); break; }
                    ImGui::PopID();
                }
                ImGui::TextUnformatted("Prefabはシーンデータと一緒に保存されます。");
            }
            ImGui::Separator();
            ImGui::TextWrapped("%s", lastStatus.c_str());
            ImGui::EndChild();
        } // 右Inspector
    }

#endif
}
