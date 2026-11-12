#include "pause.h"
#include "input.h"
#include "scene_manager.h"

#include "texture.h"
#include "tool.h"
#include "common.h"

#pragma region データ: カバーパネル

struct CoverPanel {
	Vector2 pos = kWindowCenter;
	Vector2 size = ConvertVector2(kWindowSize);

	unsigned int color = 0x00000099;

	Texture texture = {};
};
static CoverPanel coverPanel = {};

#pragma endregion

#pragma region 宣言: 関数

static void InitCoverPanel(void);
static void DrawCoverPanel(void);

void Resume(void);

#pragma endregion

void InitPause(void) {
	InitCoverPanel();

}

void UpdatePause(void) {
	Resume();
}

void DrawPause(void) {
	DrawCoverPanel();
}

#pragma region 関数: カバーパネル

/// <summary>
/// CoverPanelの初期化処理
/// </summary>
static void InitCoverPanel(void) {
	//テクスチャデータの取得
	coverPanel.texture = GetTexture(TextureType::White1x1);
	coverPanel.texture.color = coverPanel.color;

}

/// <summary>
/// CoverPanelの描画処理
/// </summary>
static void DrawCoverPanel(void) {
	
	DrawTextureAsUI(coverPanel.texture, coverPanel.pos, coverPanel.size, 0.0f);
}

#pragma endregion

#pragma region 関数: 選択関係

/// <summary>
/// ポーズボタンでのポーズ解除処理
/// </summary>
/// <param name=""></param>
void Resume(void) {
	if (CheckInputAction(InputAction::Pause)) {
		ChangeSceneImmediate(Scene::Play);
	}
}

#pragma endregion
