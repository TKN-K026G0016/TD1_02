#pragma once
#include "BossBase.h"
#include "PortalArena.h"
#include "CreatorGameRuntime.h"
#include "AssetEditor.h"
#include <string>

// ステージ1・2の共通状態。更新・描画・エディターが同一のデータを参照する。
namespace StageBattleInternal {
    struct BattleContext {
        CreatorGameRuntime::Runtime creatorRuntime;
        Boss boss;
        PortalArena arena;
        BossBattleSettings liveBattleConfig;
        bool stageCleared = false;
        int reloadIndicatorFrames = 0;
        bool f8Prev = false;
        int tutorialFrame = 0;
        int screwRepairCooldownFrames = 0;
        bool repairWasActive = false;
        bool showDebugOverlay = false;
        int clearDelayFrames = 0;
    };
    extern BattleContext battleContext;
    inline std::string CameraBGFilePath() {
        AssetEditor::FixSettingsLocation();
        return AssetEditor::resourceRoot + "\\camera_background_settings.txt";
    }

    inline std::string PlayerTuningFilePath() {
        AssetEditor::FixSettingsLocation();
        return AssetEditor::resourceRoot + "\\player_tuning_settings.txt";
    }

    inline std::string BossCombatFilePath() {
        AssetEditor::FixSettingsLocation();
        return AssetEditor::resourceRoot + "\\boss_combat_editor_settings.txt";
    }
    inline std::string GameHudFilePath() {
        AssetEditor::FixSettingsLocation();
        return AssetEditor::resourceRoot + "\\game_hud_settings.txt";
    }
    inline std::string Stage2FilePath() {
        AssetEditor::FixSettingsLocation();
        return AssetEditor::resourceRoot + "\\stage2_trajectory.txt";
    }

}

namespace StageBattleEditor {
    void Draw();
    bool IsOpen();
}
