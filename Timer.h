#pragma once

class Timer {
public:
	//コンストラクタ
	Timer(int time);

	//タイマーリセット
	void Reset(void);
	//カウントアップ処理
	void CountUp(void);

	//達したかのチェック
	bool IsReady(void) const;

private:

	int time_ = 0;
	int count_ = 0;
};