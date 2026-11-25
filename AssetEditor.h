#pragma once
// Game asset swap + sprite sheet preview (header only; C++20)
// No player physics or collision behavior is modified here.
#include <Novice.h>
#include <Windows.h>
#include <commdlg.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdlib>
#pragma comment(lib, "Comdlg32.lib")
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace AssetEditor {
    enum Slot {
        Background1, Background2, PlayerIdle, PlayerWalk, PlayerJump, PlayerFall,
        Tentacle, Gate, BossBody, BossForm2, MapChip, SlotCount
    };
    struct Data {
        char path[512]{};
        int handle = -1;
        int width = 0, height = 0;
        int cellW = 32, cellH = 32;
        int fps = 8, columns = 1, rows = 1;
        int frame = 0;
        bool play = false, loop = true;
        float clock = 0;
    };
    inline Data assets[SlotCount]{};
    inline const char* names[SlotCount] = {
        "背景：ステージ1", "背景：ステージ2", "プレイヤー：待機", "プレイヤー：歩行",
        "プレイヤー：ジャンプ", "プレイヤー：落下", "触手", "ゲート",
        "ボス：第1形態", "ボス：第2形態", "マップチップ"
    };
    inline const char* defaults[SlotCount] = {
        "", "", "./resources/texture/player/video_sample/playerIdle.png",
        "./resources/texture/player/video_sample/player_walk.png",
        "./resources/texture/player/video_sample/player_jump.png",
        "./resources/texture/player/video_sample/player_fall.png",
        "./resources/texture/player/video_sample/tentacle.png",
        "./resources/texture/player/video_sample/gate.png",
        "./resources/texture/boss/form1/boss_body.png",
        "./resources/texture/boss/form2/boss_body.png",
        "./resources/texture/stage/mapchip.png"
    };
    inline bool opened = false; // 統合画面の画像タブでのみプレビュー
    inline int selected = PlayerIdle;
    inline int sequence = 1;
    inline char status[256] = "F6: Asset Editor";
    inline const char* config = "./resources/asset_editor_settings.txt"; // 旧設定ファイル名（互換用）

    // PNG選択ダイアログの影響を受けないよう、起動時のリソースフォルダを固定する。
    // Visual Studioの実行フォルダ／exeのフォルダのいずれから起動しても、
    // resources/texture が見つかる最も近い階層を採用する。
    inline std::string resourceRoot;
    inline std::string settingsFile;

    inline bool IsDirectory(const std::string& path) {
        const DWORD attrs = GetFileAttributesA(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    inline std::string FullPath(const std::string& path) {
        char full[4096]{};
        const DWORD n = GetFullPathNameA(path.c_str(), (DWORD)sizeof(full), full, nullptr);
        if (n == 0 || n >= sizeof(full)) return path;
        return full;
    }
    inline void FixSettingsLocation() {
        if (!settingsFile.empty()) return;
        char current[MAX_PATH]{};
        GetCurrentDirectoryA(MAX_PATH, current);
        char executable[MAX_PATH]{};
        GetModuleFileNameA(nullptr, executable, MAX_PATH);
        std::string exeDir = executable;
        const size_t slash = exeDir.find_last_of("\\/");
        if (slash != std::string::npos) exeDir.resize(slash);

        const std::string starts[2] = { current, exeDir };
        for (const std::string& start : starts) {
            std::string parent = start;
            for (int depth = 0; depth <= 5; ++depth) {
                const std::string candidate = FullPath(parent + "\\resources");
                if (IsDirectory(candidate + "\\texture")) {
                    resourceRoot = candidate;
                    settingsFile = resourceRoot + "\\asset_editor_settings.txt";
                    return;
                }
                parent = FullPath(parent + "\\..");
            }
        }
        // textureディレクトリが存在しない場合も、安全な固定先を確保する。
        resourceRoot = FullPath(std::string(current) + "\\resources");
        settingsFile = resourceRoot + "\\asset_editor_settings.txt";
    }
    inline std::string BossSettingsPath() {
        FixSettingsLocation();
        return FullPath(resourceRoot + "\\..\\bossbattle_stage1.txt");
    }

    inline std::string ResolveTexturePath(const char* requested) {
        FixSettingsLocation();
        if (!requested) return {};
        std::string p = requested;
        // すでに有効なパスなら以前のロード方式をそのまま使う。
        if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
        for (char& ch : p) if (ch == '/') ch = '\\';
        if (p.compare(0, 2, ".\\") == 0) p.erase(0, 2);
        if (p.compare(0, 10, "resources\\") == 0) {
            return resourceRoot + "\\" + p.substr(10);
        }
        return p;
    }

    inline int ReadPNGSize(const char* path, int& w, int& h) {
        unsigned char b[24]{};
        FILE* fp = nullptr;
        if (fopen_s(&fp, path, "rb") != 0 || !fp) return 0;
        const size_t bytes = fread(b, 1, sizeof(b), fp);
        fclose(fp);
        if (bytes != sizeof(b) || std::memcmp(b, "\x89PNG\r\n\x1a\n", 8) != 0) return 0;
        w = (int)((unsigned(b[16]) << 24) | (unsigned(b[17]) << 16) | (unsigned(b[18]) << 8) | unsigned(b[19]));
        h = (int)((unsigned(b[20]) << 24) | (unsigned(b[21]) << 16) | (unsigned(b[22]) << 8) | unsigned(b[23]));
        return w > 0 && h > 0 && w < 16384 && h < 16384;
    }
    inline bool Apply(Slot slot, const char* path, int w = 0, int h = 0) {
        if (!path || !*path) return false;
        int realW = 0, realH = 0;
        const std::string resolved = ResolveTexturePath(path);
        if (!ReadPNGSize(resolved.c_str(), realW, realH)) {
            std::snprintf(status, sizeof(status), "PNG missing: %.210s", path);
            return false;
        }
        const int tex = Novice::LoadTexture(resolved.c_str());
        if (tex < 0) { std::snprintf(status, sizeof(status), "Novice LoadTexture failed"); return false; }
        Data& a = assets[slot];
        std::snprintf(a.path, sizeof(a.path), "%s", path);
        a.handle = tex; a.width = realW; a.height = realH;
        a.cellW = w > 0 ? w : realW;
        a.cellH = h > 0 ? h : realH;
        a.cellW = std::clamp(a.cellW, 1, a.width);
        a.cellH = std::clamp(a.cellH, 1, a.height);
        a.columns = (std::max)(1, a.width / a.cellW);
        a.rows = (std::max)(1, a.height / a.cellH);
        a.frame = 0; a.clock = 0;
        std::snprintf(status, sizeof(status), "Loaded: %s", path);
        return true;
    }
    inline int Handle(Slot s) { return assets[s].handle; }
    // PNG差し替え後のセルサイズをゲーム描画でも利用する。
    // 物理座標やプレイヤーの移動処理には触れない。
    struct FrameSource { int x, y, w, h; };
    inline FrameSource GetFrame(Slot slot, int gameplayFrame) {
        const Data& a = assets[slot];
        const int cellW = (std::max)(1, a.cellW);
        const int cellH = (std::max)(1, a.cellH);
        const int cols = (std::max)(1, a.columns);
        const int total = (std::max)(1, a.columns * a.rows);
        int frame = (a.play ? a.frame : gameplayFrame);
        frame = (std::max)(0, frame) % total;
        return { (frame % cols) * cellW, (frame / cols) * cellH, cellW, cellH };
    }
    inline Slot PlayerSlotForHandle(int handle) {
        const Slot playerSlots[] = { PlayerIdle, PlayerWalk, PlayerJump, PlayerFall };
        for (Slot s : playerSlots) if (handle >= 0 && handle == assets[s].handle) return s;
        return PlayerIdle;
    }
    inline bool Save() {
        FixSettingsLocation();
        if (!IsDirectory(resourceRoot)) CreateDirectoryA(resourceRoot.c_str(), nullptr);
        FILE* f = nullptr;
        if (fopen_s(&f, settingsFile.c_str(), "wb") != 0 || !f) {
            std::snprintf(status, sizeof(status), "Save FAILED: %.210s", settingsFile.c_str());
            return false;
        }
        // V2: 5個の数値とパスをタブで区切るため、パスに空白があっても読める。
        fprintf(f, "AssetEditorV2\n");
        for (int i = 0; i < SlotCount; ++i) {
            const Data& a = assets[i];
            fprintf(f, "%d\t%d\t%d\t%d\t%d\t%s\n",
                i, a.cellW, a.cellH, a.fps, a.loop ? 1 : 0,
                a.path[0] ? a.path : "-");
        }
        const bool writeFailed = ferror(f) != 0;
        const int closeResult = fclose(f);
        if (writeFailed || closeResult != 0) {
            std::snprintf(status, sizeof(status), "Save FAILED: %.210s", settingsFile.c_str());
        }
        else {
            std::snprintf(status, sizeof(status), "Save OK: %.220s", settingsFile.c_str());
        }
        return !writeFailed && closeResult == 0;
    }
    inline void Initialize() {
        FixSettingsLocation();
        for (int i = 0; i < SlotCount; ++i) {
            assets[i] = {};
            if (defaults[i][0]) {
                const bool isPlayerStrip = i == PlayerWalk || i == PlayerJump || i == PlayerFall;
                if (isPlayerStrip) Apply((Slot)i, defaults[i], 32, 64);
                else Apply((Slot)i, defaults[i]);
            }
        }
        FILE* f = nullptr;
        if (fopen_s(&f, settingsFile.c_str(), "rb") != 0 || !f) {
            std::snprintf(status, sizeof(status), "No saved settings: %.190s", settingsFile.c_str());
            return;
        }
        char line[2048]{};
        if (!fgets(line, sizeof(line), f) ||
            (strncmp(line, "AssetEditorV1", 13) != 0 && strncmp(line, "AssetEditorV2", 13) != 0)) {
            fclose(f);
            std::snprintf(status, sizeof(status), "Invalid settings: %.190s", settingsFile.c_str());
            return;
        }
        int restored = 0;
        int failed = 0;
        while (fgets(line, sizeof(line), f)) {
            char* p = line;
            long values[5]{};
            bool parsed = true;
            for (int k = 0; k < 5; ++k) {
                while (*p == ' ' || *p == '\t') ++p;
                char* end = nullptr;
                values[k] = std::strtol(p, &end, 10);
                if (end == p) { parsed = false; break; }
                p = end;
            }
            if (!parsed) { ++failed; continue; }
            while (*p == ' ' || *p == '\t') ++p;
            size_t len = std::strlen(p);
            while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r')) p[--len] = '\0';
            const int i = (int)values[0];
            if (i < 0 || i >= SlotCount) { ++failed; continue; }
            if (*p != '\0' && std::strcmp(p, "-") != 0) {
                if (Apply((Slot)i, p, (int)values[1], (int)values[2])) {
                    assets[i].fps = std::clamp((int)values[3], 1, 60);
                    assets[i].loop = values[4] != 0;
                    ++restored;
                }
                else ++failed;
            }
        }
        fclose(f);
        std::snprintf(status, sizeof(status), "Restored %d slot(s), failed %d / %.175s",
            restored, failed, settingsFile.c_str());
    }
    inline bool ChooseFile(Slot s) {
        FixSettingsLocation();
        char file[MAX_PATH]{};
        OPENFILENAMEA ofn{}; ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "PNG image (*.png)\0*.png\0\0";
        ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
        // これがないとダイアログ操作後にカレントディレクトリが変わってしまう。
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        ofn.lpstrTitle = "Select PNG resource";
        if (!GetOpenFileNameA(&ofn)) return false;
        int w = 0, h = 0;
        if (!ReadPNGSize(file, w, h)) {
            std::snprintf(status, sizeof(status), "Only valid PNG is supported");
            return false;
        }
        const std::string textureDir = resourceRoot + "\\texture";
        const std::string localDir = textureDir + "\\editor_assets";
        if (!IsDirectory(resourceRoot)) CreateDirectoryA(resourceRoot.c_str(), nullptr);
        if (!IsDirectory(textureDir)) CreateDirectoryA(textureDir.c_str(), nullptr);
        if (!IsDirectory(localDir)) CreateDirectoryA(localDir.c_str(), nullptr);
        char localName[100]{};
        std::string absolutePath;
        do {
            std::snprintf(localName, sizeof(localName), "slot_%02d_%06d.png", (int)s, sequence++);
            absolutePath = localDir + "\\" + localName;
        } while (GetFileAttributesA(absolutePath.c_str()) != INVALID_FILE_ATTRIBUTES);
        if (!CopyFileA(file, absolutePath.c_str(), FALSE)) {
            std::snprintf(status, sizeof(status), "PNG copy FAILED: %.170s", absolutePath.c_str());
            return false;
        }
        const std::string relative = std::string("./resources/texture/editor_assets/") + localName;
        return Apply(s, relative.c_str());
    }
    inline void Update(float dt) {
        for (int i = 0;i < SlotCount;++i) {
            Data& a = assets[i];
            if (!a.play || a.handle < 0) continue;
            a.clock += std::clamp(dt, 0.0f, 0.1f);
            const float seconds = 1.0f / float((std::max)(1, a.fps));
            const int total = (std::max)(1, a.columns * a.rows);
            while (a.clock >= seconds) {
                a.clock -= seconds;
                if (a.frame + 1 < total) ++a.frame;
                else if (a.loop) a.frame = 0;
                else { a.play = false; break; }
            }
        }
    }
    inline bool DrawBackground(Slot slot) {
        const Data& a = assets[slot];
        if (a.handle < 0 || a.width <= 0 || a.height <= 0) return false;
        Novice::DrawQuad(0, 0, 1280, 0, 0, 720, 1280, 720,
            0, 0, a.width, a.height, a.handle, WHITE);
        return true;
    }
    inline void DrawPreview() {
        if (!opened) return;
        const Data& a = assets[selected];
        if (a.handle < 0) return;
        const int sx = (a.frame % (std::max)(1, a.columns)) * a.cellW;
        const int sy = (a.frame / (std::max)(1, a.columns)) * a.cellH;
        const float scale = (std::min)(4.0f, (std::min)(360.0f / float(a.cellW), 290.0f / float(a.cellH)));
        const int dw = (std::max)(1, int(a.cellW * scale));
        const int dh = (std::max)(1, int(a.cellH * scale));
        const int x = 1050 - dw / 2, y = 350 - dh / 2;
        Novice::DrawBox(850, 175, 400, 350, 0, 0x0E1422E8, kFillModeSolid);
        Novice::DrawQuad(x, y, x + dw, y, x, y + dh, x + dw, y + dh,
            sx, sy, a.cellW, a.cellH, a.handle, WHITE);
        Novice::ScreenPrintf(858, 187, "SPRITE PREVIEW  %d / %d", a.frame + 1, (std::max)(1, a.columns * a.rows));
    }
    // 統合ImGuiの「画像/アニメ」タブから呼ばれる。別ウィンドウは作らない。
    inline void DrawPanel() {
#ifdef USE_IMGUI
        ImGui::TextUnformatted("画像を選択するとゲームの描画へ即時反映します。");
        ImGui::TextUnformatted("変更を次回起動に残すには、上部の「まとめて保存」を押してください。");
        ImGui::Separator();
        ImGui::BeginChild("##AssetSlotList", ImVec2(0, 168), true);
        for (int i = 0; i < SlotCount; ++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(names[i], selected == i)) selected = i;
            ImGui::PopID();
        }
        ImGui::EndChild();
        Data& a = assets[selected];
        ImGui::Text("%s", names[selected]);
        ImGui::TextWrapped("画像: %s", a.path[0] ? a.path : "（未設定）");
        if (ImGui::Button("PNGを選択・差し替え")) ChooseFile((Slot)selected);
        ImGui::SameLine();
        if (ImGui::Button("元画像に戻す") && defaults[selected][0]) Apply((Slot)selected, defaults[selected]);
        ImGui::Text("画像サイズ: %d x %d px", a.width, a.height);
        if (a.handle >= 0 && a.width > 0 && a.height > 0) {
            int cw = a.cellW, ch = a.cellH;
            bool changed = false;
            changed |= ImGui::InputInt("1コマ横幅(px)", &cw, 1, 16);
            changed |= ImGui::InputInt("1コマ高さ(px)", &ch, 1, 16);
            if (changed) {
                a.cellW = std::clamp(cw, 1, a.width);
                a.cellH = std::clamp(ch, 1, a.height);
                a.columns = (std::max)(1, a.width / a.cellW);
                a.rows = (std::max)(1, a.height / a.cellH);
                a.frame = (std::min)(a.frame, a.columns * a.rows - 1);
            }
            ImGui::SliderInt("FPS", &a.fps, 1, 60);
            ImGui::Checkbox("ループ再生", &a.loop);
            if (ImGui::Button(a.play ? "一時停止" : "アニメ再生")) a.play = !a.play;
            ImGui::SameLine();
            if (ImGui::Button("先頭に戻る")) { a.play = false; a.frame = 0; a.clock = 0; }
            ImGui::SameLine();
            if (ImGui::Button("1コマ進む")) a.frame = (a.frame + 1) % (std::max)(1, a.columns * a.rows);
            int total = (std::max)(1, a.columns * a.rows);
            ImGui::SliderInt("プレビューコマ", &a.frame, 0, total - 1);
            ImGui::Text("横%d / 縦%d / 合計%dコマ", a.columns, a.rows, total);
        }
        ImGui::Separator();
        ImGui::TextWrapped("状態: %s", status);
        ImGui::TextWrapped("保存先: %s", settingsFile.c_str());
        ImGui::TextWrapped("注意: 触手・ゲートは元画像と同じ構成を推奨。プレイヤーは1枚絵にも対応。");
#endif
    }
}
