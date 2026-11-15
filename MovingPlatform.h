#pragma once

#include "Vector2.h"

class MovingPlatform {
private:

	// レール上の通過地点を最大5個保存
	static const int kPointCount = 5;
	int targetIndex_;
	Vector2 points_[kPointCount];
	Vector2 position_;  // 足場の現在位置
	Vector2 previousPosition_; // 更新前の足場の位置
	Vector2 moveDelta_;        // 1フレームで移動した量


public:
	void Initialize(Vector2 position);
	void Update();
	void Draw() const;

};
