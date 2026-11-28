// ステージ1の進行管理。ボス固有の処理は専用のBossファイルへ。
#include "Stage1.h"
#include "StageBattle.h"
#include <Novice.h>

namespace Stage1 {
    void Init() { StageBattle::Reset(StageId::Copter); }
    void Update(const char* liveKeys) { StageBattle::Update(liveKeys); }
    void Draw() { StageBattle::Draw(); }
    void DrawClearGuide() { Novice::ScreenPrintf(475, 330, "SPACE: STAGE 2    R: RETRY STAGE 1"); }
}
