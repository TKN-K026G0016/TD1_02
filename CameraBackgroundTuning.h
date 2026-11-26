#pragma once
// 軽量なステージ別カメラ・背景設定。画像はAssetEditorで読み込み済みのものを再利用。
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace CameraBackgroundTuning {
    struct Settings {
        bool follow = false;             // 既定では従来通り固定カメラ
        float followEase = 0.12f;       // 1フレームの追従係数
        float zoom = 1.0f;             // 通常時の倍率
        float offsetX = 0.0f, offsetY = 0.0f;
        float minX = 640.0f, maxX = 640.0f; // 既定では中央固定
        float minY = 360.0f, maxY = 360.0f;
        bool parallax = false;          // 既定は従来の背景描画
        float speeds[3] = { 0.2f, 0.5f, 1.0f }; // 遠・中・近（カメラ移動への追従率）
        float autoSpeed[3] = { 0.0f, 0.0f, 0.0f }; // px/frame
    };
    inline Settings stages[2]{};
    inline int stageIndex = 0;
    inline float backgroundTime = 0.0f;
    inline float cameraX = 640.0f, cameraY = 360.0f;
    inline float targetX = 640.0f, targetY = 360.0f;
    inline const char* status = "未保存";
    inline Settings& Current() { return stages[stageIndex]; }
    inline float Clamp(float n, float a, float b) { return (std::max)(a, (std::min)(b, n)); }
    inline void SelectStage(int stage) {
        const int index = stage == 2 ? 1 : 0;
        if (index != stageIndex) {
            stageIndex = index;
            cameraX = 640.0f; cameraY = 360.0f;
            targetX = 640.0f; targetY = 360.0f;
            backgroundTime = 0.0f;
        }
    }
    inline void SetTarget(float x, float y) { targetX = x; targetY = y; }
    inline void Tick() { backgroundTime += 1.0f; if (backgroundTime > 100000.0f) backgroundTime = 0.0f; }
    inline bool Save(const char* file) {
        FILE* fp = nullptr; if (fopen_s(&fp, file, "w") != 0 || !fp) { status = "カメラ保存失敗"; return false; }
        std::fprintf(fp, "CAMERA_BG_V1\n");
        for (const Settings& s : stages) {
            std::fprintf(fp, "%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %d\n",
                int(s.follow), s.followEase, s.zoom, s.offsetX, s.offsetY, s.minX, s.maxX, s.minY, s.maxY, int(s.parallax));
            std::fprintf(fp, "%.6f %.6f %.6f %.6f %.6f %.6f\n",
                s.speeds[0], s.speeds[1], s.speeds[2], s.autoSpeed[0], s.autoSpeed[1], s.autoSpeed[2]);
        }
        const bool ok = std::fclose(fp) == 0; status = ok ? "カメラ保存成功" : "カメラ保存失敗";return ok;
    }
    inline bool Load(const char* file) {
        FILE* fp = nullptr;if (fopen_s(&fp, file, "r") != 0 || !fp) { status = "カメラ設定なし（初期値）";return false; }
        char tag[32]{}; Settings tmp[2]{};
        bool ok = fscanf_s(fp, "%31s", tag, (unsigned)sizeof(tag)) == 1 && std::string(tag) == "CAMERA_BG_V1";
        for (int i = 0;i < 2 && ok;++i) {
            int f = 0, p = 0; Settings& s = tmp[i];
            ok = fscanf_s(fp, "%d %f %f %f %f %f %f %f %f %d",
                &f, &s.followEase, &s.zoom, &s.offsetX, &s.offsetY, &s.minX, &s.maxX, &s.minY, &s.maxY, &p) == 10;
            if (ok) ok = fscanf_s(fp, "%f %f %f %f %f %f",
                &s.speeds[0], &s.speeds[1], &s.speeds[2], &s.autoSpeed[0], &s.autoSpeed[1], &s.autoSpeed[2]) == 6;
            s.follow = f != 0;s.parallax = p != 0;
            ok = ok && std::isfinite(s.zoom) && s.zoom >= 0.5f && s.zoom <= 2.0f
                && s.followEase >= 0.01f && s.followEase <= 1.0f
                && s.minX <= s.maxX && s.minY <= s.maxY;
            for (int k = 0;k < 3;++k) ok = ok && std::isfinite(s.speeds[k]) && std::isfinite(s.autoSpeed[k])
                && s.speeds[k] >= 0 && s.speeds[k] <= 2 && std::fabs(s.autoSpeed[k]) <= 15;
        }
        std::fclose(fp);
        if (ok) { for (int i = 0;i < 2;++i) stages[i] = tmp[i]; }
        status = ok ? "カメラ読込成功" : "カメラ設定が不正（維持）"; return ok;
    }
#ifdef USE_IMGUI
    inline void DrawPanel() {
        Settings& s = Current();
        ImGui::Text("編集対象: ステージ%d", stageIndex + 1);
        ImGui::TextUnformatted("変更はゲーム画面に即時反映。保存はウィンドウ上部の『まとめて保存』。");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("カメラ", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("実際のカメラ中心: X %.1f / Y %.1f", cameraX, cameraY);
            ImGui::Text("プレイヤー座標: X %.1f / Y %.1f", targetX, targetY);
            ImGui::Checkbox("プレイヤー追従", &s.follow);
            ImGui::SameLine();
            if (ImGui::Button("固定カメラに戻す")) {
                s.follow = false;
                s.offsetX = s.offsetY = 0.0f;
                s.minX = s.maxX = 640.0f;
                s.minY = s.maxY = 360.0f;
            }
            if (s.follow && s.minX == s.maxX && s.minY == s.maxY) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                    "移動範囲が固定されています。追従するには『追従範囲を広げる』を押してください。");
            }
            if (ImGui::Button("画面内で追従を有効化")) {
                // 1280x720画面の内側だけ。実際の可動範囲はズーム率でも制限する。
                s.minX = 0.0f; s.maxX = 1280.0f;
                s.minY = 0.0f; s.maxY = 720.0f;
                s.follow = true;
            }
            ImGui::SliderFloat("追従速度", &s.followEase, 0.01f, 1.0f, "%.2f");
            ImGui::SliderFloat("ズーム倍率", &s.zoom, 1.0f, 2.0f, "%.2f");
            ImGui::TextWrapped("ステージ外が見えないように、カメラを1280x720の内側に自動制限します。等倍では上下左右にスクロールできません。追従を見せる場合は倍率を1より大きくしてください。");
            ImGui::DragFloat("カメラXオフセット", &s.offsetX, 1.0f, -600.0f, 600.0f);
            ImGui::DragFloat("カメラYオフセット", &s.offsetY, 1.0f, -400.0f, 400.0f);
            if (ImGui::TreeNode("カメラの移動制限（詳細）")) {
                ImGui::DragFloatRange2("X 範囲", &s.minX, &s.maxX, 1.0f, -5000.0f, 5000.0f);
                ImGui::DragFloatRange2("Y 範囲", &s.minY, &s.maxY, 1.0f, -5000.0f, 5000.0f);
                ImGui::TreePop();
            }
        }
        if (ImGui::CollapsingHeader("背景スクロール", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("背景スクロールを使用", &s.parallax);
            ImGui::TextWrapped("遠景 = ステージ背景 / 中景 = 追加PNG 1 / 近景 = 追加PNG 2");
            ImGui::TextWrapped("追従倍率0ならカメラに連動せず、1なら同じ速度で動きます。");
            const char* label[3] = { "遠景", "中景", "近景" };
            for (int i = 0; i < 3; ++i) {
                ImGui::PushID(i);
                if (ImGui::TreeNodeEx(label[i], i == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
                    ImGui::SliderFloat("カメラ連動倍率", &s.speeds[i], 0.0f, 2.0f, "%.2f");
                    ImGui::SliderFloat("自動スクロール速度 (px/frame)", &s.autoSpeed[i], -15.0f, 15.0f, "%.2f");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("背景スクロール停止")) {
                for (int i = 0; i < 3; ++i) s.autoSpeed[i] = 0.0f;
            }
        }
        ImGui::Separator();
        if (ImGui::Button("このステージのカメラ・背景設定を初期化")) s = Settings{};
        ImGui::TextUnformatted(status);
    }
#endif
}
