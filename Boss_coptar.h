#pragma once
#include "Vector2.h"

class Boss {
private:
    Vector2 position_ = {};
    float speed_ = 2.0f;

public:
    void Initialize(Vector2 spawnPos);
    void Update();
    void Draw() const;
};

