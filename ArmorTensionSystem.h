#pragma once
#include "ExactPlayerBattle.h"
#include "BossCombatEditorSettings.h"
#include <cmath>

// Boss-side armor mechanics. The sample player remains responsible for
// physics, two-arm grappling and its 96-segment tentacle rendering.
class ArmorTensionSystem {
public:
    static constexpr int kMaxParts = 16;
    static constexpr float kBaseRequiredTension = 400.0f;
    static constexpr float kLowestRequiredTension = 160.0f;
    void Reset() {
        for (int i = 0;i < kMaxParts;++i) loosen_[i] = 0.0f;
        displayTarget_ = -1; lastGap_ = 0.0f; resultFrames_ = 0; success_ = false;
        for (int h = 0; h < 2; ++h) previousGrip_[h] = -1;
        for (int i = 0; i < kMaxParts; ++i) hitCooldown_[i] = 0;
    }
    // fixedCount is the number of ordinary grip points before boss targets.
    // Return boss-part index only when a real 2-arm yank has sufficient tension.
    int Update(const ExactPlayerBattle::Snapshot& snap, int fixedCount, int partCount,
        const Vector2* gripPoints, int gripPointCount) {
        if (resultFrames_ > 0) --resultFrames_;
        for (int i = 0; i < kMaxParts; ++i)
            if (hitCooldown_[i] > 0) --hitCooldown_[i];

        // ネジに「新しく接続した瞬間」だけ緩み度を加算する。
        // 掴み続けた時間・巻き取り量は緩み度に影響しない。
        for (int h = 0; h < 2; ++h) {
            const int grip = snap.handGripIndex[h];
            const int part = grip - fixedCount;
            if (grip != previousGrip_[h] && part >= 0 && part < partCount &&
                part < kMaxParts && part > 0 && part < partCount - 1 &&
                grip < gripPointCount && gripPoints &&
                hitCooldown_[part] == 0) {
                const float dx = gripPoints[grip].x - snap.position.x;
                const float dy = gripPoints[grip].y - snap.position.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const auto& tuning = BossCombatEditor::values;
                const float span = tuning.hitMaxDistance - tuning.hitMinDistance;
                const float rate = span > 0.001f ? Clamp01((distance - tuning.hitMinDistance) / span) : 1.0f;
                const float gain = tuning.hitMinLoosen +
                    (tuning.hitMaxLoosen - tuning.hitMinLoosen) * rate;
                loosen_[part] = Clamp01(loosen_[part] + gain);
                displayTarget_ = part;
                hitCooldown_[part] = tuning.hitCooldownFrames;
                // 緩ませだけでネジを外す。接続したこのフレームに一度だけ発動。
                // 本体(0)と目(最終番号)は破壊対象ではない。
                if (tuning.loosenAutoPeelEnabled &&
                    loosen_[part] >= tuning.loosenAutoPeelThreshold) {
                    previousGrip_[h] = grip;
                    previousGrip_[1 - h] = snap.handGripIndex[1 - h];
                    success_ = true;
                    resultFrames_ = 60;
                    return part;
                }
            }
            previousGrip_[h] = grip;
        }
        const int target = snap.yankTarget - fixedCount;
        // yankTarget はプレイヤー側で「背景GPの巻き取り中、ボス側の
        // 触手が限界まで張った瞬間」にのみ発行される。
        // 接続だけでは剥がれず、張力を発生させる必要がある。
        if (BossCombatEditor::values.twoGripPeelEnabled &&
            snap.yankTarget >= fixedCount && target > 0 &&
            target < partCount - 1 && target < kMaxParts) {
            displayTarget_ = target;
            lastGap_ = snap.yankGap;
            const bool enough = snap.yankGap >= RequiredTension(target);
            success_ = enough;
            resultFrames_ = 60;
            if (enough) return target;
        }
        return -1;
    }
    float Loosen(int index) const {
        return index >= 0 && index < kMaxParts ? loosen_[index] : 0.0f;
    }
    float RequiredTension(int index) const {
        return BossCombatEditor::values.requiredTensionFull -
            (BossCombatEditor::values.requiredTensionFull - BossCombatEditor::values.requiredTensionLoose) * Loosen(index);
    }
    // AIが巻き直しを行う。緩み度を減らす（0%で完全に締まった状態）。
    void Tighten(int index, float amount) {
        if (index >= 0 && index < kMaxParts && amount>0.0f)
            loosen_[index] = Clamp01(loosen_[index] - amount);
    }
    int MostLoose(int count, float* loosenValue = nullptr) const {
        int index = -1; float best = 0.0f;
        for (int i = 0;i < count && i < kMaxParts;++i)
            if (loosen_[i] > best) { best = loosen_[i]; index = i; }
        if (loosenValue)*loosenValue = best;
        return index;
    }
    int DisplayTarget() const { return displayTarget_; }
    float LastGap() const { return lastGap_; }
    int ResultFrames() const { return resultFrames_; }
    bool LastSuccess() const { return success_; }
    // HUD: 0=未操作, 1=成功, 2=張力不足
    int LastYankState() const { return resultFrames_ > 0 ? (success_ ? 1 : 2) : 0; }

    void OnPartPeeled(int index) {
        if (index >= 0 && index < kMaxParts) loosen_[index] = 0.0f;
    }
private:
    static float Clamp01(float a) { return a < 0 ? 0 : (a > 1 ? 1 : a); }
    float loosen_[kMaxParts] = {};
    int previousGrip_[2] = { -1, -1 };
    int hitCooldown_[kMaxParts] = {};
    int displayTarget_ = -1;
    float lastGap_ = 0.0f;
    int resultFrames_ = 0;
    bool success_ = false;
};
