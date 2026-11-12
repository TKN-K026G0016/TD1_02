#include "input.h"
#include "vector2.h"

#include <Novice.h>

#pragma region データ: コンフィグ

#pragma region ゲームパッド関係

//Aボタン
constexpr PadButton kGamePadAButton = kPadButton10;
//Bボタン
constexpr PadButton kGamePadBButton = kPadButton11;
//STARTボタン
constexpr PadButton kGamePadStartButton = kPadButton4;

//十字キー(上)
constexpr PadButton kGamePadUpButton = kPadButton0;
//十字キー(下)
constexpr PadButton kGamePadDownButton = kPadButton1;
//十字キー(左)
constexpr PadButton kGamePadLeftButton = kPadButton2;
//十字キー(右)
constexpr PadButton kGamePadRightButton = kPadButton3;

//スティックのニュートラル状態
static constexpr int kStickNeutralValue = 0x0000;
//スティックの非反応領域
static constexpr int kStickDeadZone = 700;

#pragma endregion

#pragma region マウス関係

static constexpr int kMouseButtonL = 0;
static constexpr int kMouseButtonR = 1;

float mouseDeadRadius = 8.0f;

#pragma endregion

#pragma endregion

#pragma region データ: 入力状況

#pragma region ゲームパッド関係

enum class StickDirection {
	Left,
	Right,
	Up,
	Down
};

//左スティックの入力状況
static  Vector2i stickInputL = { 0,0 };
//右スティックの入力状況
static Vector2i stickInputR = { 0,0 };
//左スティックの入力状況(1F前)
static Vector2i preStickInputL = { 0,0 };
//右スティックの入力状況(1F前)
static Vector2i preStickInputR = { 0,0 };

//ゲームパッドの接続フラグ
bool isConnectGamePad = false;

#pragma endregion

#pragma region キーボード関係
// キー入力結果を受け取る箱
char keys[256] = { 0 };
char preKeys[256] = { 0 };

static Vector2i cursorPos = { 0, 0 };

#pragma endregion

#pragma endregion

/// <summary>
/// マウスカーソルの座標を取得
/// </summary>
static void GetMouseCursorPos(void) {
	Novice::GetMousePosition(&cursorPos.x, &cursorPos.y);
}

/// <summary>
/// ゲームパッドの接続状況取得
/// </summary>
/// <param name=""></param>
static void CheckConnectGamePad(void) {
	//接続された状態時、
	if (Novice::GetNumberOfJoysticks() > 0 && !isConnectGamePad) {
		isConnectGamePad = true;

		//接続サウンド
		/*PlaySe(SeType::MoveCursorVoice);*/

	}
	//外された状態時
	else if (Novice::GetNumberOfJoysticks() <= 0 && isConnectGamePad) {
		isConnectGamePad = false;
	}
}

/// <summary>
/// スティックの入力状況取得
/// </summary>
/// <param name=""></param>
static void GetInputGamePadStick(void) {
	//左スティック
	preStickInputL = stickInputL;
	Novice::GetAnalogInputLeft(0, &stickInputL.x, &stickInputL.y);

	//右スティック
	preStickInputR = stickInputR;
	Novice::GetAnalogInputRight(0, &stickInputR.x, &stickInputR.y);
}

/// <summary>
/// スティックが指定方向に倒されているか
/// </summary>
/// <param name="stickValue">確認したいスティックの値</param>
/// <param name="direction">確認したい方向</param>
static bool IsStickPassedDeadZone(Vector2i stickValue, StickDirection direction) {
	switch (direction) {
	case StickDirection::Left:

		if (stickValue.x <= kStickNeutralValue - kStickDeadZone) {
			return true;
		}

		break;
	case StickDirection::Right:

		if (stickValue.x >= kStickNeutralValue + kStickDeadZone) {
			return true;
		}

		break;
	case StickDirection::Up:

		if (stickValue.y <= kStickNeutralValue - kStickDeadZone) {
			return true;
		}

		break;
	case StickDirection::Down:

		if (stickValue.y >= kStickNeutralValue + kStickDeadZone) {
			return true;
		}

		break;
	}


	return false;
}


/// <summary>
/// 入力状況の更新
/// </summary>
void UpdateInput(void) {
	//キーボード
	memcpy(preKeys, keys, 256);
	Novice::GetHitKeyStateAll(keys);

	//マウスのカーソル位置
	GetMouseCursorPos();

	//コントローラーの接続状況
	CheckConnectGamePad();
	if (isConnectGamePad) {
		//スティックの入力状況
		GetInputGamePadStick();
	}
}

/// <summary>
/// 指定アクションの入力状況を返す
/// </summary>
/// <param name="action">InputAction</param>
/// <returns></returns>
bool CheckInputAction(InputAction action) {
	switch (action) {
	case InputAction::SelectLeft: {
		//キーボード
		if (!isConnectGamePad) {

			if (keys[DIK_A] && !preKeys[DIK_A]) {
				return true;
			} else {
				return false;
			}

		}
		//ゲームパッド
		else {
			if (Novice::IsTriggerButton(0, kGamePadLeftButton) ||
				(IsStickPassedDeadZone(stickInputL, StickDirection::Left) && !IsStickPassedDeadZone(preStickInputL, StickDirection::Left))) {
				return true;
			} else {
				return false;
			}
		}

		break;
	}

	case InputAction::SelectRight: {
		//キーボード
		if (!isConnectGamePad) {

			if (keys[DIK_D] && !preKeys[DIK_D]) {
				return true;
			} else {
				return false;
			}

		}
		//ゲームパッド
		else {
			if (Novice::IsTriggerButton(0, kGamePadRightButton) ||
				(IsStickPassedDeadZone(stickInputL, StickDirection::Right) && !IsStickPassedDeadZone(preStickInputL, StickDirection::Right))) {
				return true;
			} else {
				return false;
			}
		}

		break;
	}

	case InputAction::SelectUp: {
		//キーボード
		if (!isConnectGamePad) {

			if (keys[DIK_W] && !preKeys[DIK_W]) {
				return true;
			} else {
				return false;
			}

		}
		//ゲームパッド
		else {
			if (Novice::IsTriggerButton(0, kGamePadUpButton) ||
				(IsStickPassedDeadZone(stickInputL, StickDirection::Up) && !IsStickPassedDeadZone(preStickInputL, StickDirection::Up))) {
				return true;
			} else {
				return false;
			}
		}

		break;
	}

	case InputAction::SelectDown: {
		//キーボード
		if (!isConnectGamePad) {

			if (keys[DIK_S] && !preKeys[DIK_S]) {
				return true;
			} else {
				return false;
			}

		}
		//ゲームパッド
		else {
			if (Novice::IsTriggerButton(0, kGamePadDownButton) ||
				(IsStickPassedDeadZone(stickInputL, StickDirection::Down) && !IsStickPassedDeadZone(preStickInputL, StickDirection::Down))) {
				return true;
			} else {
				return false;
			}
		}

		break;
	}

	case InputAction::Confirm: {

		//キーボード
		if (!isConnectGamePad) {
			if ((keys[DIK_RETURN] && !preKeys[DIK_RETURN])
				|| (keys[DIK_SPACE] && !preKeys[DIK_SPACE])) {
				return true;
			} else {
				return false;
			}
		}
		//ゲームパッド
		else {
			if (Novice::IsTriggerButton(0, kGamePadAButton) || Novice::IsTriggerButton(0, kGamePadStartButton)) {
				return true;
			} else {
				return false;
			}
		}

		break;
	}

	case InputAction::Cancel: {

		//キーボード
		if (!isConnectGamePad) {
			if (keys[DIK_ESCAPE] && !preKeys[DIK_ESCAPE]) {
				return true;
			} else {
				return false;
			}
		}
		//ゲームパッド
		else {
			if (Novice::IsTriggerButton(0, kGamePadBButton) || Novice::IsTriggerButton(0, kGamePadBButton)) {
				return true;
			} else {
				return false;
			}
		}

		break;
	}

	case InputAction::Pause: {

		//キーボード
		if (!isConnectGamePad) {
			if (keys[DIK_ESCAPE] && !preKeys[DIK_ESCAPE]) {
				return true;
			} else {
				return false;
			}
		}
		//ゲームパッド
		else {
			if (Novice::IsTriggerButton(0, kGamePadStartButton) || Novice::IsTriggerButton(0, kGamePadStartButton)) {
				return true;
			} else {
				return false;
			}
		}

		break;
	}
	}

	return false;
}

/// <summary>
/// ゲームパッドの接続状況取得
/// </summary>
/// <returns>isConnectGamePad</returns>
bool GetIsConnectGamePad(void) {
	return isConnectGamePad;
}