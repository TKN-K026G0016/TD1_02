#pragma once
#include "vector2.h"
#include "timer.h"
#include <Novice.h>

/// テクスチャの登録から描画までの手順
/// 
/// 0. 使いたいファイルで #include "texture.h" をする
/// 
/// 1. texture.hのTextureTypeにテクスチャの名前を追加(他と混ざらないように)
/// 
/// 2. texture.cppのTextureInit内に例に沿って中身を記述(テクスチャ同士の順番に気を付けて)
/// 
/// 3. 使いたいファイルで変数を宣言
///		static Texture sample1Texture = {};
/// 
/// 4. 使いたいファイルのInit内で初期化
///		sample1Texture = GetTexture(TextureType::Sample1);
/// 
/// 5. 使いたいファイルのDraw内で描画(用途によって関数を使い分ける)
///		DrawTextureObj(sample1Texture, pos, size);

//テクスチャの種類
enum class TextureType {
	//===============
	//Common
	//===============
	White1x1,

	//===============
	//Title
	//===============


	//===============
	//Play
	//===============
	//Player
	PlayerBody,
	PlayerArm,

	//Enemy
	BossBodyForm1,

	//MapChip
	SampleMapChip,

	//background

	//UI

	//===============
	//Gameover
	//===============

	Count
};

//テクスチャデータ
struct Texture {
	int tHandle = -1;
	Vector2i refSize = { 0, 0 };
	//<animation>
	Timer animChangeTimer{ 0 };
	//アニメーションのコマ数
	int animLimit = 0;
	//現在のコマ数
	int animNum = 0;
	//色関係
	unsigned int color = WHITE;
};

/// <summary>
/// テクスチャの読み込み処理
/// </summary>
void InitTexture(void);

/// <summary>
/// テクスチャデータの取得
/// </summary>
/// <param name="type">テクスチャの種類</param>
/// <returns>テクスチャデータ</returns>
Texture GetTexture(TextureType type);


void DrawTextureAsObj(Texture texture, Vector2 pos, Vector2 size, float rotateTheta, bool reverseSwitch);
void DrawTextureAsUI(Texture texture, Vector2 pos, Vector2 size, float rotateTheta);

void DrawGaugeAsObj(Texture texture, Vector2 centerPos, Vector2 size, float rate);
void DrawGaugeAsUI(Texture texture, Vector2 centerPos, Vector2 size, float rate);

void UpdateAnimation(Texture& texture);