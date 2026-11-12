#pragma once

/// InputActionの登録手順
/// 1. texture.hのInputActionに新しいアクションの名前を追加(他と混ざらないように)
/// 
/// 2. texture.cppのCheckInputAction内に例に沿って中身を記述(switch文内に新しいcaseを追加する)
/// 

///指定したInputActionの入力状況取得手順
///
/// 1. 入力を検知したいファイルで「#include "Input.h"」する
///
/// 2. 調べたいInputActionを引数に入れたCheckInputAction()をif文の条件にする
/// (ex.)if(CheckInputAction(InputAction:: Jump)){ //ジャンプ処理 }
///

enum class InputAction {
	SelectRight,
	SelectLeft,
	SelectUp,
	SelectDown,

	Confirm,
	Cancel,

	Pause,

	MoveRight,
	MoveLeft,
	MoveUp,
	MoveDown,

	Jump,
	Attack,

	Count,
};

void UpdateInput(void);
bool CheckInputAction(InputAction action);
bool GetIsConnectGamePad(void);