#include "Texture.h"
#include "Timer.h"
#include "Tool.h"
#include "Matrix.h"
#include "Camera.h"

#include <Novice.h>
#include <algorithm>

Texture textures[ToInt(TextureType::Count)] = {};

void InitTexture(void) {
	Texture temp[ToInt(TextureType::Count)] = {

		//サンプル
		//テクスチャデータ, 参照サイズ, animChangeTimer(time, count), animLimit, animNum, color
		//{Novice::LoadTexture("./resources/player/player_body.png"), { 128, 128 }, {0, 0}, 0, 0, WHITE },

		//===============
		//Title
		//===============
		{ Novice::LoadTexture("./NoviceResources/white1x1.png"), { 1, 1 }, {0}, 0, 0, WHITE },

		//===============
		//Title
		//===============


		//===============
		//Play
		//===============
		//Player
		{ Novice::LoadTexture("./resources/texture/player/player_idle.png"), { 32, 64 }, {0}, 0, 0, WHITE },
		{ Novice::LoadTexture("./resources/texture/player/player_arm.png"), { 64, 64 }, {0}, 0, 0, WHITE },

		//Enemy
		{ Novice::LoadTexture("./resources/texture/boss/form1/boss.png"), { 256, 256 }, {0}, 0, 0, WHITE },

		//MapChip
		{ Novice::LoadTexture("./resources/texture/stage/mapchip.png"), { 32, 32 }, {0}, 0, 0, WHITE },

		//background

		//UI

		//===============
		//Gameover
		//===============

	};

	memcpy(textures, temp, sizeof(textures));

}


Texture GetTexture(TextureType type) {
	return textures[ToInt(type)];
}


#pragma region 関数: 描画処理

/// <summary>
/// Objの描画処理
/// </summary>
/// <param name="texture">テクスチャデータ</param>
/// <param name="pos">中心座標</param>
/// <param name="size">サイズ</param>
/// <param name="rotateTheta">回転量</param>
/// <param name="reverseSwitch">判定スイッチ</param>
void DrawTextureAsObj(Texture texture, Vector2 pos, Vector2 size, float rotateTheta, bool reverseSwitch) {
	Matrix3x3 worldMatrix = MakeWorldMatrix({ 1.0f,1.0f }, rotateTheta, pos);
	Matrix3x3 wvpVpMatrix = Multiply(worldMatrix, GetWorldToScreenMatrix());

	//ローカル頂点座標
	Vector2 localVertex[kVertexNum];
	//ワールド頂点座標
	Vector2 vertexW[kVertexNum];
	//スクリーン頂点座標
	Vector2i vertexS[kVertexNum];

	//ローカル頂点座標作成
	SetVertex({ 0, 0 }, size, 0.0f, localVertex);
	//スクリーン座標に変換
	for (int i = 0; i < kVertexNum; i++) {
		vertexW[i] = Transform(localVertex[i], wvpVpMatrix);
		vertexS[i] = ConvertVector2(vertexW[i]);
	}

	//通常
	if (!reverseSwitch) {
		Novice::DrawQuad(
			vertexS[0].x, vertexS[0].y,
			vertexS[1].x, vertexS[1].y,
			vertexS[2].x, vertexS[2].y,
			vertexS[3].x, vertexS[3].y,
			texture.refSize.x * texture.animNum, 0, texture.refSize.x, texture.refSize.y,
			texture.tHandle, texture.color
		);
	}
	//逆転
	else {
		Novice::DrawQuad(
			vertexS[0].x, vertexS[0].y,
			vertexS[1].x, vertexS[1].y,
			vertexS[2].x, vertexS[2].y,
			vertexS[3].x, vertexS[3].y,
			texture.refSize.x * (texture.animNum + 1), 0, -texture.refSize.x, texture.refSize.y,
			texture.tHandle, texture.color
		);
	}

}

/// <summary>
/// HUDの描画処理
/// </summary>
/// <param name="texture">テクスチャデータ</param>
/// <param name="pos">中心座標</param>
/// <param name="size">サイズ</param>
/// <param name="rotateTheta">回転量</param>
void DrawTextureAsUI(Texture texture, Vector2 pos, Vector2 size, float rotateTheta) {
	Matrix3x3 worldMatrix = MakeWorldMatrix({ 1.0f,1.0f }, rotateTheta, pos);

	//ローカル頂点座標
	Vector2 localVertex[kVertexNum];
	//ワールド頂点座標
	Vector2 vertexW[kVertexNum];
	//スクリーン頂点座標
	Vector2i vertexS[kVertexNum];

	//ローカル頂点座標作成
	SetVertex({ 0, 0 }, size, 0.0f, localVertex);
	for (int i = 0; i < kVertexNum; i++) {
		vertexW[i] = Transform(localVertex[i], worldMatrix);
		vertexS[i] = ConvertPosWToSUI(vertexW[i]);
	}

	Novice::DrawQuad(
	vertexS[0].x, vertexS[0].y,
	vertexS[1].x, vertexS[1].y,
	vertexS[2].x, vertexS[2].y,
	vertexS[3].x, vertexS[3].y,
	texture.refSize.x * texture.animNum, 0, texture.refSize.x, texture.refSize.y,
	texture.tHandle, texture.color
	);
}

/// <summary>
/// ゲージの描画処理(Obj)
/// </summary>
/// <param name="texture">テクスチャデータ</param>
/// <param name="centerPos">中心座標</param>
/// <param name="size">サイズ</param>
/// <param name="rate">ゲージの割合</param>
void DrawGaugeAsObj(Texture texture, Vector2 centerPos, Vector2 size, float rate) {
	if (rate <= 0.0f) return;

	//割合に補正を掛ける
	rate = std::clamp(rate, 0.0f, 1.0f);
	float gaugeWidth = size.x * rate;
	int gaugeRefWidth = ToInt(texture.refSize.x * rate);

	//ワールド行列作成
	Matrix3x3 worldMatrix = MakeWorldMatrix({ 1.0f,1.0f }, 0.0f, centerPos);
	Matrix3x3 wvpVpMatrix = Multiply(worldMatrix, GetWorldToScreenMatrix());

	Vector2 localVertex[kVertexNum] = {};
	Vector2 vertexW[kVertexNum] = {};
	Vector2i vertexS[kVertexNum] = {};
	//ローカル座標生成
	SetVertex({ 0, 0 }, size, 0.0f, localVertex);
	//右側の頂点をrateに合わせてx座標更新
	localVertex[1].x = localVertex[0].x + gaugeWidth;
	localVertex[3].x = localVertex[2].x + gaugeWidth;

	for (int i = 0; i < kVertexNum; i++) {
		//ワールド座標作成
		vertexW[i] = Transform(localVertex[i], wvpVpMatrix);
		//スクリーン座標に変更
		vertexS[i] = ConvertVector2(vertexW[i]);
	}

	Novice::DrawQuad(
		vertexS[0].x, vertexS[0].y,
		vertexS[1].x, vertexS[1].y,
		vertexS[2].x, vertexS[2].y,
		vertexS[3].x, vertexS[3].y,
		 texture.refSize.x * texture.animNum, 0, gaugeRefWidth, texture.refSize.y,
		texture.tHandle, texture.color
	);
}

/// <summary>
/// ゲージの描画処理(HUD)
/// </summary>
/// <param name="texture">テクスチャデータ</param>
/// <param name="centerPos">中心座標</param>
/// <param name="size">サイズ</param>
/// <param name="rate">ゲージの割合</param>
void DrawGaugeAsUI(Texture texture, Vector2 centerPos, Vector2 size, float rate) {
	if (rate <= 0.0f) return;

	//割合に補正を掛ける
	rate = std::clamp(rate, 0.0f, 1.0f);
	float gaugeWidth = size.x * rate;
	int gaugeRefWidth = ToInt(texture.refSize.x * rate);

	//ワールド行列作成
	Matrix3x3 worldMatrix = MakeWorldMatrix({ 1.0f,1.0f }, 0.0f, centerPos);

	Vector2 localVertex[kVertexNum] = {};
	Vector2 vertexW[kVertexNum] = {};
	Vector2i vertexS[kVertexNum] = {};
	//ローカル座標生成
	SetVertex({ 0, 0 }, size, 0.0f, localVertex);
	//右側の頂点をrateに合わせてx座標更新
	localVertex[1].x = localVertex[0].x + gaugeWidth;
	localVertex[3].x = localVertex[2].x + gaugeWidth;

	for (int i = 0; i < kVertexNum; i++) {
		//ワールド座標作成
		vertexW[i] = Transform(localVertex[i], worldMatrix);
		//スクリーン座標に変更
		vertexS[i] = ConvertPosWToSUI(vertexW[i]);
	}

	Novice::DrawQuad(
		vertexS[0].x, vertexS[0].y,
		vertexS[1].x, vertexS[1].y,
		vertexS[2].x, vertexS[2].y,
		vertexS[3].x, vertexS[3].y,
		 texture.refSize.x * texture.animNum, 0, gaugeRefWidth, texture.refSize.y,
		texture.tHandle, texture.color
	);
}

#pragma endregion

/// <summary>
/// アニメーション変更処理(通常)
/// </summary>
/// <param name="texture">テクスチャデータ</param>
void UpdateAnimation(Texture& texture) {
	texture.animChangeTimer.CountUp();
	if (texture.animChangeTimer.IsReady()) {
		texture.animChangeTimer.Reset();

		if (texture.animNum < texture.animLimit) {
			texture.animNum++;
		} else {
			texture.animNum = 0;
		}

	}
}