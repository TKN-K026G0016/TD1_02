// ステージ管理：初期化・切り替え・更新・描画の振り分けのみを担当。
#include "Stage.h"
#include "StageBattle.h"
#include "Stage1.h"
#include "Stage2.h"
#include "CloudStage.h"
#include <Novice.h>
#include <dinput.h>

namespace {
    StageId currentStage = StageId::Copter;
    bool f7Prev = false;
    bool spacePrev = false;
    constexpr Vector2 kGravityScale = { 0.0f, -1.2f };
}

StageId GetStageId() { return currentStage; }

void ResetStageBattle(StageId stage) {
    currentStage = stage;
    switch (currentStage) {
    case StageId::Copter: Stage1::Init(); break;
    case StageId::Coaster: Stage2::Init(); break;
    case StageId::Cloud: InitCloudStage(); break;
    }
}

void InitStage() {
    StageBattle::LoadSettings();
    ResetStageBattle(StageId::Copter);
}

void UpdateStage() {
    char liveKeys[256] = {};
    Novice::GetHitKeyStateAll(liveKeys);
    StageBattle::UpdateShortcuts(liveKeys);

    // F7でステージ切り替え。SPACEはステージ1・2のクリア後のみ有効。
    const bool f7Now = liveKeys[DIK_F7] != 0;
    const bool spaceNow = liveKeys[DIK_SPACE] != 0;
    const bool nextStage = (f7Now && !f7Prev) ||
        (spaceNow && !spacePrev && StageBattle::IsCleared() && currentStage != StageId::Cloud);
    f7Prev = f7Now;
    spacePrev = spaceNow;
    if (nextStage) {
        switch (currentStage) {
        case StageId::Copter: ResetStageBattle(StageId::Coaster); break;
        case StageId::Coaster: ResetStageBattle(StageId::Cloud); break;
        case StageId::Cloud: ResetStageBattle(StageId::Copter); break;
        }
        return;
    }
    switch (currentStage) {
    case StageId::Copter: Stage1::Update(liveKeys); break;
    case StageId::Coaster: Stage2::Update(liveKeys); break;
    case StageId::Cloud: UpdateCloudStage(); break;
    }
}

void DrawStage() {
    switch (currentStage) {
    case StageId::Copter: Stage1::Draw(); break;
    case StageId::Coaster: Stage2::Draw(); break;
    case StageId::Cloud: DrawCloudStage(); break;
    }
}

Vector2 GetGravityScale() { return kGravityScale; }
