#pragma once

/// Bgmの登録から再生までの手順
/// 
/// 0. 使いたいファイルで #include "Bgm.h" をする
/// 
/// 1. Bgm.hのBgmTypeにBgmの名前を追加(他と混ざらないように)
/// 
/// 2. Bgm.cppのBgmInit内に例に沿って中身を記述(Bgm同士の順番に気を付けて)
/// 
/// 3. PlayBgm()で再生(用途によって関数を使い分ける)
///		PlayBgm(BgmType:: sample);

enum class BgmType {
	//BGMなし
	None,

	Sample,

	Count
};

enum class ModeBgmFade {
	Standby,
	//フェード後に曲変更(次の曲はChangeNextBgmTypeで宣言)
	FadeAndChange,
	//フェードアウト
	FadeOut
};

void InitBgm(void);

void UpdateBgm(void);

/// <summary>
/// BGM再生
/// </summary>
/// <param name="type">次のBGM</param>
void PlayBgm(BgmType type);

/// <summary>
/// 再生中のBGM一時停止
/// </summary>
void PauseBgm(void);

/// <summary>
/// 一時停止中のBGM再生
/// </summary>
void ResumeBgm(void);

/// <summary>
/// BGMフェードモード変更
/// </summary>
/// <param name="mode">フェードモード</param>
void ChangeModeBgmFade(ModeBgmFade mode);

/// <summary>
/// フェード後の次の曲再生
/// </summary>
/// <param name="type">次の曲</param>
void ChangeNextBgmType(BgmType type);