#include <Novice.h>
#include "input.h"
#include "texture.h"
#include "se.h"
#include "bgm.h"
#include "tool.h"
#include "common.h"
#include "scene_manager.h"

const char kWindowTitle[] = "LC1A_14_タカノハヤト_タイトル";

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// ライブラリの初期化
	Novice::Initialize(kWindowTitle, kWindowSize.x, kWindowSize.y);

	//// キー入力結果を受け取る箱
	//char keys[256] = {0};
	//char preKeys[256] = {0};

	///
	/// ↓初期化処理ここから
	///

	InitTool();
	InitTexture();
	InitSe();
	InitBgm();
	InitScene();

	///
	/// ↑初期化処理ここまで
	///

	// ウィンドウの×ボタンが押されるまでループ
	while (Novice::ProcessMessage() == 0) {
		// フレームの開始
		Novice::BeginFrame();

		//// キー入力を受け取る
		//memcpy(preKeys, keys, 256);
		//Novice::GetHitKeyStateAll(keys);

		///
		/// ↓更新処理ここから
		///

		UpdateInput();
		UpdateScene();
		UpdateBgm();

		///
		/// ↑更新処理ここまで
		///

		///
		/// ↓描画処理ここから
		///

		DrawScene();

		///
		/// ↑描画処理ここまで
		///

		// フレームの終了
		Novice::EndFrame();

		//// ESCキーが押されたらループを抜ける
		//if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) 
		//{
		//	break;
		//}
	}

	// ライブラリの終了
	Novice::Finalize();
	return 0;
}
