#pragma once
// F5: 日本語の最終調整パネル。ゲームロジックを変更せず、既存の設定データだけ編集する。
#include "BossBattleSettings.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace FinalTuningEditor {
    struct Actions {
        bool apply = false;
        bool save = false;
        bool reload = false;
        bool reset = false;
        bool restart = false;
        bool stage1 = false;
        bool stage2 = false;
        int testAttack = -1; // 0:突進 1:プレス 2:射撃 3:コマ 4:ヨーヨー
    };
#ifdef USE_IMGUI
    inline bool open = false;
    inline bool dirty = false;
    inline void Help(const char* description) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(330.0f);
            ImGui::TextUnformatted(description);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
    inline void SliderF(const char* label, float* value, float low, float high, const char* tooltip) {
        if (ImGui::SliderFloat(label, value, low, high, "%.2f")) dirty = true;
        Help(tooltip);
    }
    inline void SliderI(const char* label, int* value, int low, int high, const char* tooltip) {
        if (ImGui::SliderInt(label, value, low, high)) dirty = true;
        Help(tooltip);
    }
    inline Actions DrawContents(BossBattleSettings& s, int bossHP, int playerHP, bool coaster) {
        Actions a{};
        ImGui::TextUnformatted("F5: 表示切替   F6: リソース   F9: 詳細ボス設定");
        ImGui::Text("ステージ: %s   ボスHP: %d   プレイヤーHP: %d", coaster ? "第2" : "第1", bossHP, playerHP);
        ImGui::Separator();
        if (ImGui::BeginTabBar("FinalTuningTabs")) {
            if (ImGui::BeginTabItem("基本")) {
                SliderI("ボス最大HP", &s.bossMaxHp, 1, 10000, "最大HPは戦闘リスタート後に完全反映します。");
                SliderI("プレイヤー最大HP", &s.playerMaxHp, 1, 1000, "最大HPを変更します。移動やジャンプの挙動には触れません。");
                SliderI("外装復帰HP割合", &s.phase2HpPercent, 1, 99, "第1ステージで使用するHP割合です。");
                SliderI("通常攻撃ダメージ", &s.normalDamage, 0, 500, "通常攻撃のダメージ。");
                SliderI("回転攻撃ダメージ", &s.spinDamage, 0, 500, "回転攻撃のダメージ。");
                SliderI("グラップルダメージ", &s.grappleDamage, 0, 500, "グラップルによる攻撃ダメージ。");
                SliderI("連打ダメージ", &s.mashDamage, 0, 500, "ダウン中の連打ダメージ。");
                SliderI("被ダメージ倍率 (%)", &s.incomingDamagePercent, 0, 500, "100で標準、50で半分です。");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("ボス移動")) {
                SliderF("通常飛行速度", &s.flightSpeed, 0.1f, 30.f, "第1段階の飛行速度。");
                SliderF("強化時飛行速度", &s.flightSpeedPhase2, 0.1f, 35.f, "強化時の飛行速度。");
                SliderI("飛行方向変更間隔", &s.flightInterval, 10, 360, "フレーム単位。値を小さくすると頻繁に方向が変わります。");
                SliderI("強化時変更間隔", &s.flightIntervalPhase2, 10, 360, "フレーム単位。");
                SliderI("攻撃間の待機", &s.idleFrames, 8, 180, "ステージ2では一部の行動に固定待機時間があります。");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("攻撃")) {
                SliderF("横突進の最大速度", &s.rushSpeed, 5.f, 50.f, "ボスの横突進速度。");
                SliderF("横突進の加速度", &s.rushAccel, 0.1f, 10.f, "突進開始後の加速度。");
                SliderI("突進の予兆", &s.rushWarning, 8, 180, "フレーム単位。60フレームで約1秒。");
                SliderI("突進後の隙", &s.rushDizzy, 0, 240, "反撃可能な硬直時間。");
                ImGui::Separator();
                SliderF("急降下の最高速度", &s.pressSpeed, 5.f, 80.f, "縦方向のプレス攻撃に使う速度。");
                SliderF("急降下の加速度", &s.pressAccel, 0.1f, 10.f, "縦方向のプレスの加速度。");
                SliderI("急降下の予兆", &s.pressWarning, 8, 180, "急降下を予告する時間。");
                SliderI("急降下後の隙", &s.pressStun, 0, 180, "着地後の反撃チャンス。");
                SliderI("射撃の予兆", &s.shotWarning, 8, 180, "射撃を予告する時間。");
                SliderF("射撃の先読み", &s.shotPredict, 0.f, 40.f, "プレイヤーの移動先を狙う強さ。");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("コマ・ヨーヨー")) {
                SliderF("コマの初速", &s.topInitialSpeed, 3.f, 45.f, "コマの飛び出し速度。");
                SliderF("コマの重力", &s.topGravity, 0.1f, 8.f, "落下の加速度。");
                SliderF("床バウンドの高さ", &s.topBounceHeight, 0.f, 30.f, "床で回転中の上下幅。");
                SliderF("コマの戻り加速度", &s.topReturnAccel, 0.005f, 0.5f, "折り返し後の速度補間。");
                SliderF("コマの当たり半径", &s.topHitRadius, 20.f, 128.f, "見た目とヒット判定の調整に使用。");
                SliderI("コマの最大生存時間", &s.topMaxLifetime, 180, 1800, "異常時の安全停止フレーム数。");
                SliderF("ヨーヨーの射程", &s.yoyoRange, 100.f, 900.f, "最大の伸び距離。");
                SliderI("ヨーヨーの時間", &s.yoyoDuration, 30, 300, "伸びて戻るまでのフレーム数。");
                SliderI("コマ・ヨーヨーの予兆", &s.toyWarning, 8, 180, "攻撃前の構え時間。");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("テスト")) {
                if (ImGui::Button("第1ステージへ")) a.stage1 = true;
                ImGui::SameLine();
                if (ImGui::Button("第2ステージへ")) a.stage2 = true;
                ImGui::Separator();
                if (ImGui::Button("横突進をテスト")) a.testAttack = 0;
                if (ImGui::Button("急降下をテスト")) a.testAttack = 1;
                if (ImGui::Button("射撃をテスト")) a.testAttack = 2;
                if (ImGui::Button("コマをテスト")) a.testAttack = 3;
                if (ImGui::Button("ヨーヨーをテスト")) a.testAttack = 4;
                ImGui::TextWrapped("注意: テスト技は現在のボスAIの状態によって反映タイミングが異なります。");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Separator();
        if (!s.Valid()) ImGui::TextColored(ImVec4(1, .4f, .4f, 1), "設定値が無効です。保存・反映できません。");
        if (ImGui::Button("ゲームに反映")) a.apply = true;
        ImGui::SameLine();
        if (ImGui::Button("保存")) a.save = true;
        ImGui::SameLine();
        if (ImGui::Button("再読込")) a.reload = true;
        ImGui::SameLine();
        if (ImGui::Button("戦闘リスタート")) a.restart = true;
        if (ImGui::Button("初期値に戻す")) a.reset = true;
        ImGui::SameLine();
        ImGui::TextUnformatted(dirty ? "未保存の変更あり" : "保存後の状態");
        ImGui::TextWrapped("変更値は「ゲームに反映」で適用します。保存でTXTに記録します。再読込とリスタートは現在の戦闘を初期化します。");
        return a;
    }
#endif
}
