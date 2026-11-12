#pragma once

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