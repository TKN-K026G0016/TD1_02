#pragma once
// ステージ2の攻撃軌道/GP頻度パラメータ。プレイヤー物理・当たり判定には触れない。
#include "Vector2.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Stage2Tuning {
    struct Settings {
        float cutterEdgeMargin = 48.0f; // 画面端からの余白
        float cutterTop = 75.0f;       // 8の字上端（画面Y下向き）
        float cutterBottom = 635.0f;   // 8の字下端
        float cutterCenterX = 640.0f;  // 中央縦断位置
        int cutterDuration = 165;       // 8の字1周フレーム数
        float missileStartSpeed = 5.0f;
        float missileAcceleration = 0.065f;
        float missileTurnDeg = 1.55f;  // 1フレームの旋回角度
        int missileHomingFrames = 50;
        int missileInterval = 65;
        float gpScrollSpeed = 1.15f;
        float gpSpacing = 640.0f;
        // 第2ステージの流れるGP。追加テクスチャは使わない。
        int gpCount = 2;
        float gpStartX = 115.0f;
        float gpLeftY = 470.0f;
        float gpRightY = 545.0f;
        float rushHeight = 192.0f;      // 世界座標(+Y上)
        float rushSpeedMultiplier = 1.75f;
        float verticalStartHeight = 600.0f; // 世界座標
        float alignXSpeed = 15.0f;
        float alignYSpeed = 13.0f;
        int verticalWaitFrames = 32;
        float verticalAccel = 1.9f;
        float verticalMaxSpeed = 31.0f;
        int verticalRecoveryFrames = 38;
        float verticalReturnHeight = 370.0f;
        // Stage2：4種類の高速フライバイを1つの突進AIで実現
        float flybyOutsideMargin = 90.0f; // 機体全体が画面外まで抜ける距離
        float flybyArcHeight = 180.0f;     // 斜め・S字の高度差
        float flybySpeed = 1.0f;          // 横突進速度の追加倍率
        bool flybyVariants = true;         // OFFなら水平飛行のみ

    };
    inline Settings values{};
    inline char status[256] = "Stage2: default settings";
    inline void ClampValues(Settings& v) {
        v.cutterEdgeMargin = std::clamp(v.cutterEdgeMargin, 10.0f, 270.0f);
        v.cutterTop = std::clamp(v.cutterTop, 15.0f, 300.0f);
        v.cutterBottom = std::clamp(v.cutterBottom, 400.0f, 700.0f);
        v.cutterCenterX = std::clamp(v.cutterCenterX, 400.0f, 880.0f);
        v.cutterDuration = std::clamp(v.cutterDuration, 45, 180);
        v.missileStartSpeed = std::clamp(v.missileStartSpeed, 1.0f, 25.0f);
        v.missileAcceleration = std::clamp(v.missileAcceleration, 0.0f, 0.5f);
        v.missileTurnDeg = std::clamp(v.missileTurnDeg, 0.0f, 12.0f);
        v.missileHomingFrames = std::clamp(v.missileHomingFrames, 0, 160);
        v.missileInterval = std::clamp(v.missileInterval, 20, 180);
        v.gpScrollSpeed = std::clamp(v.gpScrollSpeed, 0.0f, 6.0f);
        v.gpSpacing = std::clamp(v.gpSpacing, 260.0f, 1000.0f);
        v.gpCount = std::clamp(v.gpCount, 0, 2);
        v.gpStartX = std::clamp(v.gpStartX, 0.0f, 1280.0f);
        v.gpLeftY = std::clamp(v.gpLeftY, 50.0f, 650.0f);
        v.gpRightY = std::clamp(v.gpRightY, 50.0f, 650.0f);
        v.rushHeight = std::clamp(v.rushHeight, 180.0f, 650.0f);
        v.rushSpeedMultiplier = std::clamp(v.rushSpeedMultiplier, 0.5f, 4.0f);
        v.verticalStartHeight = std::clamp(v.verticalStartHeight, 350.0f, 650.0f);
        v.alignXSpeed = std::clamp(v.alignXSpeed, 2.0f, 30.0f);
        v.alignYSpeed = std::clamp(v.alignYSpeed, 2.0f, 30.0f);
        v.verticalWaitFrames = std::clamp(v.verticalWaitFrames, 5, 120);
        v.verticalAccel = std::clamp(v.verticalAccel, 0.1f, 8.0f);
        v.verticalMaxSpeed = std::clamp(v.verticalMaxSpeed, 8.0f, 70.0f);
        v.verticalRecoveryFrames = std::clamp(v.verticalRecoveryFrames, 0, 180);
        v.verticalReturnHeight = std::clamp(v.verticalReturnHeight, 200.0f, 550.0f);
        v.flybyOutsideMargin = std::clamp(v.flybyOutsideMargin, 20.0f, 300.0f);
        v.flybyArcHeight = std::clamp(v.flybyArcHeight, 0.0f, 280.0f);
        v.flybySpeed = std::clamp(v.flybySpeed, 0.5f, 3.0f);
    }
    // 既存ステージ2の8の字をポイントで再現。
    // 中心(640付近)では上下に長い直線区間があり、画面両端まで曲がる。
    inline Vector2 CutterPoint(float t) {
        const Settings& v = values;
        const float center = v.cutterCenterX;
        const float left = v.cutterEdgeMargin;
        const float right = 1280.0f - v.cutterEdgeMargin;
        const float high = v.cutterTop;
        const float low = v.cutterBottom;
        const float mid = (high + low) * 0.5f;
        const Vector2 path[] = {
            {center, mid}, {center - (center - left) * 0.39f, low}, {left + 37.0f,low - 55.0f},
            {left,mid}, {left + 37.0f,high + 75.0f}, {center - (center - left) * 0.39f, high},
            {center,high + 10.0f}, {center,mid}, {center,low},
            {center + (right - center) * 0.39f,low}, {right - 42.0f,low - 55.0f},
            {right,mid}, {right - 42.0f,high + 75.0f}, {center + (right - center) * 0.39f,high},
            {center,high + 10.0f}, {center,mid}, {center,low}
        };
        constexpr int count = int(sizeof(path) / sizeof(path[0]));
        t = t - std::floor(t);
        const float pos = t * float(count - 1);
        const int seg = (std::min)((int)pos, count - 2);
        const float u = pos - float(seg);
        const float smooth = u * u * (3.0f - 2.0f * u);
        return { path[seg].x + (path[seg + 1].x - path[seg].x) * smooth,
                 path[seg].y + (path[seg + 1].y - path[seg].y) * smooth };
    }
    // 保存先はAssetEditorのresourceRootを受け取る（作業ディレクトリに依存しない）。
    inline bool Save(const char* path) {
        FILE* f = nullptr;
        if (fopen_s(&f, path, "wb") != 0 || !f) {
            std::snprintf(status, sizeof(status), "Save FAILED: %s", path);return false;
        }
        ClampValues(values);
        const Settings& v = values;
        std::fprintf(f, "Stage2TrajectoryV1\n");
        std::fprintf(f, "%.6f %.6f %.6f %.6f %d %.6f %.6f %.6f %d %d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %d %.6f %.6f %d %.6f\n",
            v.cutterEdgeMargin, v.cutterTop, v.cutterBottom, v.cutterCenterX, v.cutterDuration,
            v.missileStartSpeed, v.missileAcceleration, v.missileTurnDeg, v.missileHomingFrames,
            v.missileInterval, v.gpScrollSpeed, v.gpSpacing, v.rushHeight, v.rushSpeedMultiplier,
            v.verticalStartHeight, v.alignXSpeed, v.alignYSpeed, v.verticalWaitFrames, v.verticalAccel,
            v.verticalMaxSpeed, v.verticalRecoveryFrames, v.verticalReturnHeight);
        std::fprintf(f, "%.6f %.6f %.6f %d\n", v.flybyOutsideMargin,
            v.flybyArcHeight, v.flybySpeed, v.flybyVariants ? 1 : 0);
        // 旧設定ファイルとの互換を保つため末尾に追加。
        std::fprintf(f, "%d %.6f %.6f %.6f\n", v.gpCount, v.gpStartX, v.gpLeftY, v.gpRightY);
        const bool writeOk = std::ferror(f) == 0;
        const bool closeOk = std::fclose(f) == 0;
        const bool ok = writeOk && closeOk;
        std::snprintf(status, sizeof(status), "Stage2 save: %s", ok ? "OK" : "FAILED");
        return ok;
    }
    inline bool Load(const char* path) {
        FILE* f = nullptr;
        if (fopen_s(&f, path, "rb") != 0 || !f) { std::snprintf(status, sizeof(status), "Stage2: default settings");return false; }
        char magic[64]{};
        Settings v;
        const bool goodHeader = fscanf_s(f, "%63s", magic, (unsigned)sizeof(magic)) == 1 && std::strcmp(magic, "Stage2TrajectoryV1") == 0;
        int n = 0;
        if (goodHeader) {
            n = fscanf_s(f, "%f %f %f %f %d %f %f %f %d %d %f %f %f %f %f %f %f %d %f %f %d %f",
                &v.cutterEdgeMargin, &v.cutterTop, &v.cutterBottom, &v.cutterCenterX, &v.cutterDuration,
                &v.missileStartSpeed, &v.missileAcceleration, &v.missileTurnDeg, &v.missileHomingFrames,
                &v.missileInterval, &v.gpScrollSpeed, &v.gpSpacing, &v.rushHeight, &v.rushSpeedMultiplier,
                &v.verticalStartHeight, &v.alignXSpeed, &v.alignYSpeed, &v.verticalWaitFrames, &v.verticalAccel,
                &v.verticalMaxSpeed, &v.verticalRecoveryFrames, &v.verticalReturnHeight);
        }
        // 旧設定ファイルの末尾に追加された値。なければデフォルトを使用。
        if (n == 22) {
            int variants = 1;
            if (fscanf_s(f, "%f %f %f %d", &v.flybyOutsideMargin,
                &v.flybyArcHeight, &v.flybySpeed, &variants) == 4)
                v.flybyVariants = variants != 0;
            // 旧形式には存在しないため、未指定なら初期値を維持する。
            (void)fscanf_s(f, "%d %f %f %f", &v.gpCount,
                &v.gpStartX, &v.gpLeftY, &v.gpRightY);
        }
        std::fclose(f);
        const bool finite = std::isfinite(v.cutterEdgeMargin) && std::isfinite(v.cutterTop) &&
            std::isfinite(v.cutterBottom) && std::isfinite(v.cutterCenterX) &&
            std::isfinite(v.missileStartSpeed) && std::isfinite(v.missileAcceleration) &&
            std::isfinite(v.missileTurnDeg) && std::isfinite(v.gpScrollSpeed) &&
            std::isfinite(v.gpSpacing) && std::isfinite(v.rushHeight) &&
            std::isfinite(v.rushSpeedMultiplier) && std::isfinite(v.verticalStartHeight) &&
            std::isfinite(v.alignXSpeed) && std::isfinite(v.alignYSpeed) &&
            std::isfinite(v.verticalAccel) && std::isfinite(v.verticalMaxSpeed) &&
            std::isfinite(v.verticalReturnHeight);
        if (!goodHeader || n != 22 || !finite) {
            std::snprintf(status, sizeof(status), "Stage2 load FAILED");return false;
        }
        ClampValues(v);values = v;
        std::snprintf(status, sizeof(status), "Stage2 loaded: OK");return true;
    }
}
