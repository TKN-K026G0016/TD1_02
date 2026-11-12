#include "se.h"
#include "tool.h"

#include <Novice.h>

struct SE {
	int aHandle = -1;
	float volume = 1.0f;
	//再生中かの状態
	int pHandle = -1;
};
static SE ses[ToInt(SeType::Count)] = {};

#pragma region データ: 音量
float seVolume = 1.0f;

#pragma endregion


void InitSe(void) {
	SE temp[ToInt(SeType::Count)] = {
		//Sample
		{Novice::LoadAudio("./NoviceResources/fanfare.wav"), 1.0f},
	};

	memcpy(ses, temp, sizeof(ses));
}

void PlaySe(SeType type) {
	ses[ToInt(type)].pHandle = Novice::PlayAudio(ses[ToInt(type)].aHandle, false, ses[ToInt(type)].volume * seVolume);
}

/// <summary>
/// Seのループ再生処理
/// </summary>
/// <param name="type">Seの種類</param>
void PlaySeContinuous(SeType type) {
	if (Novice::IsPlayingAudio(ses[ToInt(type)].pHandle) != 1) {
		ses[ToInt(type)].pHandle = Novice::PlayAudio(ses[ToInt(type)].aHandle, true, ses[ToInt(type)].volume * seVolume);
	}
}

/// <summary>
/// 再生中のSEを停止
/// </summary>
/// <param name="type">Seの種類</param>
void StopPlayingSe(SeType type) {
	if (Novice::IsPlayingAudio(ses[ToInt(type)].pHandle) == 1) {
		Novice::StopAudio(ses[ToInt(type)].pHandle);
	}
}

/// <summary>
/// 指定SEが再生中か
/// </summary>
/// <param name="type"></param>
/// <returns></returns>
bool CheckIsPlayingSe(SeType type) {
	//再生中であれば
	if (Novice::IsPlayingAudio(ses[ToInt(type)].pHandle) == 1) {
		return true;
	} else {
		return false;
	}
}