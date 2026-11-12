#include "bgm.h"
#include "timer.h"
#include "tool.h"

#include <Novice.h>



struct BGM {
	int aHandle = -1;
	float volume = 1.0f;
};

#pragma region 音量
float bgmVolume = 1.0f;
float bgmVolumeMax = 1.0f;

#pragma endregion

static BGM bgms[ToInt(BgmType::Count)] = {};
//現在再生中のBGM
static BgmType currentType = BgmType::None;
//フェード後に再生するBGM
static BgmType nextType = BgmType::None;

//再生中フラグ
static int playHandle = -1;

struct BGMFader {
	ModeBgmFade nowMode = ModeBgmFade::Standby;

	float fadeSpeed = 0.01f;
	float fadeOutSpeed = 0.005f;

	Timer waitPlayTimer{120};
};
BGMFader bgmFader;

static void BgmFade(void) {
	switch (bgmFader.nowMode) {
	case ModeBgmFade::FadeOut: {
		if (bgmVolume > 0.0f) {
			bgmVolume -= bgmFader.fadeOutSpeed;
			Novice::SetAudioVolume(playHandle, bgmVolume * bgms[ToInt(currentType)].volume);
		} else {
			bgmVolume = bgmVolumeMax;
			PlayBgm(BgmType::None);

			bgmFader.nowMode = ModeBgmFade::Standby;
		}
		break;
	}
	case ModeBgmFade::FadeAndChange: {
		if (bgmVolume > 0.0f) {
			bgmVolume -= bgmFader.fadeSpeed;
			Novice::SetAudioVolume(playHandle, bgmVolume * bgms[ToInt(currentType)].volume);
		} else {
			//しばらく時間をおく
			bgmFader.waitPlayTimer.CountUp();
			if (bgmFader.waitPlayTimer.IsReady()) {
				//音声を切替
				bgmVolume = bgmVolumeMax;
				PlayBgm(nextType);

				bgmFader.waitPlayTimer.Reset();
				bgmFader.nowMode = ModeBgmFade::Standby;
			}
		}
		break;
	}
	}
}

void InitBgm(void) {
	BGM temp[ToInt(BgmType::Count)] = {
		{0, 0.0f},

		//sample
		{Novice::LoadAudio("./NoviceResources/mokugyo.wav"), 1.0f},
	};

	memcpy(bgms, temp, sizeof(bgms));
}

void UpdateBgm(void) {
	BgmFade();
}

void PlayBgm(BgmType type) {
	//同じBGMの場合スキップ
	if (currentType == type) {
		return;
	}

	//Noneが指定されたら、曲を停止
	if (type == BgmType::None) {
		Novice::StopAudio(playHandle);
		currentType = type;
		return;
	}

	//違うBGMの場合、いったん曲を変更
	if (playHandle != -1) {
		Novice::StopAudio(playHandle);
	}
	playHandle = Novice::PlayAudio(bgms[ToInt(type)].aHandle, true, bgms[ToInt(type)].volume * bgmVolume);
	currentType = type;
}

void PauseBgm(void) {
	Novice::PauseAudio(playHandle);
}

void ResumeBgm(void) {
	Novice::ResumeAudio(playHandle);
}

void ChangeModeBgmFade(ModeBgmFade mode) {
	bgmFader.nowMode = mode;
}

void ChangeNextBgmType(BgmType type) {
	nextType = type;
}
