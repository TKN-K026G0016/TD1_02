#pragma once
// ステージ1・2で共有する戦闘処理。ステージ3は独立したCloudStageを使用する。
#include "Stage.h"

enum class StageId { Copter = 1, Coaster = 2, Cloud = 3 };

StageId GetStageId();
void ResetStageBattle(StageId stage); // エディターからの再開始にも使用

namespace StageBattle {
    void LoadSettings();
    void Reset(StageId stage);
    void UpdateShortcuts(const char* liveKeys);
    void Update(const char* liveKeys);
    void Draw();
    bool IsCleared();
}
