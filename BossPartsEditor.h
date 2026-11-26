#pragma once
// ボス1～4共通の軽量パーツ配置エディター。
// 画像を変更した時だけ読み込み、通常フレームではキャッシュ済みハンドルを使う。
// 物理・GP・ネジの座標は変更しない（表示用）。
#include <Novice.h>
#include "StageCollision.h"
#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <cstring>
#include <array>
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#pragma comment(lib, "Comdlg32.lib")

namespace BossPartsEditor {
    constexpr int kBossCount = 4;
    constexpr int kMaxParts = 24;
    constexpr float kPi = 3.14159265359f;

    struct Part {
        std::string name = "新しいパーツ";
        std::string path;
        float x = 0.0f; // ボス中心からの相対X。右が正。
        float y = 0.0f; // ボス中心からの相対Y。上が正。
        float scale = 1.0f;
        float angle = 0.0f; // 度
        int layer = 0;
        int link = -1;     // -1:常時 / 0..3:装甲 / 10..15:アクセサリー
        bool visible = true;
        int collisionType = 0; // 0=なし、1=固体、2=すり抜け床、3=接触ダメージ
        float colliderScale = 1.0f;
        int contactDamage = 5;
        int handle = -1;
        int width = 0;
        int height = 0;
    };
    struct Profile {
        bool enabled = false; // OFFは従来描画のまま
        std::vector<Part> parts;
    };
    inline std::array<Profile, kBossCount> profiles{};
    inline bool initialized = false;
    inline std::string settingsPath;
    inline std::string resourceRoot;
    inline int importCounter = 0;
    inline int selectedBoss = 0;
    inline int selectedPart = -1;
    inline char status[256] = "未保存";

    inline std::string ResolvePath(const std::string& p) {
        if (p.compare(0, 12, "./resources/") == 0 || p.compare(0, 12, ".\\resources\\") == 0)
            return resourceRoot + "\\" + p.substr(12);
        return p;
    }
    inline bool FileExists(const std::string& p) {
        const DWORD a = GetFileAttributesA(ResolvePath(p).c_str());
        return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
    }
    inline bool GetPNGSize(const std::string& p, int& w, int& h) {
        unsigned char b[24]{};
        FILE* f = nullptr;
        if (fopen_s(&f, ResolvePath(p).c_str(), "rb") != 0 || !f) return false;
        const size_t bytes = fread(b, 1, sizeof(b), f);
        fclose(f);
        if (bytes != sizeof(b) || std::memcmp(b, "\x89PNG\r\n\x1a\n", 8)) return false;
        w = (int)((unsigned(b[16]) << 24) | (unsigned(b[17]) << 16) | (unsigned(b[18]) << 8) | b[19]);
        h = (int)((unsigned(b[20]) << 24) | (unsigned(b[21]) << 16) | (unsigned(b[22]) << 8) | b[23]);
        return w > 0 && h > 0 && w <= 8192 && h <= 8192;
    }
    inline bool SetImage(Part& p, const std::string& path) {
        int w = 0, h = 0;
        if (!GetPNGSize(path, w, h)) {
            std::snprintf(status, sizeof(status), "PNGを読み込めません: %.180s", path.c_str());
            return false;
        }
        // Noviceのテクスチャハンドルは再利用。再読み込みはパスを変更した時のみ。
        const int handle = Novice::LoadTexture(ResolvePath(path).c_str());
        if (handle < 0) {
            std::snprintf(status, sizeof(status), "テクスチャの読み込みに失敗");
            return false;
        }
        p.path = path; p.width = w; p.height = h; p.handle = handle;
        std::snprintf(status, sizeof(status), "読み込み成功 (%d x %d)", w, h);
        return true;
    }
    inline bool ChoosePNG(Part& p) {
        char path[MAX_PATH]{};
        OPENFILENAMEA d{};
        d.lStructSize = sizeof(d);
        d.lpstrFilter = "PNG (*.png)\0*.png\0\0";
        d.lpstrFile = path;
        d.nMaxFile = MAX_PATH;
        d.lpstrTitle = "ボスのパーツPNGを選択";
        d.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameA(&d)) return false;
        // 元の場所からPNGをプロジェクト内へコピーし、再起動後も参照できるようにする。
        const std::string bossDir = resourceRoot + "\\texture\\boss";
        const std::string destDir = bossDir + "\\editor_parts";
        CreateDirectoryA(bossDir.c_str(), nullptr);
        CreateDirectoryA(destDir.c_str(), nullptr);
        ++importCounter;
        char name[128]{};
        std::snprintf(name, sizeof(name), "boss%d_part%02d_%04d.png", selectedBoss + 1, selectedPart + 1, importCounter);
        const std::string dest = destDir + "\\" + name;
        if (!CopyFileA(path, dest.c_str(), 0)) {
            std::snprintf(status, sizeof(status), "PNGのコピーに失敗しました");
            return false;
        }
        return SetImage(p, std::string("./resources/texture/boss/editor_parts/") + name);
    }
    inline bool Save() {
        if (settingsPath.empty()) return false;
        std::ofstream f(settingsPath, std::ios::binary | std::ios::trunc);
        if (!f) { std::snprintf(status, sizeof(status), "保存失敗: 書き込み不可"); return false; }
        f << "BOSS_PARTS_V1\n";
        f << std::setprecision(9);
        for (int i = 0; i < kBossCount; ++i) {
            const auto& pf = profiles[i];
            f << i << ' ' << int(pf.enabled) << ' ' << pf.parts.size() << '\n';
            for (const auto& p : pf.parts) {
                f << std::quoted(p.name) << ' ' << std::quoted(p.path) << ' '
                    << p.x << ' ' << p.y << ' ' << p.scale << ' ' << p.angle << ' '
                    << p.layer << ' ' << p.link << ' ' << int(p.visible) << ' ' << p.collisionType << ' ' << p.colliderScale << ' ' << p.contactDamage << '\n';
            }
        }
        const bool ok = bool(f);
        std::snprintf(status, sizeof(status), ok ? "パーツ配置を保存しました" : "保存失敗");
        return ok;
    }
    inline bool Load() {
        if (settingsPath.empty()) return false;
        std::ifstream f(settingsPath, std::ios::binary);
        if (!f) return false;
        std::string magic;
        if (!(f >> magic) || magic != "BOSS_PARTS_V1") return false;
        std::array<Profile, kBossCount> loaded{};
        for (int i = 0; i < kBossCount; ++i) {
            int index = -1, en = 0, count = 0;
            if (!(f >> index >> en >> count) || index != i || count < 0 || count > kMaxParts) return false;
            loaded[i].enabled = (en != 0);
            for (int j = 0; j < count; ++j) {
                Part p;
                int vis = 0;
                if (!(f >> std::quoted(p.name) >> std::quoted(p.path) >>
                    p.x >> p.y >> p.scale >> p.angle >> p.layer >> p.link >> vis)) return false;
                p.visible = vis != 0;
                // 旧BOSS_PARTS_V1は追加パラメーター無しでも読み込める。
                std::string remains;
                std::getline(f, remains);
                if (!remains.empty()) {
                    std::istringstream extra(remains);
                    if (!(extra >> p.collisionType >> p.colliderScale >> p.contactDamage)) {
                        p.collisionType = 0; p.colliderScale = 1.f; p.contactDamage = 5;
                    }
                }
                if (p.collisionType < 0 || p.collisionType>3 ||
                    !std::isfinite(p.colliderScale) || p.colliderScale < 0.05f || p.colliderScale>2.0f ||
                    p.contactDamage < 0 || p.contactDamage>100) return false;
                if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
                    !std::isfinite(p.scale) || !std::isfinite(p.angle)) return false;
                p.scale = (std::max)(0.05f, (std::min)(p.scale, 10.0f));
                p.x = (std::max)(-5000.f, (std::min)(p.x, 5000.f));
                p.y = (std::max)(-5000.f, (std::min)(p.y, 5000.f));
                p.layer = (std::max)(-100, (std::min)(p.layer, 100));
                // 画像が移動・削除されていても設定の読み込み自体は続行する。
                if (!p.path.empty() && FileExists(p.path)) SetImage(p, p.path);
                loaded[i].parts.push_back(std::move(p));
            }
        }
        profiles = std::move(loaded);
        std::snprintf(status, sizeof(status), "パーツ配置を再読込しました");
        return true;
    }
    inline void Initialize(const std::string& path) {
        if (initialized) return;
        settingsPath = path;
        const size_t slash = path.find_last_of("\\/");
        resourceRoot = slash == std::string::npos ? "." : path.substr(0, slash);
        initialized = true;
        Load();
    }
    inline bool Active(int form) {
        if (!initialized || form < 0 || form >= kBossCount || !profiles[form].enabled) return false;
        for (const auto& p : profiles[form].parts)
            if (p.visible && p.handle >= 0) return true;
        return false;
    }
    // テクスチャ四隅を回転させる。鏡像でも描画位置はずれない。
    inline void DrawPart(const Part& p, float baseX, float baseY, float zoom, bool flipX) {
        if (!p.visible || p.handle < 0 || p.width <= 0 || p.height <= 0) return;
        const float x = baseX + (flipX ? -p.x : p.x) * zoom;
        const float y = baseY - p.y * zoom;
        const float theta = -p.angle * (kPi / 180.0f) * (flipX ? -1.0f : 1.0f);
        const float c = std::cos(theta), s = std::sin(theta);
        const float hw = p.width * p.scale * zoom * 0.5f;
        const float hh = p.height * p.scale * zoom * 0.5f;
        const float sx = flipX ? -1.f : 1.f;
        auto corner = [&](float u, float v) {
            const float px = u * sx * hw, py = v * hh;
            return std::pair<int, int>(int(std::lround(x + c * px - s * py)),
                int(std::lround(y + s * px + c * py)));
            };
        const auto a = corner(-1.f, -1.f), b = corner(1.f, -1.f);
        const auto c2 = corner(-1.f, 1.f), d = corner(1.f, 1.f);
        Novice::DrawQuad(a.first, a.second, b.first, b.second, c2.first, c2.second, d.first, d.second,
            0, 0, p.width, p.height, p.handle, 0xFFFFFFFF);
    }
    // 可視状態を呼び出し元が返す。装甲・付属品の破壊状態と表示を一致させる。
    template<class IsLinkedVisible>
    inline void DrawWorld(int form, float screenX, float screenY, float cameraZoom,
        bool flipX, IsLinkedVisible isVisible) {
        if (!Active(form)) return;
        const auto& parts = profiles[form].parts;
        // 小数十個だけの描画順ソート。毎フレームのヒープ確保なし。
        std::array<int, kMaxParts> order{};
        const int count = int((std::min)(parts.size(), size_t(kMaxParts)));
        for (int i = 0; i < count; ++i) order[i] = i;
        // ボスパーツを描画順で並べ替える
        std::stable_sort(
            order.begin(),
            order.begin() + count,
            [&parts](int a, int b) {
                return parts[a].layer < parts[b].layer;
            }
        );
        for (int i = 0;i < count;++i) {
            const Part& p = parts[order[i]];
            if (p.link >= 0 && !isVisible(p.link)) continue;
            DrawPart(p, screenX, screenY, cameraZoom, flipX);
        }
    }

    // 破壊済みパーツは呼び出し側のisLinkedVisibleで判定ごと消える。
    template<class IsLinkedVisible>
    inline int CollectColliders(int form, Vector2 bossPosition,
        StageCollision::Rect* output, int capacity, IsLinkedVisible isLinkedVisible) {
        if (!output || capacity <= 0 || !Active(form)) return 0;
        int n = 0;
        for (const auto& p : profiles[form].parts) {
            if (!p.visible || p.handle < 0 || p.collisionType == 0 ||
                (p.link >= 0 && !isLinkedVisible(p.link))) continue;
            if (n >= capacity) break;
            const float halfW = p.width * p.scale * p.colliderScale * .5f;
            const float halfH = p.height * p.scale * p.colliderScale * .5f;
            const float cx = bossPosition.x + p.x, cy = bossPosition.y + p.y;
            StageCollision::Rect r;
            r.left = cx - halfW; r.right = cx + halfW;
            r.bottom = cy - halfH; r.top = cy + halfH;
            r.type = static_cast<StageCollision::Type>(p.collisionType);
            r.damage = p.contactDamage;
            output[n++] = r;
        }
        return n;
    }
#ifdef USE_IMGUI
    inline void DrawPanel() {
        const char* forms[] = { "ボス1", "ボス2", "ボス3（準備用）", "ボス4（準備用）" };
        ImGui::Combo("編集するボス", &selectedBoss, forms, kBossCount);
        Profile& pf = profiles[selectedBoss];
        ImGui::Checkbox("ゲーム描画をパーツ構成に置き換える", &pf.enabled);
        if (selectedBoss >= 2)
            ImGui::TextWrapped("ボス3・4は配置の保存と編集のみ。戦闘ステージへの接続は今後追加します。");
        if (selectedBoss == 0 && ImGui::Button("第1ボスの既存パーツを読み込む")) {
            // 現在のゲーム内で使われているPNGと配置を初期配置として登録する。
            struct Seed { const char* name; const char* path; float l, t; int w, h; int link; int layer; };
            const Seed seeds[] = {
                {"ジェット1","./resources/texture/boss/form1/boss_jet1.png",45,243,256,256,10,-5},
                {"ジェット2","./resources/texture/boss/form1/boss_jet2.png",115,247,256,256,11,-4},
                {"ジェット3","./resources/texture/boss/form1/boss_jet3.png",116,236,256,256,12,-3},
                {"ジェット4","./resources/texture/boss/form1/boss_jet4.png",304,216,256,256,13,-2},
                {"パイプ","./resources/texture/boss/form1/boss_pipes.png",208,99,256,256,14,-1},
                {"プロペラ1","./resources/texture/boss/form1/boss_propeller1.png",231,107,256,256,1,1},
                {"プロペラ2","./resources/texture/boss/form1/boss_propeller2.png",91,75,256,256,2,2},
                {"プロペラ3","./resources/texture/boss/form1/boss_propeller3.png",-47,108,256,256,3,3},
                {"本体","./resources/texture/boss/form1/boss_body.png",79,111,256,256,-1,4},
                {"目","./resources/texture/boss/form1/boss_eye.png",82,204,128,128,-1,5}
            };
            pf.parts.clear();
            for (const Seed& a : seeds) {
                Part p; p.name = a.name; p.x = a.l + a.w * .5f - 256.f;
                p.y = 256.f - a.t - a.h * .5f; p.link = a.link; p.layer = a.layer;
                if (FileExists(a.path)) SetImage(p, a.path);
                pf.parts.push_back(std::move(p));
            }
            selectedPart = 0;
        }
        if (selectedBoss == 1 && ImGui::Button("第2ボスの本体を読み込む")) {
            Part p; p.name = "戦闘機本体";
            const char* path = "./resources/texture/boss/form2/boss_body.png";
            if (FileExists(path)) SetImage(p, path);
            pf.parts.clear(); pf.parts.push_back(std::move(p)); selectedPart = 0;
        }
        if (selectedBoss == 2 && ImGui::Button("第3ボスの既存素材を登録")) {
            pf.parts.clear();
            const char* names[] = { "雲の本体","目" };
            const char* paths[] = { "./resources/texture/boss/form3/boss_cloud.png",
                                 "./resources/texture/boss/form3/boss_eye.png" };
            for (int i = 0;i < 2;++i) {
                Part p; p.name = names[i];p.layer = i;
                if (FileExists(paths[i])) SetImage(p, paths[i]);
                pf.parts.push_back(std::move(p));
            }
            selectedPart = 0;
        }
        if (ImGui::Button("パーツを追加") && pf.parts.size() < kMaxParts) {
            pf.parts.emplace_back(); selectedPart = int(pf.parts.size()) - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("配置を保存")) Save();
        ImGui::SameLine();
        if (ImGui::Button("再読込")) { Load(); selectedPart = -1; }
        ImGui::TextWrapped("%s", status);
        ImGui::Separator();
        ImGui::BeginChild("##BossPartList", ImVec2(190, 370), true);
        for (int i = 0;i<int(pf.parts.size());++i) {
            ImGui::PushID(i);
            std::string label = pf.parts[i].name + "##part";
            if (ImGui::Selectable(label.c_str(), i == selectedPart)) selectedPart = i;
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (selectedPart >= 0 && selectedPart<int(pf.parts.size())) {
            Part& p = pf.parts[selectedPart];
            char name[128]{}; strncpy_s(name, sizeof(name), p.name.c_str(), _TRUNCATE);
            if (ImGui::InputText("名前", name, sizeof(name))) p.name = name;
            if (ImGui::Button("PNGを読み込む")) ChoosePNG(p);
            ImGui::TextWrapped("PNG: %s", p.path.empty() ? "未選択" : p.path.c_str());
            ImGui::Checkbox("表示", &p.visible);
            const char* colliderNames[] = { "なし", "床・壁", "すり抜け床", "接触ダメージ" };
            ImGui::Combo("物理・接触判定", &p.collisionType, colliderNames, 4);
            if (p.collisionType != 0) {
                ImGui::SliderFloat("当たり判定の倍率", &p.colliderScale, 0.05f, 2.f);
                if (p.collisionType == 3) ImGui::SliderInt("触れた時のダメージ", &p.contactDamage, 0, 100);
                ImGui::TextUnformatted("破壊連動パーツは消滅時に判定もOFF。");
            }
            ImGui::DragFloat("X（右が＋）", &p.x, 1.f, -2000.f, 2000.f);
            ImGui::DragFloat("Y（上が＋）", &p.y, 1.f, -2000.f, 2000.f);
            ImGui::DragFloat("倍率", &p.scale, 0.01f, 0.05f, 10.f);
            ImGui::DragFloat("回転（度）", &p.angle, 1.f, -360.f, 360.f);
            ImGui::DragInt("描画順", &p.layer, 1.f, -100, 100);
            const char* linkLabels[] = { "常時表示","装甲0","装甲1","装甲2","装甲3",
                                      "付属1","付属2","付属3","付属4","付属5","付属6" };
            int linkIndex = (p.link < 0) ? 0 : (p.link < 4 ? p.link + 1 : (p.link >= 10 && p.link <= 15 ? p.link - 5 : 0));
            if (ImGui::Combo("破壊状態と連動", &linkIndex, linkLabels, 11))
                p.link = (linkIndex == 0 ? -1 : (linkIndex <= 4 ? linkIndex - 1 : linkIndex + 5));
            if (ImGui::Button("このパーツを削除")) {
                pf.parts.erase(pf.parts.begin() + selectedPart);selectedPart = -1;
            }
        }
        else {
            ImGui::TextUnformatted("左の一覧でパーツを選択してください。");
        }
        ImGui::EndGroup();
        ImGui::Separator();
        ImGui::TextUnformatted("配置プレビュー（左クリックで選択パーツを移動）");
        const ImVec2 top = ImGui::GetCursorScreenPos();
        const ImVec2 size(400, 260);
        ImGui::InvisibleButton("##BossPartPreview", size);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(top, ImVec2(top.x + size.x, top.y + size.y), IM_COL32(35, 49, 64, 255));
        draw->AddLine(ImVec2(top.x + size.x / 2, top.y), ImVec2(top.x + size.x / 2, top.y + size.y), IM_COL32(110, 125, 145, 150));
        draw->AddLine(ImVec2(top.x, top.y + size.y / 2), ImVec2(top.x + size.x, top.y + size.y / 2), IM_COL32(110, 125, 145, 150));
        constexpr float previewScale = .45f;
        for (int i = 0;i<int(pf.parts.size());++i) {
            const auto& p = pf.parts[i];
            const ImVec2 pt(top.x + size.x * .5f + p.x * previewScale, top.y + size.y * .5f - p.y * previewScale);
            draw->AddCircleFilled(pt, i == selectedPart ? 7.f : 5.f, i == selectedPart ? IM_COL32(255, 207, 96, 255) : IM_COL32(112, 207, 255, 255));
            draw->AddText(ImVec2(pt.x + 8, pt.y - 7), IM_COL32(235, 235, 235, 255), p.name.c_str());
        }
        if (selectedPart >= 0 && selectedPart<int(pf.parts.size()) && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 pos = ImGui::GetIO().MousePos;
            auto& p = pf.parts[selectedPart];
            p.x = (pos.x - top.x - size.x * .5f) / previewScale;
            p.y = -(pos.y - top.y - size.y * .5f) / previewScale;
        }
        ImGui::TextUnformatted("実際のPNGの重なりはゲーム画面で確認。編集内容は保存ボタンで確定します。");
    }
#endif
} // namespace BossPartsEditor
