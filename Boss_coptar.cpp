#include "Boss_coptar.h"
#include "Tool.h"

// 初期位置を設定する
void Boss::Initialize(Vector2 spawnPos) {

    
    position_ = spawnPos;
}

// ボスを移動させる
void Boss::Update() {

    position_.x += speed_;
  

}

// ボスを描画する
void Boss::Draw() const {

    DrawHitCircle(position_, 40.0f, 0xFF00FFFF, true);

}

