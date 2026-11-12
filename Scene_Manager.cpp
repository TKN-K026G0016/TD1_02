#include "Scene_Manager.h"
#include "Texture.h"
#include "Tool.h"
#include "Common.h"

#include "Title.h"
#include "Play.h"
#include "Pause.h"

#pragma region データ: シーン関係

// 初期化関数の配列
static constexpr void (*InitFuncs[ToInt(Scene::Count)])(void) = {
	InitTitle,
	InitPlay,
	InitPause,
};

// アップデート関数の配列
static constexpr void (*UpdateFuncs[ToInt(Scene::Count)])(void) = {
	UpdateTitle,
	UpdatePlay,
	UpdatePause,
};

// 描画関数の配列
static constexpr void (*DrawFuncs[ToInt(Scene::Count)])(void) = {
	DrawTitle,
	DrawPlay,
	DrawPause,
};

//現在のシーン
static Scene nowScene = Scene::Title;

#pragma endregion

#pragma region データ: フェード関係

struct FadePanel {
	Vector2 pos = kWindowCenter;

	Vector2 size = ConvertVector2(kWindowSize);

	//透明度
	int fadeAlpha = 0;

	//現在のモード
	ModeFade nowFadeMode = ModeFade::Standby;
	ModeFade preFadeMode = nowFadeMode;
	//フェード先
	FadeTarget fadeTarget = FadeTarget::Title;
};
FadePanel fadePanel;
Texture fadePanelTexture = {};

//フェードの速度
int fadeSpeed = 7;
//アルファ値の上限値
int alphaMax = 255;
//アルファ値の下限値
int alphaMin = 0;

//フェードアウト完了後の待機時間
Timer waitChangeSceneTimer{60};

#pragma endregion

#pragma region 関数: フェード関係

static void GetTextureFadePanel(void) {
	if (fadePanelTexture.tHandle == -1) {
		fadePanelTexture = GetTexture(TextureType::White1x1);
	}
}

static void MoveFadePanel(void) {
	switch (fadePanel.nowFadeMode) {
	case ModeFade::FadeOut:
	{
		//初期化
		if (fadePanel.preFadeMode != fadePanel.nowFadeMode) {
			fadePanel.fadeAlpha = 0;

			fadePanel.preFadeMode = fadePanel.nowFadeMode;
		}

		fadePanel.fadeAlpha += fadeSpeed;
		if (fadePanel.fadeAlpha >= alphaMax) {
			fadePanel.fadeAlpha = alphaMax;


			//フェード後の処理(リトライorゲームオーバーorクリアorタイトル)
			waitChangeSceneTimer.CountUp();

			if (waitChangeSceneTimer.IsReady()) {

				fadePanel.fadeAlpha = 0;
				fadePanel.nowFadeMode = ModeFade::Standby;
				waitChangeSceneTimer.Reset();

				switch (fadePanel.fadeTarget) {
				case FadeTarget::Title:
					ChangeScene(Scene::Title);
					break;
				case FadeTarget::Play:
					ChangeScene(Scene::Play);

					break;
				}
			}
		}

		//アルファ値に合わせてColorを変更
		fadePanelTexture.color = (0x00000000 | fadePanel.fadeAlpha);

		break;
	}
	case ModeFade::FadeIn: {

		//初期化
		if (fadePanel.preFadeMode != fadePanel.nowFadeMode) {
			fadePanel.fadeAlpha = alphaMax;

			fadePanel.preFadeMode = fadePanel.nowFadeMode;
		}

		fadePanel.fadeAlpha -= fadeSpeed;
		if (fadePanel.fadeAlpha >= alphaMin) {
			fadePanel.fadeAlpha = alphaMin;
			fadePanel.nowFadeMode = ModeFade::Standby;
		}

		break;
	}
	}
}

static void DrawFadePanel(void) {
	if (fadePanel.nowFadeMode == ModeFade::Standby) {
		return;
	}

	//アルファ値に合わせてColorを変更
	fadePanelTexture.color = (0x00000000 | fadePanel.fadeAlpha);

	DrawTextureAsUI(fadePanelTexture, fadePanel.pos, fadePanel.size, 0.0f);
}

#pragma endregion

void InitScene(void) {
	GetTextureFadePanel();

	InitFuncs[ToInt(nowScene)]();
}

void UpdateScene(void) {
	//フェード
	MoveFadePanel();

	UpdateFuncs[ToInt(nowScene)]();
}

void DrawScene(void) {
	if (nowScene == Scene::Pause) {
		DrawPlay();
		DrawPause();
	} else {
		DrawFuncs[ToInt(nowScene)]();
	}

	//フェード
	DrawFadePanel();
}

void ChangeScene(Scene nextScene) {
	nowScene = nextScene;

	InitScene();
}

/// <summary>
/// 初期化処理なしのシーン変更(Pause→Playなどに)
/// </summary>
/// <param name="nextScene"></param>
void ChangeSceneImmediate(Scene nextScene) {
	nowScene = nextScene;
}

/// <summary>
/// 現在のフェード状態を取得
/// </summary>
/// <returns></returns>
ModeFade GetNowModeFade(void) {
	return fadePanel.nowFadeMode;
}

/// <summary>
/// 現在のフェード状態を変更
/// </summary>
/// <returns></returns>
void ChangeModeFade(ModeFade mode) {
	fadePanel.nowFadeMode = mode;
}

/// <summary>
/// フェードアウト後のシーンを変更
/// </summary>
/// <param name="target"></param>
void ChangeFadeTarget(FadeTarget target) {
	fadePanel.fadeTarget = target;
}

#pragma endregion