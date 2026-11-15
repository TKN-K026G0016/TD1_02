// ============================================================================
// 【このファイルの役割】Scene_Manager.h — シーン種類と遷移ルール
// シーン列挙型や変更関数の宣言。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

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