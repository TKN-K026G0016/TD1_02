#include <Novice.h>
#include <cstring>
#include "input.h"
#include "texture.h"
#include "se.h"
#include "bgm.h"
#include "tool.h"
#include "common.h"
#include "scene_manager.h"
#include "AssetEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif


const char kWindowTitle[] = "TD1_1202_タイトル";

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// ライブラリの初期化
	Novice::Initialize(kWindowTitle, kWindowSize.x, kWindowSize.y);
#ifdef USE_IMGUI
	// 初回描画より前に日本語フォントを登録（Novice側のImGuiバックエンドに依存）。
	if (ImGui::GetCurrentContext()) {
		ImGuiIO& io = ImGui::GetIO();
		ImFont* jp = io.Fonts->AddFontFromFileTTF(
			"C:/Windows/Fonts/meiryo.ttc", 17.0f, nullptr,
			io.Fonts->GetGlyphRangesJapanese());
		if (jp) io.FontDefault = jp;
	}
#endif

	//// キー入力結果を受け取る箱
	//char keys[256] = {0};
	//char preKeys[256] = {0};

	///
	/// ↓初期化処理ここから
	///

	// Visual Studioの実行フォルダがDebug等でも、すべてのテクスチャが
	// ゲームプロジェクトのresourcesを基準に読まれるように起動時に一度だけ統一。
	// PNG選択画面ではOFN_NOCHANGEDIRを使うため、実行中は動かさない。
	AssetEditor::FixSettingsLocation();
	const std::string projectRoot = AssetEditor::FullPath(AssetEditor::resourceRoot + "\\..");
	if (AssetEditor::IsDirectory(projectRoot)) SetCurrentDirectoryA(projectRoot.c_str());

	InitTool();
	AssetEditor::Initialize();
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
		AssetEditor::Update(1.0f / 60.0f);

		///
		/// ↑更新処理ここまで
		///

		///
		/// ↓描画処理ここから
		///

		DrawScene();
		AssetEditor::DrawPreview();

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
