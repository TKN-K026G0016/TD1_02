#include<cmath>
#include<Novice.h>
#include "MovingPlatform.h"

void MovingPlatform::Initialize(Vector2 position) {
	position_ = position;
	// 各地点を初期位置からの相対位置で設定
	points_[0] = position;
	points_[1] = { position.x + 150.0f, position.y };
	points_[2] = { position.x + 280.0f, position.y + 150.0f };
	points_[3] = { position.x + 430.0f, position.y + 100.0f };
	points_[4] = { position.x + 550.0f, position.y };
	// 最初は1番の地点へ向かう
	targetIndex_ = 1;
	previousPosition_ = position_;
	moveDelta_ = { 0.0f, 0.0f };

}

void MovingPlatform::Update() {
	// 移動する前の座標を保存
	previousPosition_ = position_;

	// 次の目標地点を取得する
	Vector2 target = points_[targetIndex_];
	// 現在位置から目標までの距離をXとYで計算
	float dx = target.x - position_.x;
	float dy = target.y - position_.y;

	// 目標までの直線距離
	float distance = sqrtf(dx * dx + dy * dy);

	if (distance > 3.0f) {

		// 移動方向を求め、速度3pxで進む
		position_.x += (dx / distance) * 3.0f;
		position_.y += (dy / distance) * 3.0f;

	}
	else {

		// 到着したので次の地点に進む
		targetIndex_++;

		// 最後の地点を越えたら0番に戻る
		if (targetIndex_ >= kPointCount) {
			targetIndex_ = 0;
		}
	}

	// 今回どれだけ移動したかを計算する
	moveDelta_.x = position_.x - previousPosition_.x;
	moveDelta_.y = position_.y - previousPosition_.y;

}

void MovingPlatform::Draw() const {

	// 足場の大きさ
	const int width = 120;
	const int height = 24;

	// 足場の中心座標から左上座標を計算
	int drawX = static_cast<int>(
		position_.x - width / 2
		);

	int drawY = static_cast<int>(
		position_.y - height / 2
		);

	// 足場を四角形で描画する
	Novice::DrawBox(
		drawX,
		drawY,
		width,
		height,
		0.0f,
		0x69C8EFFF,
		kFillModeSolid
	);
}

