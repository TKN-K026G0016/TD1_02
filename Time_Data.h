#pragma once

struct TimeData {
	int min;
	int sec;
	int frame;
};

void ResetTimeData(TimeData& data);
void CountTimeData(TimeData& data);
int ConvertTimeDataToInt(const TimeData& data);