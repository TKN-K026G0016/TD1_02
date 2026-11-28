#pragma once
#include "Vector2.h"
class Boss;
class PortalArena;

namespace Stage2 {
    void Init();
    void Update(const char* liveKeys);
    void Draw();
    void DrawClearGuide();
    void SetupBoss(Boss& boss);
    bool IsBossOffscreen(const Boss& boss);
    void UpdateGimmicks(PortalArena& arena, const Boss& boss);
    int HitPlayer(PortalArena& arena, Vector2 player, Vector2* source);
}
