#pragma once
// プレイヤーのゲーム内調整値。初期値は移植用サンプルと完全に同じ。
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <cstddef>
#ifdef USE_IMGUI
#include <imgui.h>
#endif
namespace PlayerTuning {
    struct Settings {
        float moveSpeed = 8.0f;
        float gravity = 0.8f;
        float maxFallSpeed = 24.0f;
        float jumpPower = 20.0f;
        float jumpCut = 5.0f;
        float lockRange = 400.0f;
        float gripReachSpeed = 45.0f;
        float gripPullSpeed = 22.0f;
        float gripLaunchSpeed = 26.0f;
    };
    inline Settings values{};
    inline void Clamp() {
        values.moveSpeed = (std::clamp)(values.moveSpeed, 0.0f, 30.0f);
        values.gravity = (std::clamp)(values.gravity, 0.0f, 3.0f);
        values.maxFallSpeed = (std::clamp)(values.maxFallSpeed, 1.0f, 60.0f);
        values.jumpPower = (std::clamp)(values.jumpPower, 0.0f, 45.0f);
        values.jumpCut = (std::clamp)(values.jumpCut, 0.0f, 30.0f);
        values.lockRange = (std::clamp)(values.lockRange, 50.0f, 1000.0f);
        values.gripReachSpeed = (std::clamp)(values.gripReachSpeed, 1.0f, 120.0f);
        values.gripPullSpeed = (std::clamp)(values.gripPullSpeed, 1.0f, 80.0f);
        values.gripLaunchSpeed = (std::clamp)(values.gripLaunchSpeed, 1.0f, 80.0f);
    }
    inline bool Save(const char* file) {
        Clamp();
        FILE* fp = nullptr;
        if (fopen_s(&fp, file, "w") != 0 || fp == nullptr) return false;
        const int n = fprintf(fp, "PLAYER_TUNING_V1\n%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
            values.moveSpeed, values.gravity, values.maxFallSpeed, values.jumpPower,
            values.jumpCut, values.lockRange, values.gripReachSpeed, values.gripPullSpeed, values.gripLaunchSpeed);
        const int closed = fclose(fp);
        return n > 0 && closed == 0;
    }
    inline bool Load(const char* file) {
        FILE* fp = nullptr;
        if (fopen_s(&fp, file, "r") != 0 || fp == nullptr) return false;
        char magic[32]{};
        Settings temp{};
        const bool ok = fscanf_s(fp, "%31s", magic, (unsigned)sizeof(magic)) == 1 &&
            strcmp(magic, "PLAYER_TUNING_V1") == 0 &&
            fscanf_s(fp, "%f %f %f %f %f %f %f %f %f", &temp.moveSpeed,
                &temp.gravity, &temp.maxFallSpeed, &temp.jumpPower, &temp.jumpCut,
                &temp.lockRange, &temp.gripReachSpeed, &temp.gripPullSpeed,
                &temp.gripLaunchSpeed) == 9;
        fclose(fp);
        if (ok) { values = temp; Clamp(); }
        return ok;
    }
#ifdef USE_IMGUI
    inline void DrawPanel() {
        ImGui::TextUnformatted("変更した数値はその場で反映されます。初期値は元のサンプルと同じです。");
        if (ImGui::CollapsingHeader("移動・ジャンプ", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("左右移動速度", &values.moveSpeed, 0.0f, 30.0f, "%.1f px/f");
            ImGui::SliderFloat("重力", &values.gravity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("落下速度上限", &values.maxFallSpeed, 1.0f, 60.0f, "%.1f");
            ImGui::SliderFloat("ジャンプ初速", &values.jumpPower, 0.0f, 45.0f, "%.1f");
            ImGui::SliderFloat("短押しジャンプ速度", &values.jumpCut, 0.0f, 30.0f, "%.1f");
        }
        if (ImGui::CollapsingHeader("触手・グリップ", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("GPの自動照準距離", &values.lockRange, 50.0f, 1000.0f, "%.0f px");
            ImGui::SliderFloat("触手を伸ばす速さ", &values.gripReachSpeed, 1.0f, 120.0f, "%.1f px/f");
            ImGui::SliderFloat("巻き取り速度", &values.gripPullSpeed, 1.0f, 80.0f, "%.1f px/f");
            ImGui::SliderFloat("離脱時の飛び出し速度", &values.gripLaunchSpeed, 1.0f, 80.0f, "%.1f px/f");
        }
        Clamp();
        if (ImGui::Button("プレイヤー設定を初期値に戻す")) values = Settings{};
        ImGui::TextDisabled("画像・描画倍率は『画像・アニメ』タブで編集してください。");
        ImGui::TextDisabled("当たり判定は変更しません。HPはこのタブの『HP・耐久力』から変更できます。");
    }
#endif
}
