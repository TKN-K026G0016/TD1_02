#pragma once
// ステージ別HUDと勝敗ルール。Noviceの文字/矩形描画だけで軽量に動作する。
#include <cstdio>
#include <algorithm>
#include <string>
#include <Novice.h>
#include <cmath>
#include <cstdint>
#include "Camera.h"
#include "CameraBackgroundTuning.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace GameHudEditor {
    enum Rule { Hp = 0, Time = 1, HpAndTime = 2 };
    struct TextItem {
        bool enabled = false;
        char text[64] = "";
        int x = 450, y = 80;
        bool worldAttached = false; // 画面固定=false／カメラ追従=true
        float baseScale = 1.0f; // カメラ倍率1.0時の文字の大きさ
    };
    struct Settings {
        int rule = Hp;
        int limitSeconds = 180;
        bool showPlayerHp = true;
        bool showBossHp = true;
        bool showArmor = true;
        bool showTimer = true;
        int playerX = 18, playerY = 92;
        int bossX = 970, bossY = 92;
        int timerX = 565, timerY = 35;
        int barWidth = 284;
        float playerScale = 1.0f;
        float bossScale = 1.0f;
        float timerScale = 1.0f;
        bool playerWorldAttached = false;
        bool bossWorldAttached = false;
        bool timerWorldAttached = false;
        TextItem labels[4]{};
    };
    inline Settings stages[4]{};
    inline int currentStage = 0;
    inline int remainingFrames = 180 * 60;
    inline bool expired = false;
    inline bool preview = false;
    inline const char* status = "未保存";
    inline int Clamp(int x, int lo, int hi) { return (std::max)(lo, (std::min)(hi, x)); }
    inline void Validate(Settings& s) {
        s.rule = Clamp(s.rule, 0, 2);
        s.limitSeconds = Clamp(s.limitSeconds, 1, 3600);
        s.barWidth = Clamp(s.barWidth, 80, 400);
        auto validScale = [](float& n) { if (!std::isfinite(n)) n = 1.0f; n = (std::max)(0.5f, (std::min)(3.0f, n)); };
        validScale(s.playerScale); validScale(s.bossScale); validScale(s.timerScale);
        s.playerX = Clamp(s.playerX, 0, 1200);
        s.bossX = Clamp(s.bossX, 0, 1200);
        s.timerX = Clamp(s.timerX, 0, 1200);
        s.playerY = Clamp(s.playerY, 0, 675);
        s.bossY = Clamp(s.bossY, 0, 675);
        s.timerY = Clamp(s.timerY, 0, 675);
        for (auto& label : s.labels) {
            label.x = Clamp(label.x, 0, 1200);
            label.y = Clamp(label.y, 0, 675);
            label.text[63] = '\0';
            validScale(label.baseScale);
        }
    }
    inline Settings& Current() { return stages[currentStage]; }
    inline bool UsesHp() { return Current().rule != Time; }
    inline bool UsesTime() { return Current().rule != Hp; }
    inline void SelectAndReset(int stageNumber) {
        currentStage = Clamp(stageNumber - 1, 0, 3);
        Validate(Current());
        remainingFrames = Current().limitSeconds * 60;
        expired = false;
    }
    inline void ResetTimer() {
        Validate(Current());
        remainingFrames = Current().limitSeconds * 60;
        expired = false;
    }
    // 60fps想定。クリア演出中・敗北時は止める。
    inline void Tick(bool active) {
        if (active && UsesTime() && !expired) {
            if (remainingFrames > 0) --remainingFrames;
            if (remainingFrames <= 0) { remainingFrames = 0; expired = true; }
        }
    }
    inline int SecondsLeft() { return (remainingFrames + 59) / 60; }
    inline bool IsTimeUp() { return UsesTime() && expired; }
    // スペースや日本語を含む文字列も、設定ファイル内では16進文字列で安全に保存。
    inline std::string EncodeText(const char* value) {
        const char* hex = "0123456789ABCDEF";
        std::string result;
        for (int i = 0; i < 63 && value[i]; ++i) {
            const unsigned char c = (unsigned char)value[i];
            result += hex[c >> 4];
            result += hex[c & 15];
        }
        return result.empty() ? "-" : result;
    }
    inline bool DecodeText(const char* hex, char* out) {
        out[0] = 0;
        if (hex[0] == '-' && hex[1] == 0) return true;
        const std::string h = hex;
        if (h.size() % 2 != 0 || h.size() > 126) return false;
        auto digit = [](char c)->int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
            };
        for (size_t i = 0; i < h.size(); i += 2) {
            const int a = digit(h[i]), b = digit(h[i + 1]);
            if (a < 0 || b < 0) return false;
            out[i / 2] = (char)(a * 16 + b);
        }
        out[h.size() / 2] = 0;
        return true;
    }
    inline bool Save(const char* path) {
        FILE* fp = nullptr;
        if (fopen_s(&fp, path, "w") != 0 || !fp) { status = "HUD設定の保存に失敗"; return false; }
        std::fprintf(fp, "GAME_HUD_V4\n");
        for (int i = 0; i < 4; ++i) {
            Settings s = stages[i]; Validate(s);
            std::fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d\n",
                s.rule, s.limitSeconds, int(s.showPlayerHp), int(s.showBossHp),
                int(s.showArmor), int(s.showTimer), s.playerX, s.playerY,
                s.bossX, s.bossY, s.timerX, s.timerY, s.barWidth);
            std::fprintf(fp, "%d %d %d\n", int(s.playerWorldAttached), int(s.bossWorldAttached), int(s.timerWorldAttached));
            std::fprintf(fp, "%.5f %.5f %.5f\n", s.playerScale, s.bossScale, s.timerScale);
            for (const TextItem& label : s.labels)
                std::fprintf(fp, "%d %d %d %d %.5f %s\n", int(label.enabled), label.x, label.y, int(label.worldAttached),
                    label.baseScale, EncodeText(label.text).c_str());
        }
        const bool ok = std::fclose(fp) == 0;
        status = ok ? "HUD設定を保存しました" : "HUD設定の保存に失敗";
        return ok;
    }
    inline bool Load(const char* path) {
        FILE* fp = nullptr;
        if (fopen_s(&fp, path, "r") != 0 || !fp) { status = "HUD設定なし（初期設定）"; return false; }
        char version[32]{};
        Settings next[4]{};
        bool ok = fscanf_s(fp, "%31s", version, (unsigned)sizeof(version)) == 1;
        const bool v4 = std::string(version) == "GAME_HUD_V4";
        const bool v3 = std::string(version) == "GAME_HUD_V3";
        const bool v2 = std::string(version) == "GAME_HUD_V2";
        ok = ok && (v4 || v3 || v2 || std::string(version) == "GAME_HUD_V1");
        for (int i = 0; i < 4 && ok; ++i) {
            int p, b, a, t;
            Settings& s = next[i];
            ok = fscanf_s(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d",
                &s.rule, &s.limitSeconds, &p, &b, &a, &t,
                &s.playerX, &s.playerY, &s.bossX, &s.bossY,
                &s.timerX, &s.timerY, &s.barWidth) == 13;
            if (ok) {
                s.showPlayerHp = p != 0; s.showBossHp = b != 0;
                s.showArmor = a != 0; s.showTimer = t != 0;
                if (v3 || v4) {
                    int followPlayer = 0, followBoss = 0, followTimer = 0;
                    ok = fscanf_s(fp, "%d %d %d", &followPlayer, &followBoss, &followTimer) == 3;
                    s.playerWorldAttached = followPlayer != 0;
                    s.bossWorldAttached = followBoss != 0;
                    s.timerWorldAttached = followTimer != 0;
                    if (v4) {
                        ok = fscanf_s(fp, "%f %f %f", &s.playerScale, &s.bossScale, &s.timerScale) == 3;
                    }
                }
                if (v2 || v3 || v4) {
                    for (TextItem& label : s.labels) {
                        int enabled = 0;
                        char word[128]{};
                        int labelFollow = 0;
                        if (v4) {
                            ok = fscanf_s(fp, "%d %d %d %d %f %127s", &enabled, &label.x, &label.y,
                                &labelFollow, &label.baseScale, word, (unsigned)sizeof(word)) == 6;
                        }
                        else if (v3) {
                            ok = fscanf_s(fp, "%d %d %d %d %127s", &enabled, &label.x, &label.y,
                                &labelFollow, word, (unsigned)sizeof(word)) == 5;
                        }
                        else {
                            ok = fscanf_s(fp, "%d %d %d %127s", &enabled, &label.x, &label.y,
                                word, (unsigned)sizeof(word)) == 4;
                        }
                        label.worldAttached = labelFollow != 0;
                        if (!ok) break;
                        label.enabled = enabled != 0;
                        if (!DecodeText(word, label.text)) { ok = false; break; }
                    }
                }
                Validate(s);
            }
        }
        std::fclose(fp);
        if (ok) {
            for (int i = 0; i < 4; ++i) stages[i] = next[i];
            ResetTimer();
        }
        status = ok ? "HUD設定を読み込みました" : "HUD設定の読込に失敗";
        return ok;
    }
    // HUDに設定した座標は等倍・標準カメラ時の画面座標。
    // カメラ追従の場合はその点をステージのワールド位置として扱い、ズーム・移動を反映。
    // テキストの文字サイズはNovice::ScreenPrintfの仕様により固定のまま。
    inline void ResolvePosition(int x, int y, bool worldAttached, int& screenX, int& screenY) {
        screenX = x; screenY = y;
        if (!worldAttached) return;
        const float z = GetCameraZoom();
        const float camX = CameraBackgroundTuning::cameraX;
        const float camY = CameraBackgroundTuning::cameraY;
        screenX = int(640.0f + (float(x) - camX) * z);
        screenY = int(360.0f - (720.0f - float(y) - camY) * z);
    }
    // 標準サイズでは従来のScreenPrintfを使う。拡大縮小時は小型の5x7文字で描画する。
    // ScreenPrintf単体には文字倍率指定がないため、拡大時のみピクセル文字に切り替える。
    inline uint8_t GlyphRow(char c, int row) {
        static const uint8_t digits[10][7] = {
            {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
            {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,30,1,1,17,14},
            {6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
            {14,17,17,15,1,2,12}
        };
        static const uint8_t letters[26][7] = {
            {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{15,16,16,16,16,16,15},
            {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
            {15,16,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
            {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
            {17,27,21,21,17,17,17},{17,25,25,21,19,19,17},{14,17,17,17,17,17,14},
            {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
            {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
            {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
            {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
        };
        if (row < 0 || row >= 7) return 0;
        if (c >= '0' && c <= '9') return digits[c - '0'][row];
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z') return letters[c - 'A'][row];
        switch (c) {
        case ':': return row == 2 || row == 5 ? 4 : 0;
        case '/': return (row < 7) ? uint8_t(1 << ((std::min)(4, 6 - row))) : 0;
        case '-': return row == 3 ? 31 : 0;
        case '_': return row == 6 ? 31 : 0;
        case '.': return row == 6 ? 4 : 0;
        case '!': return row == 6 ? 4 : (row == 5 ? 0 : 4);
        case '%': { static const uint8_t pct[7] = { 17,18,4,4,8,17,0 }; return pct[row]; }
        case '+': return row == 3 ? 31 : (row >= 1 && row <= 5 ? 4 : 0);
        case '(': return row == 0 || row == 6 ? 2 : 4;
        case ')': return row == 0 || row == 6 ? 8 : 4;
        case '?': { static const uint8_t q[7] = { 14,17,1,2,4,0,4 }; return q[row]; }
        default: return 0;
        }
    }
    inline float EffectiveScale(float base, bool attached) {
        return base * (attached ? GetCameraZoom() : 1.0f);
    }
    inline void DrawScaledText(int x, int y, const char* value, float scale) {
        if (std::fabs(scale - 1.0f) < 0.015f) { Novice::ScreenPrintf(x, y, "%s", value); return; }
        // 描画負荷を抑えるためサイズは1～6pxの整数単位に丸める。
        const int pixel = Clamp(int(scale * 2.0f + 0.5f), 1, 12);
        const int pitch = 6 * pixel;
        int ox = x;
        for (int n = 0; value[n] && n < 80; ++n) {
            unsigned char c = (unsigned char)value[n];
            if (c >= 128) { ox += pitch; continue; } // 日本語は従来どおり別フォント対応
            for (int r = 0; r < 7; ++r) {
                const uint8_t bits = GlyphRow(char(c), r);
                for (int j = 0; j < 5; ++j)
                    if (bits & (1 << (4 - j)))
                        Novice::DrawBox(ox + j * pixel, y + r * pixel, pixel, pixel, 0, 0xFFFFFFFF, kFillModeSolid);
            }
            ox += pitch;
        }
    }
    inline void DrawBar(int x, int y, int width, int current, int maximum, unsigned int color, float scale) {
        const int safeMax = (std::max)(maximum, 1);
        const int drawnWidth = (std::max)(2, int(width * scale));
        const int drawnHeight = (std::max)(2, int(15.0f * scale));
        const int filled = Clamp(current, 0, safeMax) * (drawnWidth - 2) / safeMax;
        Novice::DrawBox(x, y, drawnWidth, drawnHeight, 0, 0x222735FF, kFillModeSolid);
        Novice::DrawBox(x + 1, y + 1, filled, (std::max)(1, drawnHeight - 2), 0, color, kFillModeSolid);
    }
    inline void Draw(int playerHp, int maxPlayerHp, int bossHp, int maxBossHp,
        int armor, int maxArmor, bool enraged) {
        const Settings& s = Current();
        int px = 0, py = 0, bx = 0, by = 0, tx = 0, ty = 0;
        ResolvePosition(s.playerX, s.playerY, s.playerWorldAttached, px, py);
        ResolvePosition(s.bossX, s.bossY, s.bossWorldAttached, bx, by);
        ResolvePosition(s.timerX, s.timerY, s.timerWorldAttached, tx, ty);
        const float ps = EffectiveScale(s.playerScale, s.playerWorldAttached);
        const float bs = EffectiveScale(s.bossScale, s.bossWorldAttached);
        const float ts = EffectiveScale(s.timerScale, s.timerWorldAttached);
        char caption[160]{};
        if (s.showPlayerHp && UsesHp()) {
            DrawBar(px, py, s.barWidth, playerHp, maxPlayerHp, 0x46DBADFF, ps);
            std::snprintf(caption, sizeof(caption), "PLAYER HP %d/%d", playerHp, maxPlayerHp);
            DrawScaledText(px, py + int(22 * ps), caption, ps);
        }
        if (s.showBossHp) {
            DrawBar(bx, by, s.barWidth, bossHp, maxBossHp, enraged ? 0xFF9A54FF : 0xD65B67FF, bs);
            std::snprintf(caption, sizeof(caption), "BOSS HP %d/%d", bossHp, maxBossHp);
            DrawScaledText(bx, by + int(22 * bs), caption, bs);
        }
        if (s.showArmor) {
            std::snprintf(caption, sizeof(caption), "ARMOR %d/%d", armor, maxArmor);
            DrawScaledText(bx, by + int((s.showBossHp ? 42 : 0) * bs), caption, bs);
        }
        if (s.showTimer && (UsesTime() || preview)) {
            const int secs = SecondsLeft();
            std::snprintf(caption, sizeof(caption), "TIME %02d:%02d", secs / 60, secs % 60);
            DrawScaledText(tx, ty, caption, ts);
        }
        for (const TextItem& label : s.labels) {
            if (label.enabled && label.text[0]) {
                int x = 0, y = 0;
                ResolvePosition(label.x, label.y, label.worldAttached, x, y);
                DrawScaledText(x, y, label.text, EffectiveScale(label.baseScale, label.worldAttached));
            }
        }
    }
#ifdef USE_IMGUI
    inline void DrawPanel() {
        Settings& s = Current();
        ImGui::Text("ステージ %d のゲームルール・HUD", currentStage + 1);
        const char* rules[] = { "HP制（HPが0で敗北）", "時間制（時間切れで敗北）", "HP＋時間制（両方有効）" };
        if (ImGui::Combo("ゲームルール", &s.rule, rules, 3)) ResetTimer();
        if (s.rule != Hp) {
            if (ImGui::SliderInt("制限時間（秒）", &s.limitSeconds, 10, 1800)) ResetTimer();
            ImGui::Text("現在の残り時間: %d秒", SecondsLeft());
            if (ImGui::Button("タイマーをリセット")) ResetTimer();
        }
        ImGui::TextWrapped("時間制のみではプレイヤーHPによる敗北を無効にします。ボスを倒すとクリア、時間切れでゲームオーバーです。");
        ImGui::Separator();
        ImGui::TextUnformatted("HUDの表示項目");
        ImGui::Checkbox("プレイヤーHP表示", &s.showPlayerHp);
        ImGui::Checkbox("ボスHP表示", &s.showBossHp);
        ImGui::Checkbox("装甲数表示", &s.showArmor);
        ImGui::Checkbox("タイマー表示", &s.showTimer);
        ImGui::Checkbox("タイマーのプレビュー", &preview);
        ImGui::Separator();
        ImGui::TextUnformatted("HUDの表示位置（画面左上から）");
        ImGui::DragInt2("プレイヤーHUD X/Y", &s.playerX, 1.0f, 0, 1200);
        ImGui::Checkbox("プレイヤーHUDをカメラに追従", &s.playerWorldAttached);
        ImGui::SliderFloat("プレイヤーHUD基本倍率", &s.playerScale, 0.5f, 3.0f, "%.2fx");
        ImGui::DragInt2("ボスHUD X/Y", &s.bossX, 1.0f, 0, 1200);
        ImGui::Checkbox("ボスHUD・装甲数をカメラに追従", &s.bossWorldAttached);
        ImGui::SliderFloat("ボスHUD基本倍率", &s.bossScale, 0.5f, 3.0f, "%.2fx");
        ImGui::DragInt2("タイマー X/Y", &s.timerX, 1.0f, 0, 1200);
        ImGui::Checkbox("タイマーをカメラに追従", &s.timerWorldAttached);
        ImGui::SliderFloat("タイマー基本倍率", &s.timerScale, 0.5f, 3.0f, "%.2fx");
        ImGui::TextDisabled("OFF: 画面固定 / ON: ステージと一緒に移動・ズーム");
        ImGui::TextDisabled("基本倍率はズームなしでも有効。追従時は基本倍率 x カメラ倍率で表示。");
        ImGui::SliderInt("HPバー横幅", &s.barWidth, 80, 400);
        ImGui::Separator();
        if (ImGui::CollapsingHeader("追加テキストUI（最大4個）", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("文字列は半角英数字を推奨します。日本語の描画には別途フォント対応が必要です。");
            for (int i = 0; i < 4; ++i) {
                TextItem& item = s.labels[i];
                ImGui::PushID(1000 + i);
                ImGui::Checkbox("表示", &item.enabled);
                ImGui::SameLine(); ImGui::Text("テキスト %d", i + 1);
                ImGui::InputText("表示内容", item.text, sizeof(item.text));
                ImGui::DragInt2("表示位置 X/Y", &item.x, 1.0f, 0, 1200);
                ImGui::Checkbox("カメラに追従", &item.worldAttached);
                ImGui::SliderFloat("文字の基本倍率", &item.baseScale, 0.5f, 3.0f, "%.2fx");
                ImGui::Separator();
                ImGui::PopID();
            }
        }
        Validate(s);
        ImGui::TextDisabled("実画面に即時反映。上部の『まとめて保存』で保存できます。");
        ImGui::Text("保存状態: %s", status);
    }
#endif
}
