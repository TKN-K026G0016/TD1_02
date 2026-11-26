#pragma once
// ボス攻撃とネジ巻き直しの調整値。毎フレームのファイル読み書きはしない。
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace BossCombatEditor {
    struct Settings {
        bool attackEnabled[5] = { true,true,true,true,true }; // Press,Shot,Rush,Top,YoYo
        float startOffsetX[5] = {};
        float startOffsetY[5] = {};
        bool missileEnabled = true;
        bool cutterEnabled = true;
        float missileOffsetX = 0.0f, missileOffsetY = 0.0f;
        float cutterOffsetX = 0.0f, cutterOffsetY = 0.0f;
        bool repairEnabled = true;
        float repairThreshold = 0.50f;
        int repairCooldown = 300;
        int repairDuration = 120;
        float repairPerFrame = 0.004f;
        float loosenMultiplier = 1.20f; // 旧設定ファイル互換用（新判定では使用しない）
        float hitMinLoosen = 0.15f; // 近距離ヒットで加算する緩み度
        float hitMaxLoosen = 0.45f; // 遠距離ヒットで加算する緩み度
        float hitMinDistance = 0.0f;
        float hitMaxDistance = 400.0f;
        int hitCooldownFrames = 12; // 60fpsで0.3秒
        float bossGripRangeMultiplier = 1.50f;
        float bossGripAimCos = 0.35f;
        float bossGripPriorityBonus = 50.0f;
        float requiredTensionFull = 280.0f;
        float requiredTensionLoose = 112.0f;
        int failedYankDamage = 0;
        // 2つの攻略ルート（旧保存ファイルでは既定値を使用）。
        bool loosenAutoPeelEnabled = true;  // 緩ませだけでネジを外す
        float loosenAutoPeelThreshold = 1.0f; // 自動で外れる緩み度
        bool twoGripPeelEnabled = true;     // ボスGP＋背景GPで引き剥がす
    };
    inline Settings values{};
    inline char status[180] = "Boss combat: defaults";
    inline bool AnyAttackEnabled() { for (bool enabled : values.attackEnabled) if (enabled)return true; return false; }
    inline bool Enabled(int attack) { return attack >= 0 && attack < 5 && values.attackEnabled[attack]; }
    inline void Clamp() {
        auto& v = values;
        v.missileOffsetX = std::clamp(v.missileOffsetX, -400.0f, 400.0f);
        v.missileOffsetY = std::clamp(v.missileOffsetY, -300.0f, 300.0f);
        v.cutterOffsetX = std::clamp(v.cutterOffsetX, -400.0f, 400.0f);
        v.cutterOffsetY = std::clamp(v.cutterOffsetY, -300.0f, 300.0f);
        v.repairThreshold = std::clamp(v.repairThreshold, 0.05f, 1.0f);
        v.repairCooldown = std::clamp(v.repairCooldown, 0, 1800);
        v.repairDuration = std::clamp(v.repairDuration, 30, 600);
        v.repairPerFrame = std::clamp(v.repairPerFrame, 0.0001f, 0.05f);
        v.loosenMultiplier = std::clamp(v.loosenMultiplier, 0.0f, 5.0f);
        v.hitMinLoosen = std::clamp(v.hitMinLoosen, 0.0f, 1.0f);
        v.hitMaxLoosen = std::clamp(v.hitMaxLoosen, v.hitMinLoosen, 1.0f);
        v.hitMinDistance = std::clamp(v.hitMinDistance, 0.0f, 2000.0f);
        v.hitMaxDistance = std::clamp(v.hitMaxDistance, v.hitMinDistance + 1.0f, 3000.0f);
        v.hitCooldownFrames = std::clamp(v.hitCooldownFrames, 0, 600);
        v.requiredTensionFull = std::clamp(v.requiredTensionFull, 30.0f, 1500.0f);
        v.requiredTensionLoose = std::clamp(v.requiredTensionLoose, 10.0f, v.requiredTensionFull);
        v.failedYankDamage = std::clamp(v.failedYankDamage, 0, 50);
        v.loosenAutoPeelThreshold = std::clamp(v.loosenAutoPeelThreshold, 0.10f, 1.0f);
        v.bossGripRangeMultiplier = std::clamp(v.bossGripRangeMultiplier, 1.0f, 2.0f);
        v.bossGripAimCos = std::clamp(v.bossGripAimCos, -0.5f, 0.9f);
        v.bossGripPriorityBonus = std::clamp(v.bossGripPriorityBonus, 0.0f, 250.0f);
        for (int i = 0;i < 5;++i) { v.startOffsetX[i] = std::clamp(v.startOffsetX[i], -500.0f, 500.0f);v.startOffsetY[i] = std::clamp(v.startOffsetY[i], -300.0f, 300.0f); }
    }
    // サクサク設定：背景GPや触手の物理は変更せず、ボスGPの照準と装甲判定だけ調整する。
    inline void ApplyQuickArmorPreset() {
        auto& v = values;
        v.bossGripRangeMultiplier = 1.50f; // ボスGP照準距離を1.5倍
        v.bossGripAimCos = 0.35f;          // 照準角を少し緩和
        v.bossGripPriorityBonus = 50.0f;  // 近くの候補ではボスGPを少し優先
        v.requiredTensionFull = 280.0f;   // 標準400の70%
        v.requiredTensionLoose = 112.0f;  // 標準160の70%
        v.hitMinLoosen = 0.15f;
        v.hitMaxLoosen = 0.45f;
        v.hitMinDistance = 0.0f;
        v.hitMaxDistance = 400.0f;
        v.hitCooldownFrames = 12;
        Clamp();
        std::snprintf(status, sizeof(status), "Quick armor preset applied - save to keep");
    }
    inline bool Save(const char* file) {
        Clamp(); FILE* f = nullptr; if (fopen_s(&f, file, "wb") != 0 || !f) { std::snprintf(status, sizeof(status), "Save failed");return false; }
        std::fprintf(f, "BossCombatV1\n");
        for (int i = 0;i < 5;++i)std::fprintf(f, "%d %.6f %.6f\n", values.attackEnabled[i] ? 1 : 0, values.startOffsetX[i], values.startOffsetY[i]);
        std::fprintf(f, "%d %.6f %d %d %.6f %.6f %.6f %.6f %d\n", values.repairEnabled ? 1 : 0, values.repairThreshold, values.repairCooldown, values.repairDuration, values.repairPerFrame, values.loosenMultiplier, values.requiredTensionFull, values.requiredTensionLoose, values.failedYankDamage);
        std::fprintf(f, "%d %d %.6f %.6f %.6f %.6f\n", values.missileEnabled ? 1 : 0, values.cutterEnabled ? 1 : 0, values.missileOffsetX, values.missileOffsetY, values.cutterOffsetX, values.cutterOffsetY);
        // V1の末尾へ追記。旧設定を読み込む処理はそのまま維持。
        std::fprintf(f, "HIT_LOOSEN_V2 %.6f %.6f %.6f %.6f %d\n",
            values.hitMinLoosen, values.hitMaxLoosen, values.hitMinDistance,
            values.hitMaxDistance, values.hitCooldownFrames);
        std::fprintf(f, "BOSS_GRIP_ASSIST_V3 %.6f %.6f %.6f\n", values.bossGripRangeMultiplier, values.bossGripAimCos, values.bossGripPriorityBonus);
        std::fprintf(f, "ARMOR_ROUTES_V4 %d %.6f %d\n",
            values.loosenAutoPeelEnabled ? 1 : 0,
            values.loosenAutoPeelThreshold, values.twoGripPeelEnabled ? 1 : 0);
        bool ok = !std::ferror(f);ok = std::fclose(f) == 0 && ok;std::snprintf(status, sizeof(status), ok ? "Saved" : "Save failed");return ok;
    }
    inline bool Load(const char* file) {
        FILE* f = nullptr;if (fopen_s(&f, file, "rb") != 0 || !f)return false;
        char magic[40]{}; bool ok = fscanf_s(f, "%39s", magic, (unsigned)sizeof(magic)) == 1 && std::strcmp(magic, "BossCombatV1") == 0;
        Settings v{};
        for (int i = 0;i < 5 && ok;++i) { int enabled = 0;ok = fscanf_s(f, "%d %f %f", &enabled, &v.startOffsetX[i], &v.startOffsetY[i]) == 3;v.attackEnabled[i] = enabled != 0; }
        int repairEnabled = 0;
        if (ok)ok = fscanf_s(f, "%d %f %d %d %f %f %f %f %d", &repairEnabled, &v.repairThreshold, &v.repairCooldown, &v.repairDuration, &v.repairPerFrame, &v.loosenMultiplier, &v.requiredTensionFull, &v.requiredTensionLoose, &v.failedYankDamage) == 9;
        v.repairEnabled = repairEnabled != 0;
        // 追加値はファイルの末尾。旧V1設定は初期値を維持できる。
        int missile = 1, cutter = 1;
        const int extras = fscanf_s(f, "%d %d %f %f %f %f", &missile, &cutter, &v.missileOffsetX, &v.missileOffsetY, &v.cutterOffsetX, &v.cutterOffsetY);
        if (extras == 6) { v.missileEnabled = missile != 0;v.cutterEnabled = cutter != 0; }
        // 旧ファイルにはこの行がないので、読み込み失敗にはしない。
        char tag[40] = {};
        if (fscanf_s(f, "%39s", tag, (unsigned)sizeof(tag)) == 1 &&
            std::strcmp(tag, "HIT_LOOSEN_V2") == 0) {
            float a, b, c, d; int e;
            if (fscanf_s(f, "%f %f %f %f %d", &a, &b, &c, &d, &e) == 5) {
                v.hitMinLoosen = a; v.hitMaxLoosen = b;
                v.hitMinDistance = c; v.hitMaxDistance = d; v.hitCooldownFrames = e;
            }
        }
        // 保存形式の違いを吸収する（従来の3数値のみ、V2行、V3行）。
        // NOTE: 前段でHIT_LOOSEN_V2を読めなかったケースも対象にする。
        if (std::strcmp(tag, "HIT_LOOSEN_V2") != 0 && tag[0]) {
            // 旧版ではタグなしで照準3値を保存していた。先頭値を既に読み取っている。
            float range = 0, cos = 0, bonus = 0;
            if (sscanf_s(tag, "%f", &range) == 1 && fscanf_s(f, "%f %f", &cos, &bonus) == 2) {
                v.bossGripRangeMultiplier = range; v.bossGripAimCos = cos; v.bossGripPriorityBonus = bonus;
            }
        }
        else {
            char extraTag[40] = {};
            if (fscanf_s(f, "%39s", extraTag, (unsigned)sizeof(extraTag)) == 1 &&
                std::strcmp(extraTag, "BOSS_GRIP_ASSIST_V3") == 0) {
                fscanf_s(f, "%f %f %f", &v.bossGripRangeMultiplier, &v.bossGripAimCos, &v.bossGripPriorityBonus);
            }
        }
        // V4は任意追加行。旧設定ファイルには存在しない。
        char routeTag[40] = {};
        while (fscanf_s(f, "%39s", routeTag, (unsigned)sizeof(routeTag)) == 1) {
            if (std::strcmp(routeTag, "ARMOR_ROUTES_V4") == 0) {
                int autoPeel = 1, twoGrip = 1;
                float threshold = 1.0f;
                if (fscanf_s(f, "%d %f %d", &autoPeel, &threshold, &twoGrip) == 3) {
                    v.loosenAutoPeelEnabled = autoPeel != 0;
                    v.loosenAutoPeelThreshold = threshold;
                    v.twoGripPeelEnabled = twoGrip != 0;
                }
                break;
            }
        }
        std::fclose(f);if (ok) { values = v;Clamp(); }
        std::snprintf(status, sizeof(status), ok ? "Loaded" : "Load failed");return ok;
    }
}
