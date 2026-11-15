// ============================================================================
// 【このファイルの役割】Scene_Manager.cpp — 画面遷移の管理
// タイトル・プレイ・ポーズなど現在のシーンを決め、更新・描画を振り分ける。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

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
Timer waitChangeSceneTimer{ 60 };

#pragma endregion

#pragma region 関数: フェード関係

// ------------------------------------------------------------------------
// 【関数】GetTextureFadePanel()
//  現在の状態や計算結果を返すための問い合わせ関数。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
static void GetTextureFadePanel(void) {
	if (fadePanelTexture.tHandle == -1) {
		fadePanelTexture = GetTexture(TextureType::White1x1);
	}
}

// ------------------------------------------------------------------------
// 【関数】MoveFadePanel()
//  この関数が担当する処理を以下の順番で実行する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
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

// ------------------------------------------------------------------------
// 【関数】DrawFadePanel()
//  今ある内部状態を描画へ反映する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
static void DrawFadePanel(void) {
	if (fadePanel.nowFadeMode == ModeFade::Standby) {
		return;
	}

	//アルファ値に合わせてColorを変更
	fadePanelTexture.color = (0x00000000 | fadePanel.fadeAlpha);

	DrawTextureAsUI(fadePanelTexture, fadePanel.pos, fadePanel.size, 0.0f);
}

#pragma endregion

// ------------------------------------------------------------------------
// 【関数】InitScene()
//  画面遷移の管理を初期化する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void InitScene(void) {
	GetTextureFadePanel();

	InitFuncs[ToInt(nowScene)]();
}

// ------------------------------------------------------------------------
// 【関数】UpdateScene()
//  現在のシーンの更新関数を呼ぶ。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void UpdateScene(void) {
	//フェード
	MoveFadePanel();

	UpdateFuncs[ToInt(nowScene)]();
}

// ------------------------------------------------------------------------
// 【関数】DrawScene()
//  現在のシーンの描画関数を呼ぶ。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void DrawScene(void) {
	if (nowScene == Scene::Pause) {
		DrawPlay();
		DrawPause();
	}
	else {
		DrawFuncs[ToInt(nowScene)]();
	}

	//フェード
	DrawFadePanel();
}

// ------------------------------------------------------------------------
// 【関数】ChangeScene()
//  別シーンへの切替要求を出す。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void ChangeScene(Scene nextScene) {
	nowScene = nextScene;

	InitScene();
}

/// <summary>
/// 初期化処理なしのシーン変更(Pause→Playなどに)
/// </summary>
/// <param name="nextScene"></param>
// ------------------------------------------------------------------------
// 【関数】ChangeSceneImmediate()
//  フェードを待たずシーンを変更する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void ChangeSceneImmediate(Scene nextScene) {
	nowScene = nextScene;
}

/// <summary>
/// 現在のフェード状態を取得
/// </summary>
/// <returns></returns>
// ------------------------------------------------------------------------
// 【関数】GetNowModeFade()
//  現在の状態や計算結果を返すための問い合わせ関数。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
ModeFade GetNowModeFade(void) {
	return fadePanel.nowFadeMode;
}

/// <summary>
/// 現在のフェード状態を変更
/// </summary>
/// <returns></returns>
// ------------------------------------------------------------------------
// 【関数】ChangeModeFade()
//  渡された値を内部の状態へ反映するための関数。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void ChangeModeFade(ModeFade mode) {
	fadePanel.nowFadeMode = mode;
}

/// <summary>
/// フェードアウト後のシーンを変更
/// </summary>
/// <param name="target"></param>
// ------------------------------------------------------------------------
// 【関数】ChangeFadeTarget()
// 何をする？ 渡された値を内部の状態へ反映するための関数。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void ChangeFadeTarget(FadeTarget target) {
	fadePanel.fadeTarget = target;
}

#pragma endregion