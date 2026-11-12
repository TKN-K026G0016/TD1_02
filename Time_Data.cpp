#include "Time_Data.h"

/// <summary>
/// 指定TimeDataのリセット処理
/// </summary>
/// <param name="data">リセットするデータ</param>
void ResetTimeData(TimeData& data) {
	data.frame = 0;
	data.sec = 0;
	data.min = 0;
}

/// <summary>
/// TimeDataの変更処理(毎フレーム呼び出し)
/// </summary>
/// <param name="data">変更したいtimeData</param>
void CountTimeData(TimeData& data) {
	data.frame++;
	if (data.frame >= 60) {
		data.frame = 0;
		data.sec++;
	}
	if (data.sec >= 60) {
		data.sec = 0;;
		data.min++;
	}
}

/// <summary>
/// TimeDataをintに変換
/// </summary>
/// <param name="data">対象のデータ</param>
/// <returns>int型のTimeData</returns>
int ConvertTimeDataToInt(const TimeData& data) {
	int value = 0;
	value += data.frame;
	value += data.sec * 60;
	value += data.min * 3600;

	return value;
}