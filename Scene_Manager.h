#pragma once

enum class Scene {
	//タイトル
	Title,
	//ステージ(本編)
	Play,
	//ポーズ
	Pause,

	Count
};

enum class ModeFade {
	Standby,
	//徐々に現れる処理
	FadeIn,
	//徐々に暗くなる処理
	FadeOut,
};

//フェード後のシーン
enum class FadeTarget {
	Title,
	Play,
};

/// <summary>
/// 現在シーンの初期化処理
/// </summary>
void InitScene(void);

/// <summary>
/// 現在シーンの更新処理
/// </summary>
void UpdateScene(void);

/// <summary>
/// 現在シーンの描画処理
/// </summary>
void DrawScene(void);

/// <summary>
/// シーン変更
/// </summary>
/// <param name="nextScene">次のシーン</param>
void ChangeScene(Scene nextScene);

/// <summary>
/// 初期化なしのシーン変更
/// </summary>
/// <param name="nextScene"></param>
void ChangeSceneImmediate(Scene nextScene);

/// <summary>
/// 現在のフェードパネルのモード確認
/// </summary>
/// <returns>現在のモード</returns>
ModeFade GetNowModeFade(void);

/// <summary>
/// フェードパネルのモード変更
/// </summary>
/// <param name="mode">次のモード</param>
void ChangeModeFade(ModeFade mode);

/// <summary>
/// フェード先のシーン変更
/// </summary>
/// <param name="target">フェード先のシーン</param>
void ChangeFadeTarget(FadeTarget target);