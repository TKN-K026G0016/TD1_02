#pragma once
#include "Vector2.h"
namespace ExactPlayerBattle {
    struct Snapshot {
        Vector2 position = { 420,128 }; Vector2 velocity = {}; bool gripping = false; bool pulling = false; int gripIndex = -1; int peeledStake = -1;
        int handGripIndex[2] = { -1,-1 }; bool handPulling[2] = { false,false };
        int loosenTarget[2] = { -1,-1 }; float loosenDelta[2] = { 0.0f,0.0f };
        int yankTarget = -1; float yankGap = 0.0f;
    };
    void SetBossAnchorStart(int index);
    // 第2ステージでスクロールする背景GPの個数（0なら通常ステージ）。
    void SetMovingAnchorCount(int count);
    void RequestReleaseBossGrips();
    int BossAnchorStart();
    void Reset();
    void SetAlternate(bool alternate);
    bool IsAlternate();
    void Update(const Vector2* anchors, int count);
    void Draw();
    Snapshot GetSnapshot();
}
