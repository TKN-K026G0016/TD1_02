// ステージ2の進行管理。ボス固有の処理は専用のBossファイルへ。
#include "Stage2.h"
#include "StageBattle.h"
#include <Novice.h>
#include "BossBase.h"
#include "PortalArena.h"

namespace Stage2 {
    void Init() { StageBattle::Reset(StageId::Coaster); }
    void Update(const char* liveKeys) { StageBattle::Update(liveKeys); }
    void Draw() { StageBattle::Draw(); }
    void SetupBoss(Boss& boss) { boss.StartCoasterStage(); }
    bool IsBossOffscreen(const Boss& boss) {
        return boss.GetPosition().x < 0.0f || boss.GetPosition().x > 1280.0f;
    }
    void UpdateGimmicks(PortalArena& arena, const Boss& boss) {
        arena.UpdateStage2(boss.GetPosition(), boss.GetHp(), boss.GetMaxHp(),
            boss.GetStage2SpecialMode(), boss.GetStage2SpecialFrame());
    }
    int HitPlayer(PortalArena& arena, Vector2 player, Vector2* source) {
        return arena.Stage2HitPlayer(player, source);
    }
    void DrawClearGuide() { Novice::ScreenPrintf(518, 330, "SPACE: STAGE 3    R: RETRY"); }
}
