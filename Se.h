#pragma once

/// Seの登録から再生までの手順
/// 
/// 0. 使いたいファイルで #include "se.h" をする
/// 
/// 1. se.hのSeTypeにSeの名前を追加(他と混ざらないように)
/// 
/// 2. se.cppのSeInit内に例に沿って中身を記述(Se同士の順番に気を付けて)
/// 
/// 3. PlaySe()で再生(用途によって関数を使い分ける)
///		PlaySe(SeType:: sample);

enum class SeType {
	Sample,

	Count
};

void InitSe(void);

void PlaySe(SeType type);

void PlaySeContinuous(SeType type);

void StopPlayingSe(SeType type);

bool CheckIsPlayingSe(SeType type);