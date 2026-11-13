#pragma once
#include "Vector2.h"


enum class BossVfxMode {
    Idle, Warning, Rush, Press, Shot, Recovery
};

class BossVisualEffect {
private:
    static constexpr int kMaxParticles = 48;
    static constexpr int kMaxTrails = 8;

    struct Particle {
        Vector2 pos = {};
        Vector2 velocity = {};
        float radius = 2.0f;
        unsigned int rgb = 0xFFFFFFFF;
        int life = 0;
        int maxLife = 0;
        int type = 0;  // 0=火花, 1=煙, 2=破片
        bool active = false;
    };
    struct Trail {
        Vector2 pos = {};
        int life = 0;
        bool active = false;
    };

    Particle particles_[kMaxParticles] = {};
    Trail trails_[kMaxTrails] = {};
    unsigned int randomSeed_ = 0x7F4A7C15u;
    BossVfxMode mode_ = BossVfxMode::Idle;
    int globalFrame_ = 0;
    int modeFrame_ = 0;
    int impactFrame_ = -1;
    int muzzleFrame_ = -1;
    float warningProgress_ = 0.0f;
    float rotorAngle_ = 0.0f;
    float groundY_ = 120.0f;
    Vector2 aimPosition_ = {};
    Vector2 impactPosition_ = {};
    Vector2 muzzlePosition_ = {};
    Vector2 lastPosition_ = {};
    float lastVelocityX_ = 0.0f;
    float lastVelocityY_ = 0.0f;
    int particleCursor_ = 0;
    int trailCursor_ = 0;

    float Random01();
    void Emit(Vector2 origin, int type, int amount);

public:
    void Initialize(Vector2 bossPosition, float groundY);
    void SetMode(BossVfxMode nextMode);
    void SetWarning(float progress, Vector2 aimPosition);
    void SetAim(Vector2 aimPosition);
    void OnImpact(Vector2 position);
    void OnMuzzleFlash(Vector2 position);
    void Update(Vector2 bossPosition);
    void DrawBehind(Vector2 bossPosition) const;
    void DrawBody(Vector2 bossPosition) const;
    void DrawFront() const;
    void DrawBullet(Vector2 position, Vector2 velocity) const;
};
