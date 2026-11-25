#pragma once
#include "ExactPlayerBattle.h"
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
    }
    // fixedCount is the number of ordinary grip points before boss targets.
    // Return boss-part index only when a real 2-arm yank has sufficient tension.
    int Update(const ExactPlayerBattle::Snapshot& snap, int fixedCount, int partCount) {
        if (resultFrames_ > 0) --resultFrames_;
        for (int h = 0;h < 2;++h) {
            const int target = snap.loosenTarget[h] - fixedCount;
            if (target >= 0 && target < partCount && target < kMaxParts) {
                // A full wind supplies roughly 25% of the source stake HP.
                // Three to four good winds give substantial preparation.
                loosen_[target] = Clamp01(loosen_[target] + snap.loosenDelta[h] * 1.2f);
                displayTarget_ = target;
            }
        }
        const int target = snap.yankTarget - fixedCount;
        if (snap.yankTarget >= fixedCount && target >= 0 && target < partCount && target < kMaxParts) {
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
        return kBaseRequiredTension -
            (kBaseRequiredTension - kLowestRequiredTension) * Loosen(index);
    }
    int DisplayTarget() const { return displayTarget_; }
    float LastGap() const { return lastGap_; }
    int ResultFrames() const { return resultFrames_; }
    bool LastSuccess() const { return success_; }
    void OnPartPeeled(int index) {
        if (index >= 0 && index < kMaxParts) loosen_[index] = 0.0f;
    }
private:
    static float Clamp01(float a) { return a < 0 ? 0 : (a > 1 ? 1 : a); }
    float loosen_[kMaxParts] = {};
    int displayTarget_ = -1;
    float lastGap_ = 0.0f;
    int resultFrames_ = 0;
    bool success_ = false;
};
