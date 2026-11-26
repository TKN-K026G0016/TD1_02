#include "AssetEditor.h"
#include <Novice.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ExactPlayerBattle.h"
#include "PlayerTuning.h"
#include "Vector2.h"
#include "BossCombatEditorSettings.h"

// ボスが画面外へ退場したフレームに、ボスGPとの接続だけを解除する。
// 通常の背景GPと、プレイヤーの移動・ジャンプ計算には触れない。
namespace ExactPlayerBattle {
	static bool releaseBossGripsRequested = false;
	static int movingAnchorCount = 0;
}

namespace ExactSampleNonAlternateBattle {
	const int kWindowWidth = 1280;
	const int kWindowHeight = 720;
	const int kPixelScale = 2;                       // 絵の拡大率
	const int kTileSize = 32 * kPixelScale;          // 床・杭の絵1枚の大きさ(px)
	const float kFloorY = 656.0f;                    // 床の上面のY。左右と上の端も壁
	const int kStakeNum = 24;                         // 杭の数(配置は kStakePos)

	struct Vector2 {
		float x;
		float y;
	};

	const char kWindowTitle[] = "触手ウーマン(仮)";

	const float kPi = 3.14159265f;

	//プレイヤー
	const float kPlayerW = 64.0f;
	const float kPlayerH = 128.0f;
	const float kPlayerHalfW = kPlayerW / 2.0f;
	const float kPlayerHalfH = kPlayerH / 2.0f;
	const int kPlayerSpriteW = 32;
	const int kPlayerSpriteH = 64;
	const int kAnimationFrameCount = 6;
	const int kAnimationSpeed = 5;


	// 操作
	const PadButton kPadTentL = kPadButton8;         // L2
	const PadButton kPadTentR = kPadButton9;         // R2
	const PadButton kPadTentL2 = kPadButton14;       // 触手L用の予備ボタン
	const PadButton kPadTentR2 = kPadButton15;       // 触手R用の予備ボタン
	const PadButton kPadJump = kPadButton12;         // X
	const PadButton kPadRelease = kPadButton10;      // A
	const float kStickYSign = 1.0f;                  // スティックを上に倒して狙いが下になるなら -1.0f
	const float kStickDead = 0.25f;                  // スティックのデッドゾーン(0〜1)

	// 触手の色
	const unsigned int kTentColor[2] = { 0xFF78BCFF, 0x62E07AFF };   // 0=L(ピンク) 1=R(緑)

	// 触手とゲート
	const int kTentacleSeg = 96;                     // 触手のパーツ数
	const int kTentacleSpriteW = 16 * kPixelScale;
	const int kTentacleSpriteH = 32 * kPixelScale;
	const float kTentacleBaseHalfSrc = 8.0f;         // 根元の太さ(ドット数・片側)
	const float kTentacleTipHalfSrc = 1.0f;          // 先端の太さ
	const int kTentacleStepRows = 4;                 // 絵を縦に何行ずつ貼るか
	const float kTentacleStep = static_cast<float>(kTentacleStepRows * kPixelScale);
	const int kTentacleMaxSample = 192;
	const int kRetractFrames = 8;
	const int kGateOpenFrames = 6;
	const int kGateCloseFrames = 6;
	const float kGateBackX = 96.0f;
	const float kGateBackY = 112.0f;
	const float kGateFollow = 0.25f;
	const float kGateSplit = 30.0f;                  // 左右のゲートのずらし量(px)
	const int kGateSpriteW = 32 * kPixelScale;
	const int kGateSpriteH = 96 * kPixelScale;
	const float kGateHalfW = kGateSpriteW / 2.0f;
	const float kGateHalfLen = kGateSpriteH / 2.0f;

	const float kKnockFriction = 0.92f;              // 横の反動の減衰率(毎フレーム)
	const int kKnockHoldFrames = 6;                  // 反動が減り始めるまでのフレーム数
	const float kKnockInputRate = 0.25f;             // 反動中の左右入力の効き(1.0で通常)

	// 杭
	const float kGripRadius = 18.0f;                 // 杭の半径
	const int kGripHalfSrc = 2;                      // 巻き付き部分の触手の太さ(目安)
	const float kGripWireHalf = static_cast<float>(kGripHalfSrc * kPixelScale);   // 巻き付き部分の太さ
	const float kGripCoilGap = 4.0f;                 // 1周ごとに増える半径(px)
	const float kGripTurns = 1.0f;                   // 杭に巻く周数
	const int kGripUnwindFrames = 14;                // 巻き解きにかかるフレーム数
	const float kGripReleaseDist = 40.0f;            // 杭にこの距離まで近づいたら離す(px)
	const int kGripPullMax = 40;                     // 引っ張りの最大フレーム数(壁に引っかかった時用)
	const float kGripLockCos = 0.5f;                 // 自動照準の角度(cos。0.5で左右60度)
	const int kGripMarkSpriteSize = 32 * kPixelScale;
	const float kGripMarkSize = 72.0f;               // 狙い中のマークのサイズ
	const float kGripMarkSizeSub = 44.0f;            // 届く範囲にあるだけの杭のマークのサイズ
	const int kMaxGripCand = 24;                     // 自動照準の候補の最大数

	// 杭の耐久と引きはがし
	const float kStakeHpMax = 120.0f;                // 杭の耐久
	const int kStakeFullReels = 4;                   // 杭が外れる巻き取り回数
	const float kStakeReelRefDist = 200.0f;          // この距離以上から巻き切ると1回分減る。近いと割合で減る
	const float kPeelNeedLen = 400.0f;               // 引きはがしに必要な2つの杭の距離
	const float kPeelReadyPulse = 0.3f;               // 引きはがせる時の輪の脈動速度
	const float kPullBright = 0.35f;                 // 引っ張り中の触手の明るさ(白に寄せる割合)
	const float kPullGlowSpeed = 0.3f;               // 引っ張り中の光の脈動速度
	const float kPullGlowAlphaMin = 60.0f;           // 光の濃さの最小
	const float kPullGlowAlphaMax = 140.0f;          // 光の濃さの最大
	const int kYankGraceFrames = 24;                 // 引っ張った直後にロープへ引っかからない猶予(f)
	const int kStakeRespawnFrames = 600;             // 外れた杭のリポップまでのフレーム数
	const int kStakePopFrames = 24;                  // リポップ演出のフレーム数
	const int kStakeShakeFrames = 6;                 // 耐久が減った時に震えるフレーム数
	const float kStakeShakePx = 3.0f;                // 震える幅
	const float kStakeDamagedTint = 0.25f;           // 耐久が0に近い杭の暗さ
	const int kPeelPieceFrames = 50;                 // 抜けた杭が飛んで消えるまでのフレーム数
	const float kPeelPieceSpeed = 14.0f;             // 抜けた杭の初速(px/f)
	const int kStakePeelMax = 8;                     // 抜けた杭の同時表示数
	const int kMaxPopup = 8;                         // 文字の同時表示数
	const int kPopupFrames = 50;                     // 文字の表示フレーム数

	// 入力方向の矢印
	const float kAimArrowRadius = 96.0f;             // プレイヤー中心から矢印までの距離
	const float kAimArrowSize = 40.0f;               // 矢印のサイズ
	const int kAimArrowSpriteSize = 16 * kPixelScale; // 1コマのサイズ
	const float kAimArrowFollow = 0.35f;             // 矢印が入力方向へ追従する割合(1フレームで残りのどれだけ進むか)

	// 左下・右下のL/R表示
	const int kLRSpriteSize = 32;                    // ui_lr.png 1コマのサイズ
	const int kCycleSpriteSize = 64;                 // ui_cycle.png のサイズ
	const float kLRUiOffsetX = 58.0f;                // プレイヤー中心からL/Rまでの横距離
	const float kLRUiOffsetY = 40.0f;                // プレイヤー中心からL/Rまでの縦距離(下)
	const float kLRUiSize = 36.0f;                   // L/Rのサイズ
	const float kLRRingSize = 72.0f;                 // 回る矢印のサイズ
	const float kLRRingSpeed = 0.12f;                // 矢印の回転速度
	const float kLRRingPullSpeed = 0.28f;            // 引っ張り中の回転速度
	const float kLRRingFade = 0.25f;                 // 矢印の出現・消滅の速さ
	const float kLRMarkSize = 26.0f;
	// 数字スプライト
	const int kNumSpriteW = 7;                       // 1コマの幅
	const int kNumSpriteH = 9;                       // 1コマの高さ
	const int kNumScale = 2;                         // 拡大率
	const int kNumGap = 3;                           // 数字の間隔


	// ひっかけの段階
	const int kGripReach = 0;                        // 伸びてる
	const int kGripHold = 1;                         // 巻き付いてぶら下がり(ロープ)
	const int kGripPull = 2;                         // 引っ張り中(ボタン押しっぱなし)
	const int kGripUnwind = 3;                       // 巻き解いてしまってる

	// 触手セットの状態
	const int kSetOut = 1;                           // 出てる
	const int kSetRetract = 2;                       // 戻り中(ゲートが閉じる)

	// 構造体
	struct Player {
		Vector2 pos;
		Vector2 Velocity;
		bool isFacingRight;
	};

	// 触手1本
	struct Tentacle {
		float len;            // 現在の長さ
		float phase;          // ゆれの位相
		float retractFrom;    // 戻り始めた時の長さ
	};

	// 1回のひっかけ(ゲート1つ+触手1本)。0=L 1=R
	struct TentacleSet {
		bool isActive;
		int state;            // kSetOut / kSetRetract
		int timer;
		Vector2 gatePos;
		Vector2 gateOffset;   // ゲートの位置(プレイヤー中心から)
		Vector2 dir;          // 杭の向き(長さ1)
		float gateOpen;       // ゲートの開き(0〜1)
		int gripPhase;        // kGripReach / kGripHold / kGripPull / kGripUnwind
		int gripTimer;        // 引っ張り開始からのフレーム数
		Vector2 gripPos;      // 杭の中心
		int gripIdx;          // 掴んでる杭の番号(stakesの添字)
		float gripSpin;       // 巻く向き(±1)
		float gripTense;      // 張り具合(0〜1)。張ってる間はゆれない
		Vector2 pullDir;      // 引っ張る向き(離すとこの向きへ飛ぶ)
		float ropeLen;        // ロープの長さ(これ以上離れられない。引っ張ると縮む)
		float pullStartDist;  // 巻き始めの杭までの距離(巻いた割合の計算用)
		float pullProg;       // 巻いた割合(0〜1)。増えた分だけ耐久が減る
		Tentacle t;           // ゲートから出る触手
	};

	// 杭(ひっかけポイント)
	struct Stake {
		Vector2 pos;          // 杭の中心(画面座標)
		float hp;             // 耐久
		int shakeUntil;       // frameCount がこの値になるまで震える
		int goneFrames;       // 外れてからリポップまでの残りフレーム(0なら外れてない)
		int popFrames;        // リポップ演出の残りフレーム
	};

	// 杭の配置(1画面にまばらに。中心座標)。数を変えたら kStakeNum も変える
	const Vector2 kStakePos[kStakeNum] = {
		{ 170.0f, 420.0f },
		{ 420.0f, 230.0f },
		{ 640.0f, 440.0f },
		{ 660.0f, 120.0f },
		{ 900.0f, 260.0f },
		{ 1110.0f, 440.0f },
		{ 1190.0f, 150.0f },
	};

	// 抜けて飛ぶ杭
	struct PeelStake {
		bool on;
		float x;              // ワールド座標
		float y;
		float vx;
		float vy;
		int t;                // 飛び始めてからのフレーム数
	};

	// 画面に出す文字
	struct StakePopup {
		bool on;
		float x;              // ワールド座標(杭の位置)
		float y;
		int t;
		int kind;             // 0:引きはがし成功 1:ロープが短くて失敗 2:巻きで外れた
	};

	// ひっかけ中の触手の経路
	struct GripPath {
		Vector2 gate;      // 根元
		Vector2 center;    // 杭の中心
		Vector2 tangent;   // 渦の始点(接点)
		float spin;        // 巻く向き
		float r0;          // 巻き終わりの半径(杭+触手の太さ)
		float rEntry;      // 巻き始めの半径
		float tauMax;      // 渦の角度の合計
		float thetaT;      // 接点の角度
		float legLen;      // 直線部分の長さ
		float spiralLen;   // 渦部分の長さ
		float total;       // 全長
	};

	// ゲートから杭までの経路を計算(渦の巻き始めの角度も)
	static void BuildGripPath(const Vector2& gate, const Vector2& center, float spin, GripPath& gp) {
		gp.gate = gate;
		gp.center = center;
		gp.spin = spin;
		gp.r0 = kGripRadius + kGripWireHalf;
		gp.tauMax = kGripTurns * 2.0f * kPi;
		// 外側ほど1周ごとに kGripCoilGap ずつ半径が増える
		gp.rEntry = gp.r0 + kGripTurns * kGripCoilGap;

		// ゲートから円の中心へのベクトル
		float ux = gate.x - center.x;
		float uy = gate.y - center.y;
		float d = sqrtf(ux * ux + uy * uy);
		if (d < 0.001f) {
			ux = 1.0f;
			uy = 0.0f;
			d = 0.001f;
		}
		else {
			ux /= d;
			uy /= d;
		}
		// ゲートから円へ引いた接線の接点の角度
		float phi = 0.0f;
		if (d > gp.rEntry) {
			phi = acosf(gp.rEntry / d);
		}
		// 接点の角度 = ゲートから見た円の中心の角度 + 接線の角度
		gp.thetaT = atan2f(uy, ux) + spin * phi;
		// 接点の座標
		gp.tangent.x = center.x + gp.rEntry * cosf(gp.thetaT);
		gp.tangent.y = center.y + gp.rEntry * sinf(gp.thetaT);
		// ゲートから接点までの距離
		float lx = gp.tangent.x - gate.x;
		float ly = gp.tangent.y - gate.y;
		// 直線部分の長さ
		gp.legLen = sqrtf(lx * lx + ly * ly);
		gp.spiralLen = gp.tauMax * (gp.rEntry + gp.r0) * 0.5f;
		gp.total = gp.legLen + gp.spiralLen;
	}

	// 根元から s(px) 進んだ位置
	static Vector2 GripPathPoint(const GripPath& gp, float s) {
		Vector2 p;
		if (s <= gp.legLen) {
			float t = (gp.legLen > 0.001f) ? s / gp.legLen : 1.0f;
			p.x = gp.gate.x + (gp.tangent.x - gp.gate.x) * t;
			p.y = gp.gate.y + (gp.tangent.y - gp.gate.y) * t;
			return p;
		}
		float sp = s - gp.legLen;
		if (sp > gp.spiralLen) {
			sp = gp.spiralLen;
		}
		// 渦の道のり sp から回転角 tau を求める(半径が角度に比例して縮む)
		float a = (gp.rEntry - gp.r0) / (2.0f * gp.tauMax);
		float tau = 0.0f;
		if (a < 0.00001f) {
			tau = sp / gp.rEntry;
		}
		else {
			float disc = gp.rEntry * gp.rEntry - 4.0f * a * sp;
			if (disc < 0.0f) {
				disc = 0.0f;
			}
			tau = (gp.rEntry - sqrtf(disc)) / (2.0f * a);
		}
		float r = gp.rEntry + (gp.r0 - gp.rEntry) * (tau / gp.tauMax);
		float th = gp.thetaT + gp.spin * tau;
		p.x = gp.center.x + r * cosf(th);
		p.y = gp.center.y + r * sinf(th);
		return p;
	}


	void Frame(bool update, bool draw, bool reset, const ::Vector2* anchors, int anchorCount, ExactPlayerBattle::Snapshot& snapshot) {
		static bool initialized = false;

		// Novice is initialized by the existing game.

		static char keys[256] = { 0 };
		static char preKeys[256] = { 0 };

		if (!initialized) srand(static_cast<unsigned int>(time(nullptr)));

		// スプライト読み込み
		int playerIdleTex = AssetEditor::Handle(AssetEditor::PlayerIdle);
		int playerWalkTex = AssetEditor::Handle(AssetEditor::PlayerWalk);
		int playerJumpTex = AssetEditor::Handle(AssetEditor::PlayerJump);
		int playerFallTex = AssetEditor::Handle(AssetEditor::PlayerFall);
		int tentacleTex = AssetEditor::Handle(AssetEditor::Tentacle);
		int gateTex = AssetEditor::Handle(AssetEditor::Gate);
		static int blockTex = Novice::LoadTexture("./resources/texture/player/video_sample/chip_block.png");

		// 自動照準のマーク(無ければ -1 のまま。仮の丸で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int gripMarkTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/grip_mark.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// 入力方向の矢印(無ければ -1。仮の丸で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int aimArrowTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/aim_arrow.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// L/R表示(無ければ -1。仮の丸で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int lrTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/ui_lr.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// L/Rの周りを回る半円矢印2本(無ければ -1。仮の丸枠で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int cycleTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/ui_cycle.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// 数字スプライト(無ければ -1。ScreenPrintf で出す)
		static auto LoadOptionalTexture = [](const char* path) -> int {
			FILE* texFile = nullptr;
			fopen_s(&texFile, path, "rb");
			if (texFile != nullptr) {
				fclose(texFile);
				return Novice::LoadTexture(path);
			}
			return -1;
			};
		static int numTex = LoadOptionalTexture("./resources/texture/player/video_sample/ui_number.png");

		// 杭の絵(無ければ -1。仮の茶色い丸で描く)
		static int stakeTex = LoadOptionalTexture("./resources/texture/player/video_sample/chip_grip.png");

		// 杭の絵を左上(x, y)・サイズ size(px)で描く。絵が無ければ何もせず false を返す(呼ぶ側で仮の丸を描く)
		static auto DrawStakeSprite = [&](int x, int y, int size, unsigned int color) -> bool {
			if (stakeTex < 0) {
				return false;
			}
			Novice::DrawQuad(x, y, x + size, y, x, y + size, x + size, y + size,
				0, 0, kTileSize, kTileSize, stakeTex, color);
			return true;
			};



		// 杭(画面に固定)。引き伸ばすと耐久が減って、0で抜ける
		static Stake stakes[kStakeNum];
		static auto ResetStakes = [&]() {
			for (int i = 0; i < kStakeNum; i++) {
				stakes[i].pos = kStakePos[i];
				stakes[i].hp = kStakeHpMax;
				stakes[i].shakeUntil = 0;
				stakes[i].goneFrames = 0;
				stakes[i].popFrames = 0;
			}
			};
		if (!initialized) ResetStakes();

		// プレイヤー
		static Player player = { {200.0f, kFloorY - kPlayerHalfH}, {0.0f, 0.0f}, true };
		static bool jumpReady = true;                // ジャンプが使えるか(着地・杭にひっかける・杭から離れると回復)

		static int animationFrame = 0;
		static int animationTimer = 0;
		static int playerTex = playerIdleTex;


		// 触手
		static TentacleSet sets[2];                  // 0=L 1=R
		if (!initialized) memset(sets, 0, sizeof(sets));
		static float knockX = 0.0f;                  // 離した時などの横の反動
		static int knockTimer = 0;                   // 反動が減り始めるまでの残りフレーム
		static Vector2 reelPos[2] = { {0.0f, 0.0f}, {0.0f, 0.0f} };   // 各触手が最後に巻いた杭の位置(引きはがしの距離判定用)
		static int reelGrace[2] = { 0, 0 };          // 各触手が巻いた直後の残りフレーム(もう片方のロープが引っかからず外れる猶予)

		// 1フレーム前の入力
		static bool preTentHold[2] = { false, false };
		static bool preJumpHold = false;
		static bool preReleaseHold = false;
		static bool isPadDebug = false;              // F1でコントローラーのボタン番号を表示

		// ひっかけポイント
		static int gripMarkSet = -1;                 // 狙いマークを出してる触手(0:L 1:R -1:なし)
		static int gripCand[kMaxGripCand];           // 自動照準の候補(届く範囲の杭)の番号
		static int gripCandCount = 0;
		static bool isGripLock = false;              // 狙ってる杭があるか
		static int gripLockIdx = 0;                  // 狙ってる杭の番号

		// 入力方向の矢印
		static float aimArrowAng = 0.0f;             // 矢印の現在の角度(プレイヤー中心から)
		static float aimArrowAlpha = 0.0f;           // 矢印の濃さ(0〜1)。入力が無いと消える

		// L/R表示:杭に着いてる間まわりを回る矢印
		static float lrRingAng[2] = { 0.0f, 0.0f };  // 回転角
		static float lrRingFade[2] = { 0.0f, 0.0f }; // 出てる度合い(0〜1)

		// 触手を解除(引っ張り中ならその向きへ飛ぶ)。そのあと巻きを解いてしまう
		static auto UnwindSet = [&](int idx) {
			TentacleSet& us = sets[idx];
			if (us.isActive == false || us.state != kSetOut || us.gripPhase == kGripUnwind) {
				return;
			}
			if (us.gripPhase == kGripPull) {
				player.Velocity.y = us.pullDir.y * PlayerTuning::values.gripLaunchSpeed;
				knockX = us.pullDir.x * PlayerTuning::values.gripLaunchSpeed;
				knockTimer = kKnockHoldFrames;
				jumpReady = true;
			}
			us.gripPhase = kGripUnwind;
			};

		// 画面の左右の端にぶつかるまで横へ動く(ぶつかったら true)
		static auto MoveX = [&](float amount) -> bool {
			player.pos.x += amount;
			if (player.pos.x < kPlayerHalfW) {
				player.pos.x = kPlayerHalfW;
				return true;
			}
			if (player.pos.x > kWindowWidth - kPlayerHalfW) {
				player.pos.x = kWindowWidth - kPlayerHalfW;
				return true;
			}
			return false;
			};

		// 床か天井にぶつかるまで縦へ動く(ぶつかったら true)
		static auto MoveY = [&](float amount) -> bool {
			player.pos.y += amount;
			if (player.pos.y + kPlayerHalfH > kFloorY) {
				player.pos.y = kFloorY - kPlayerHalfH;
				return true;
			}
			if (player.pos.y - kPlayerHalfH < 0.0f) {
				player.pos.y = kPlayerHalfH;
				return true;
			}
			return false;
			};

		// 触手の形(パーツの座標)。更新で計算して描画で使う
		static Vector2 pts[2][kTentacleSeg + 1];
		if (!initialized) memset(pts, 0, sizeof(pts));

		static PeelStake stakePeels[kStakePeelMax];   // 抜けて飛ぶ杭
		if (!initialized) memset(stakePeels, 0, sizeof(stakePeels));
		static int stakePeelNext = 0;                 // 次に使う番号

		static StakePopup popups[kMaxPopup];          // PEEL!! などの文字
		if (!initialized) memset(popups, 0, sizeof(popups));
		static int popupNext = 0;

		// 抜けた杭を飛ばす
		static auto SpawnPeel = [&](float x, float y, float vx, float vy) {
			PeelStake& pk = stakePeels[stakePeelNext];
			pk.on = true;
			pk.t = 0;
			pk.x = x;
			pk.y = y;
			pk.vx = vx;
			pk.vy = vy;
			stakePeelNext = (stakePeelNext + 1) % kStakePeelMax;
			};

		// 杭を外してリポップのカウントを始める
		static int stakeRemovedThisFrame = -1;
		static auto RemoveStake = [&](int idx) {
			stakeRemovedThisFrame = idx;
			stakes[idx].goneFrames = kStakeRespawnFrames;
			stakes[idx].popFrames = 0;
			stakes[idx].hp = kStakeHpMax;
			};

		// 文字を出す
		static auto AddPopup = [&](float x, float y, int kind) {
			StakePopup& pp = popups[popupNext];
			pp.on = true;
			pp.x = x;
			pp.y = y;
			pp.t = 0;
			pp.kind = kind;
			popupNext = (popupNext + 1) % kMaxPopup;
			};

		static int frameCount = 0;


		if (!initialized || reset) {
			player = { {420.0f, kFloorY - kPlayerHalfH}, {0.0f,0.0f}, true };
			jumpReady = true;
			memset(sets, 0, sizeof(sets));
			memset(pts, 0, sizeof(pts));
			memset(stakePeels, 0, sizeof(stakePeels));
			memset(popups, 0, sizeof(popups));
			preTentHold[0] = preTentHold[1] = false;
			preJumpHold = preReleaseHold = false;
			knockX = 0;knockTimer = 0;frameCount = 0;
			reelGrace[0] = reelGrace[1] = 0;
			ResetStakes();
			initialized = true;
		}
		// 画面外へ出たボスに接続中の触手だけ解除する。
		if (update && ExactPlayerBattle::releaseBossGripsRequested) {
			for (int h = 0; h < 2; ++h) {
				if (sets[h].isActive && sets[h].state == kSetOut &&
					sets[h].gripIdx >= ExactPlayerBattle::BossAnchorStart()) {
					sets[h].isActive = false;
					sets[h].gripIdx = -1;
					sets[h].gripTense = 0.0f;
					reelGrace[h] = 0;
				}
			}
		}

		if (update) {
			snapshot.yankTarget = -1;
			snapshot.yankGap = 0.0f;
			for (int h = 0; h < 2; ++h) {
				snapshot.loosenTarget[h] = -1;
				snapshot.loosenDelta[h] = 0.0f;
			}
		}
		if (anchors && anchorCount > 0 && update) {
			for (int i = 0;i < kStakeNum;++i) {
				if (i < anchorCount) {
					// Boss uses a world Y-up coordinate system; sample uses screen Y-down.
					stakes[i].pos = { anchors[i].x, 720.0f - anchors[i].y };
					if (i >= ExactPlayerBattle::BossAnchorStart()) {
						// Boss screws remain available until the armor system removes them.
						stakes[i].goneFrames = (anchors[i].x > 10000.0f) ? 999999 : 0;
					}
				}
				else {
					stakes[i].goneFrames = 999999;
				}
			}
			// 第2ステージの流れる背景GPは触手の接続位置も追従させる。
			// 画面端でGPが反対側へループした場合、触手だけ安全に解除する。
			for (int h = 0; h < 2; ++h) {
				const int idx = sets[h].gripIdx;
				if (!sets[h].isActive || idx < 0 || idx >= anchorCount ||
					sets[h].gripPhase == kGripUnwind) continue;
				if (idx < ExactPlayerBattle::movingAnchorCount) {
					const Vector2 newPos = stakes[idx].pos;
					const float dx = newPos.x - sets[h].gripPos.x;
					const float dy = newPos.y - sets[h].gripPos.y;
					// 一瞬で大きく座標が変わるのはGPのループか消滅。
					if (newPos.x > 10000.0f || dx * dx + dy * dy > 200.0f * 200.0f) {
						sets[h].isActive = false;
						sets[h].gripIdx = -1;
						sets[h].gripTense = 0.0f;
						reelGrace[h] = 0;
					}
					else {
						sets[h].gripPos = newPos;
					}
				}
				else if (idx >= ExactPlayerBattle::BossAnchorStart()) {
					// ボスGPは従来通り機体に追従。
					sets[h].gripPos = stakes[idx].pos;
				}
			}
		}
		{



			if (update) {
				stakeRemovedThisFrame = -1;
				memcpy(preKeys, keys, 256);
				Novice::GetHitKeyStateAll(keys);

				///
				/// ↓更新処理ここから
				///
				frameCount++;

				// コントローラー(未接続ならスティック0・ボタン未押下)
				float padX = 0.0f;
				float padY = 0.0f;
				{
					int stickRawX = 0;
					int stickRawY = 0;
					Novice::GetAnalogInputLeft(0, &stickRawX, &stickRawY);
					padX = stickRawX / 32768.0f;
					padY = stickRawY / 32768.0f * kStickYSign;
					if (sqrtf(padX * padX + padY * padY) < kStickDead) {
						padX = 0.0f;
						padY = 0.0f;
					}
				}

				// 操作(キーボードとパッドをまとめる)
				bool tentHold[2];
				tentHold[0] = (keys[DIK_Q] != 0 || keys[DIK_J] != 0 || Novice::IsPressButton(0, kPadTentL) || Novice::IsPressButton(0, kPadTentL2));
				tentHold[1] = (keys[DIK_E] != 0 || keys[DIK_K] != 0 || Novice::IsPressButton(0, kPadTentR) || Novice::IsPressButton(0, kPadTentR2));
				bool jumpHold = (keys[DIK_SPACE] != 0 || Novice::IsPressButton(0, kPadJump));
				bool releaseHold = (keys[DIK_LSHIFT] != 0 || keys[DIK_RSHIFT] != 0 || Novice::IsPressButton(0, kPadRelease));
				float inX = padX;   // 移動と狙い
				float inY = padY;
				if (keys[DIK_D] != 0) { inX += 1.0f; }
				if (keys[DIK_A] != 0) { inX -= 1.0f; }
				if (keys[DIK_S] != 0) { inY += 1.0f; }
				if (keys[DIK_W] != 0) { inY -= 1.0f; }
				{
					float inLen = sqrtf(inX * inX + inY * inY);
					if (inLen > 1.0f) {
						inX /= inLen;
						inY /= inLen;
					}
				}
				if (keys[DIK_F1] != 0 && preKeys[DIK_F1] == 0) {
					isPadDebug = !isPadDebug;
				}
				// 押した瞬間
				bool tentPress[2];
				for (int h = 0; h < 2; h++) {
					tentPress[h] = (tentHold[h] == true && preTentHold[h] == false);
				}
				bool jumpPress = (jumpHold == true && preJumpHold == false);
				bool releasePress = (releaseHold == true && preReleaseHold == false);

				// プレイヤーの移動
				player.Velocity.x = inX * PlayerTuning::values.moveSpeed;
				if (inX > 0.1f) {
					player.isFacingRight = true;
				}
				else if (inX < -0.1f) {
					player.isFacingRight = false;
				}

				// 横の反動(触手を離した時などの勢い。飛んでる間は逆向きの入力が弱まり、反動は徐々に減る)
				if (knockX != 0.0f) {
					float inputX = player.Velocity.x;
					if (inputX * knockX < 0.0f) {
						float strength = fabsf(knockX) / PlayerTuning::values.gripLaunchSpeed;   // 離した直後は1.0、反動が終わる頃は0.0
						if (strength > 1.0f) {
							strength = 1.0f;
						}
						inputX *= 1.0f - strength * (1.0f - kKnockInputRate);
					}
					player.Velocity.x = inputX + knockX;

					// 最初の kKnockHoldFrames フレームは保って、その後減らす
					if (knockTimer > 0) {
						knockTimer--;
					}
					else {
						knockX *= kKnockFriction;
						if (fabsf(knockX) < 0.5f) {
							knockX = 0.0f;
						}
					}
				}

				// 足元が床か
				bool isGround = (player.pos.y + kPlayerHalfH + 1.0f >= kFloorY);

				// 着地でジャンプ回復・反動も止める
				if (isGround == true) {
					jumpReady = true;
					if (knockTimer <= 0) {
						knockX = 0.0f;
					}
				}

				// ジャンプ
				if (jumpPress == true && jumpReady == true) {
					player.Velocity.y = -PlayerTuning::values.jumpPower;
					jumpReady = false;
				}
				if (jumpHold == false && preJumpHold == true) {
					if (player.Velocity.y < -PlayerTuning::values.jumpCut) {
						player.Velocity.y = -PlayerTuning::values.jumpCut;
					}
				}

				// 狙う向き。何も倒してなければ向いてる方向
				float aimLen = sqrtf(inX * inX + inY * inY);
				bool hasAimDir = (aimLen > 0.01f);
				float aimDx = 0.0f;
				float aimDy = 0.0f;
				if (hasAimDir == true) {
					aimDx = inX / aimLen;
					aimDy = inY / aimLen;
				}

				// 狙う杭の探し方:届く範囲の杭のうち、狙う向きの左右60度以内で、近くて向きが合うもの。
				// hasExclude が true なら exIdx の杭は除外(もう片方が掴んでる杭)。candIdx には届く範囲のものを全部入れる
				auto FindLock = [&](bool hasExclude, int exIdx, int* candIdx, int& candCount, int& lockIdx) -> bool {
					candCount = 0;
					lockIdx = 0;
					bool isLock = false;
					float adx = aimDx;
					float ady = aimDy;
					float lockCos = kGripLockCos;
					if (hasAimDir == false) {
						// 何も倒してない時:向いてる方向を基準に、どの向きの杭でも狙える(近くて向きが合うものを優先)
						adx = player.isFacingRight ? 1.0f : -1.0f;
						ady = 0.0f;
						lockCos = -1.1f;
						// もう片方が掴んでたら、その反対側を基準にする
						if (hasExclude == true) {
							float awayX = player.pos.x - stakes[exIdx].pos.x;
							float awayY = player.pos.y - stakes[exIdx].pos.y;
							float awayLen = sqrtf(awayX * awayX + awayY * awayY);
							if (awayLen > 1.0f) {
								adx = awayX / awayLen;
								ady = awayY / awayLen;
							}
						}
					}

					// 近くて向きが合う杭を探す。1.0e9f はスコアの初期値(大きいほど悪い)
					float bestScore = 1.0e9f;
					for (int i = 0; i < kStakeNum; i++) {
						if (stakes[i].goneFrames > 0) {
							continue;   // 外れた杭は狙えない
						}
						if (hasExclude == true && i == exIdx) {
							continue;
						}
						float vx = stakes[i].pos.x - player.pos.x;
						float vy = stakes[i].pos.y - player.pos.y;
						float dist = sqrtf(vx * vx + vy * vy);
						// 背景GPは従来の照準判定を維持し、ボスGPだけアシストする。
						const bool bossGp = (i >= ExactPlayerBattle::BossAnchorStart());
						const float baseRange = PlayerTuning::values.lockRange;
						const float lockRange = baseRange * (bossGp ? BossCombatEditor::values.bossGripRangeMultiplier : 1.0f);
						if (dist > lockRange || dist < 40.0f) {
							continue;
						}
						if (candCount < kMaxGripCand) {
							candIdx[candCount] = i;
							candCount++;
						}
						float cosA = (vx * adx + vy * ady) / dist;
						const float targetCos = bossGp && hasAimDir ? BossCombatEditor::values.bossGripAimCos : lockCos;
						if (cosA >= targetCos) {
							// 近いほど、向きが合うほど優先
							float score = dist + (1.0f - cosA) * 400.0f;
							if (bossGp) score -= BossCombatEditor::values.bossGripPriorityBonus;
							if (score < bestScore) {
								bestScore = score;
								isLock = true;
								lockIdx = i;
							}
						}
					}
					return isLock;
					};

				// 触手が出てるか(伸び・ロープ・引っ張り中。しまい中は含まない)
				bool isOut[2];
				for (int h = 0; h < 2; h++) {
					isOut[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind);
				}

				// 狙いマーク:まだ出してない触手が狙う杭を出す(もう片方が掴んでる杭は除く)
				gripCandCount = 0;
				isGripLock = false;
				{
					int markSet = -1;
					for (int h = 0; h < 2; h++) {
						if (isOut[h] == false) {
							markSet = h;
							break;
						}
					}
					gripMarkSet = markSet;
					if (markSet >= 0) {
						int markOther = 1 - markSet;
						bool markHasEx = isOut[markOther];
						int markExIdx = sets[markOther].gripIdx;
						isGripLock = FindLock(markHasEx, markExIdx, gripCand, gripCandCount, gripLockIdx);
					}
				}

				// 入力方向の矢印:入力方向へプレイヤーの周りを回り込む(近いほうまわり)
				if (hasAimDir == true) {
					float targetAng = atan2f(inY, inX);
					if (aimArrowAlpha <= 0.01f) {
						aimArrowAng = targetAng;   // 出始めはその向きにいきなり出す
					}
					float diffAng = targetAng - aimArrowAng;
					while (diffAng > kPi) { diffAng -= 2.0f * kPi; }
					while (diffAng < -kPi) { diffAng += 2.0f * kPi; }
					aimArrowAng += diffAng * kAimArrowFollow;
					aimArrowAlpha += 0.3f;
					if (aimArrowAlpha > 1.0f) { aimArrowAlpha = 1.0f; }
				}
				else {
					aimArrowAlpha -= 0.2f;
					if (aimArrowAlpha < 0.0f) { aimArrowAlpha = 0.0f; }
				}

				// L/Rの矢印:杭に着いてる間(ロープ・引っ張り中)だけ出て回る
				for (int h = 0; h < 2; h++) {
					bool isHooked = (sets[h].isActive == true && sets[h].state == kSetOut
						&& (sets[h].gripPhase == kGripHold || sets[h].gripPhase == kGripPull));
					if (isHooked == true) {
						lrRingAng[h] += (sets[h].gripPhase == kGripPull) ? kLRRingPullSpeed : kLRRingSpeed;
						if (lrRingAng[h] > 2.0f * kPi) { lrRingAng[h] -= 2.0f * kPi; }
						lrRingFade[h] += kLRRingFade;
						if (lrRingFade[h] > 1.0f) { lrRingFade[h] = 1.0f; }
					}
					else {
						lrRingFade[h] -= kLRRingFade;
						if (lrRingFade[h] <= 0.0f) {
							lrRingFade[h] = 0.0f;
							lrRingAng[h] = 0.0f;
						}
					}
				}

				// 触手の発射・解除
				// Aボタン(LSHIFT):出てる触手を全部解除
				if (releasePress == true) {
					for (int h = 0; h < 2; h++) {
						UnwindSet(h);
					}
					for (int h = 0; h < 2; h++) {
						isOut[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind);
					}
				}

				//1回目で射出して掴む(押しっぱなしで引っ張る)。ロープ状態でもう一度押すと巻き取り
				for (int h = 0; h < 2; h++) {
					if (tentPress[h] == false) {
						continue;
					}
					if (isOut[h] == true) {
						if (sets[h].gripPhase == kGripHold) {
							// ロープでぶら下がり中にもう一度押したら離さず杭へ巻き取る(押してる間だけ。離すとロープに戻る)
							TentacleSet& rs2 = sets[h];
							float reelDx = rs2.gripPos.x - player.pos.x;
							float reelDy = rs2.gripPos.y - player.pos.y;
							float reelDist = sqrtf(reelDx * reelDx + reelDy * reelDy);
							if (reelDist > 1.0f) {
								rs2.pullDir.x = reelDx / reelDist;
								rs2.pullDir.y = reelDy / reelDist;
							}
							rs2.gripPhase = kGripPull;
							rs2.gripTimer = 0;
							rs2.pullStartDist = reelDist;
							rs2.pullProg = 0.0f;
						}
						else {
							UnwindSet(h);
							isOut[h] = false;
						}
						continue;
					}

					// もう片方が掴んでる杭は狙わない。左右で別々の杭に掛けられる
					int other = 1 - h;
					bool hasEx = isOut[other];
					int exIdx = sets[other].gripIdx;
					int shotCand[kMaxGripCand];
					int shotCandCount = 0;
					int shotIdx = 0;
					bool isShotLock = FindLock(hasEx, exIdx, shotCand, shotCandCount, shotIdx);
					if (isShotLock == false) {
						continue;
					}

					// ゲートは杭の反対側。触手1本が杭へ伸びて巻き付く
					TentacleSet& ts = sets[h];
					memset(&ts, 0, sizeof(ts));
					ts.isActive = true;
					ts.state = kSetOut;
					ts.timer = 0;
					ts.gripPhase = kGripReach;
					ts.gripIdx = shotIdx;
					ts.gripPos = stakes[shotIdx].pos;
					float toX = ts.gripPos.x - player.pos.x;
					float toY = ts.gripPos.y - player.pos.y;
					float toLen = sqrtf(toX * toX + toY * toY);
					if (toLen < 1.0f) {
						toLen = 1.0f;
					}
					ts.dir.x = toX / toLen;
					ts.dir.y = toY / toLen;
					ts.pullDir = ts.dir;
					ts.ropeLen = toLen;
					ts.gripSpin = (ts.dir.x >= 0.0f) ? 1.0f : -1.0f;
					float sideSign = (h == 0) ? -1.0f : 1.0f;   // 左の触手は左側、右の触手は右側のゲートから出る
					ts.gateOffset.x = -ts.dir.x * kGateBackX - ts.dir.y * sideSign * kGateSplit;
					ts.gateOffset.y = -ts.dir.y * kGateBackY + ts.dir.x * sideSign * kGateSplit;
					ts.gatePos.x = player.pos.x + ts.gateOffset.x;
					ts.gatePos.y = player.pos.y + ts.gateOffset.y;
					ts.t.phase = static_cast<float>(rand() % 628) / 100.0f;
					isOut[h] = true;
				}

				// プレイヤー(重力)
				player.Velocity.y += PlayerTuning::values.gravity;
				if (player.Velocity.y > PlayerTuning::values.maxFallSpeed) {
					player.Velocity.y = PlayerTuning::values.maxFallSpeed;
				}

				// 引っ張り中は重力もキー入力も無視して杭へ一直線(2本とも引いてるなら向きを平均)
				int pullCount = 0;
				float pullSumX = 0.0f;
				float pullSumY = 0.0f;
				for (int h = 0; h < 2; h++) {
					TentacleSet& ps = sets[h];
					if (ps.isActive == false || ps.gripPhase != kGripPull) {
						continue;
					}
					float pullDx = ps.gripPos.x - player.pos.x;
					float pullDy = ps.gripPos.y - player.pos.y;
					float pullDist = sqrtf(pullDx * pullDx + pullDy * pullDy);
					if (pullDist > 1.0f) {
						ps.pullDir.x = pullDx / pullDist;
						ps.pullDir.y = pullDy / pullDist;
					}
					pullSumX += ps.pullDir.x;
					pullSumY += ps.pullDir.y;
					pullCount++;
				}
				// 触手ごとに、巻いてる間とそのあとの勢いで飛んでる間を数える(もう片方の引きはがし判定用)
				for (int h = 0; h < 2; h++) {
					if (sets[h].isActive == true && sets[h].gripPhase == kGripPull) {
						reelGrace[h] = kYankGraceFrames;
						reelPos[h] = sets[h].gripPos;
					}
					else if (reelGrace[h] > 0) {
						reelGrace[h]--;
					}
				}
				if (pullCount > 0) {
					pullSumX /= pullCount;
					pullSumY /= pullCount;
					player.Velocity.x = pullSumX * PlayerTuning::values.gripPullSpeed;
					player.Velocity.y = pullSumY * PlayerTuning::values.gripPullSpeed;
					knockX = 0.0f;
					knockTimer = 0;
					if (pullSumX > 0.1f) {
						player.isFacingRight = true;
					}
					else if (pullSumX < -0.1f) {
						player.isFacingRight = false;
					}
				}

				// 移動(壁で止まる)
				MoveX(player.Velocity.x);
				if (MoveY(player.Velocity.y) == true) {
					player.Velocity.y = 0.0f;
				}

				// 掴んでる間はその長さより遠くへ離れられない
				// もう片方が巻いてる間は巻きが優先。
				// 2つの杭の距離が kPeelNeedLen 以上なら杭が抜ける。足りなければ触手が外れる
				for (int h = 0; h < 2; h++) {
					TentacleSet& rs = sets[h];
					if (rs.isActive == false) {
						continue;
					}
					float ropeDx = player.pos.x - rs.gripPos.x;
					float ropeDy = player.pos.y - rs.gripPos.y;
					float ropeDist = sqrtf(ropeDx * ropeDx + ropeDy * ropeDy);
					if (rs.gripPhase == kGripPull) {
						// 引っ張られて近づく間はロープも縮む
						if (ropeDist < rs.ropeLen) {
							rs.ropeLen = ropeDist;
						}
						continue;
					}
					if (rs.gripPhase != kGripHold || ropeDist <= rs.ropeLen || ropeDist < 1.0f) {
						continue;
					}

					// 引きはがし判定は、もう片方が巻いてる時だけ。自分で巻いて近づいただけでは判定しない
					int otherIdx = 1 - h;
					bool isYank = ((sets[otherIdx].isActive == true && sets[otherIdx].gripPhase == kGripPull) || reelGrace[otherIdx] > 0);
					if (isYank == true) {
						// 限界まで伸びたら自動で外れる。もう片方の杭とこの杭が遠ければ抜く
						float peelGapX = rs.gripPos.x - reelPos[otherIdx].x;
						float peelGapY = rs.gripPos.y - reelPos[otherIdx].y;
						const float gap = sqrtf(peelGapX * peelGapX + peelGapY * peelGapY);
						if (rs.gripIdx >= ExactPlayerBattle::BossAnchorStart()) {
							// Boss screws are never destroyed by the sample's stake HP.
							// Emit the same two-arm tug for the armor tension controller.
							if (sets[otherIdx].gripIdx >= 0 &&
								sets[otherIdx].gripIdx < ExactPlayerBattle::BossAnchorStart()) {
								snapshot.yankTarget = rs.gripIdx;
								snapshot.yankGap = gap;
							}
							stakes[rs.gripIdx].shakeUntil = frameCount + kStakeShakeFrames;
						}
						else if (gap >= kPeelNeedLen) {
							SpawnPeel(rs.gripPos.x, rs.gripPos.y, ropeDx / ropeDist * kPeelPieceSpeed, ropeDy / ropeDist * kPeelPieceSpeed - 3.0f);
							RemoveStake(rs.gripIdx);
							AddPopup(rs.gripPos.x, rs.gripPos.y, 0);
						}
						else {
							stakes[rs.gripIdx].shakeUntil = frameCount + kStakeShakeFrames;
							AddPopup(rs.gripPos.x, rs.gripPos.y, 1);
						}
						rs.gripPhase = kGripUnwind;
						continue;
					}

					// 長さの所まで引き戻して、外向きの勢いを消す
					float ropeNx = ropeDx / ropeDist;
					float ropeNy = ropeDy / ropeDist;
					float ropeTargetX = rs.gripPos.x + ropeNx * rs.ropeLen;
					float ropeTargetY = rs.gripPos.y + ropeNy * rs.ropeLen;
					MoveX(ropeTargetX - player.pos.x);
					MoveY(ropeTargetY - player.pos.y);
					float outSpeed = player.Velocity.x * ropeNx + player.Velocity.y * ropeNy;
					if (outSpeed > 0.0f) {
						player.Velocity.y -= ropeNy * outSpeed;
						if (knockX * ropeNx > 0.0f) {
							knockX = 0.0f;
						}
					}
				}



				// プレイヤーのアニメーション
				if (isGround == false && player.Velocity.y < 0.0f) {
					playerTex = playerJumpTex;
				}
				else if (isGround == false) {
					playerTex = playerFallTex;
				}
				else if (player.Velocity.x != 0.0f) {
					playerTex = playerWalkTex;
				}
				else {
					playerTex = playerIdleTex;
				}

				animationTimer++;
				if (animationTimer >= kAnimationSpeed) {
					animationTimer = 0;
					animationFrame++;
					if (animationFrame >= kAnimationFrameCount) {
						animationFrame = 0;
					}
				}


				// 触手とゲートの更新
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false) {
						continue;
					}

					ts.timer++;

					// ゲートはプレイヤーに少し遅れてついてくる
					{
						float followX = player.pos.x + ts.gateOffset.x;
						float followY = player.pos.y + ts.gateOffset.y;
						// 画面外に出ないよう制限
						if (followX < 16.0f) { followX = 16.0f; }
						if (followX > kWindowWidth - 16.0f) { followX = kWindowWidth - 16.0f; }
						if (followY < 16.0f) { followY = 16.0f; }
						if (followY > kWindowHeight - 16.0f) { followY = kWindowHeight - 16.0f; }
						ts.gatePos.x += (followX - ts.gatePos.x) * kGateFollow;
						ts.gatePos.y += (followY - ts.gatePos.y) * kGateFollow;
					}

					if (ts.state == kSetOut) {
						// ゲートの開き
						ts.gateOpen = static_cast<float>(ts.timer) / kGateOpenFrames;
						if (ts.gateOpen > 1.0f) {
							ts.gateOpen = 1.0f;
						}

						// ゲートの口は常に杭のほうを向く(2つは左右にずらす)
						float gx = ts.gripPos.x - ts.gatePos.x;
						float gy = ts.gripPos.y - ts.gatePos.y;
						float gl = sqrtf(gx * gx + gy * gy);
						if (gl > 1.0f) {
							ts.dir.x = gx / gl;
							ts.dir.y = gy / gl;
						}
						float sideSign = (s == 0) ? -1.0f : 1.0f;
						ts.gateOffset.x = -ts.dir.x * kGateBackX - ts.dir.y * sideSign * kGateSplit;
						ts.gateOffset.y = -ts.dir.y * kGateBackY + ts.dir.x * sideSign * kGateSplit;

						GripPath gp;
						BuildGripPath(ts.gatePos, ts.gripPos, ts.gripSpin, gp);

						Tentacle& gt = ts.t;   // 杭に巻き付く触手
						if (ts.gripPhase == kGripReach) {
							// 杭の接点までまっすぐ伸びる
							gt.len += PlayerTuning::values.gripReachSpeed;
							if (gt.len >= gp.legLen) {
								gt.len = gp.legLen;
								// ボタン押しっぱなしなら引っ張る。離してたらロープでつながる
								float reachDx = ts.gripPos.x - player.pos.x;
								float reachDy = ts.gripPos.y - player.pos.y;
								float reachDist = sqrtf(reachDx * reachDx + reachDy * reachDy);
								if (reachDist > 1.0f) {
									ts.pullDir.x = reachDx / reachDist;
									ts.pullDir.y = reachDy / reachDist;
								}
								ts.ropeLen = reachDist;
								jumpReady = true;
								if (tentHold[s] == true) {
									ts.gripPhase = kGripPull;
									ts.gripTimer = 0;
									ts.pullStartDist = reachDist;
									ts.pullProg = 0.0f;
								}
								else {
									ts.gripPhase = kGripHold;
								}
							}
						}
						else if (ts.gripPhase == kGripHold) {
							// 巻き付いたまま。伸びきってる間はピンと張る
							gt.len = gp.total;
							float holdDx = player.pos.x - ts.gripPos.x;
							float holdDy = player.pos.y - ts.gripPos.y;
							float holdDist = sqrtf(holdDx * holdDx + holdDy * holdDy);
							float tenseTarget = (holdDist >= ts.ropeLen - 3.0f) ? 1.0f : 0.0f;
							ts.gripTense += (tenseTarget - ts.gripTense) * 0.3f;
						}
						else if (ts.gripPhase == kGripPull) {
							// ボタンを離したら勢いを残してロープに戻る。杭に近づいたら離して杭の向きへ飛ぶ
							ts.gripTimer++;
							float pullDx = ts.gripPos.x - player.pos.x;
							float pullDy = ts.gripPos.y - player.pos.y;
							float pullDist = sqrtf(pullDx * pullDx + pullDy * pullDy);

							// 巻きのダメージ:巻いた割合の2乗 × 距離
							// 距離の割合はkStakeReelRefDist 以上から巻けば1.0。
							// 杭の手前まで巻き切る長押しを kStakeFullReels 回やると耐久がちょうど0
							bool isDropped = false;
							{
								float reelSpan = ts.pullStartDist - kGripReleaseDist;
								if (reelSpan > 1.0f) {
									float reelProg = (ts.pullStartDist - pullDist) / reelSpan;
									if (reelProg > 1.0f) { reelProg = 1.0f; }
									if (reelProg > ts.pullProg) {
										float distRate = reelSpan / kStakeReelRefDist;
										if (distRate > 1.0f) { distRate = 1.0f; }
										if (distRate < 0.2f) { distRate = 0.2f; }
										float reelDmg = kStakeHpMax / kStakeFullReels * distRate * (reelProg * reelProg - ts.pullProg * ts.pullProg);
										ts.pullProg = reelProg;
										Stake& gs = stakes[ts.gripIdx];
										if (ts.gripIdx >= ExactPlayerBattle::BossAnchorStart()) {
											snapshot.loosenTarget[s] = ts.gripIdx;
											snapshot.loosenDelta[s] += reelDmg / kStakeHpMax;
										}
										else {
											gs.hp -= reelDmg;
										}
										if (reelDmg > 0.05f) {
											gs.shakeUntil = frameCount + kStakeShakeFrames;
										}
										if (gs.hp <= 0.5f) {   // 計算誤差で少し残らないように
											// 外れる:引っ張りの向きへ少し飛びながら落ちる。勢いは残して触手をしまう
											SpawnPeel(ts.gripPos.x, ts.gripPos.y, static_cast<float>(rand() % 5 - 2), -5.0f);
											RemoveStake(ts.gripIdx);
											AddPopup(ts.gripPos.x, ts.gripPos.y, 2);
											player.Velocity.y = ts.pullDir.y * PlayerTuning::values.gripLaunchSpeed;
											knockX = ts.pullDir.x * PlayerTuning::values.gripLaunchSpeed;
											knockTimer = kKnockHoldFrames;
											jumpReady = true;
											ts.gripPhase = kGripUnwind;
											isDropped = true;
										}
									}
								}
							}

							if (isDropped == true) {
								// もう外れた
							}
							else if (tentHold[s] == false || ts.gripTimer >= kGripPullMax) {
								player.Velocity.y = ts.pullDir.y * PlayerTuning::values.gripPullSpeed;
								knockX = ts.pullDir.x * PlayerTuning::values.gripPullSpeed;
								knockTimer = kKnockHoldFrames;
								ts.ropeLen = pullDist;
								ts.gripPhase = kGripHold;
							}
							else if (pullDist < kGripReleaseDist) {
								player.Velocity.y = ts.pullDir.y * PlayerTuning::values.gripLaunchSpeed;
								knockX = ts.pullDir.x * PlayerTuning::values.gripLaunchSpeed;
								knockTimer = kKnockHoldFrames;
								jumpReady = true;
								ts.gripPhase = kGripUnwind;
							}
						}
						else if (ts.gripPhase == kGripUnwind) {
							// 巻きを解きながらゲートへしまう
							gt.len -= gp.total / kGripUnwindFrames;
							if (gt.len <= 0.0f) {
								gt.len = 0.0f;
								ts.state = kSetRetract;
								ts.timer = 0;
								ts.t.retractFrom = ts.t.len;
								continue;
							}
						}

						// 引っ張り中:長さは変えない。ゲートが杭に近づいて直線部分が短くなった分、先端が杭に巻き付く。ピンと張ってゆれない
						if (ts.gripPhase == kGripPull) {
							if (gt.len > gp.total) {
								gt.len = gp.total;
							}
							if (gt.len < gp.legLen) {
								gt.len = gp.legLen;
							}
							ts.gripTense += 0.25f;
							if (ts.gripTense > 1.0f) { ts.gripTense = 1.0f; }
						}
						else if (ts.gripPhase == kGripUnwind) {
							ts.gripTense -= 0.15f;
							if (ts.gripTense < 0.0f) { ts.gripTense = 0.0f; }
						}

					}
					else if (ts.state == kSetRetract) {
						// 触手をゲートへ戻す
						if (ts.timer <= kRetractFrames) {
							float p = static_cast<float>(ts.timer) / kRetractFrames;
							// 触手の長さを戻す
							ts.t.len = ts.t.retractFrom * (1.0f - p * p);
						}
						else {
							// 触手を完全にしまう
							ts.t.len = 0.0f;
							// ゲートを閉じる
							ts.gateOpen = 1.0f - static_cast<float>(ts.timer - kRetractFrames) / kGateCloseFrames;
							if (ts.gateOpen <= 0.0f) {
								ts.gateOpen = 0.0f;
								ts.isActive = false;
							}
						}
					}
				}

				// 触手の形を計算
				// s:触手セットの番号(1セット1本)
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false) {
						continue;
					}
					// ゲート → 杭の接点 → 渦、の経路に沿って点を並べる
					Tentacle& tn = ts.t;
					GripPath gp;
					BuildGripPath(ts.gatePos, ts.gripPos, ts.gripSpin, gp);
					float visLen = tn.len;
					if (visLen > gp.total) {
						visLen = gp.total;
					}
					for (int i = 0; i <= kTentacleSeg; i++) {
						float sArc = visLen * static_cast<float>(i) / kTentacleSeg;
						Vector2 p = GripPathPoint(gp, sArc);
						// 直線部分だけゆらす(両端は動かさない。引っ張り中・ピンと張ってる間はゆれない)
						if (sArc < gp.legLen && gp.legLen > 1.0f) {
							float u = sArc / gp.legLen;
							float amp = 12.0f * sinf(kPi * u) * (1.0f - ts.gripTense);
							float side = sinf(frameCount * 0.15f + tn.phase + u * 6.0f) * amp;
							float lx = (gp.tangent.x - gp.gate.x) / gp.legLen;
							float ly = (gp.tangent.y - gp.gate.y) / gp.legLen;
							p.x += -ly * side;
							p.y += lx * side;
						}
						pts[s][i] = p;
					}
				}

				// 抜けた杭を元に戻す(デバッグ用)
				if (keys[DIK_R] != 0 && preKeys[DIK_R] == 0) {
					ResetStakes();
				}

				// 外れた杭のリポップ
				for (int i = 0; i < kStakeNum; i++) {
					Stake& stk = stakes[i];
					if (stk.goneFrames > 0) {
						stk.goneFrames--;
						if (stk.goneFrames == 0) {
							stk.hp = kStakeHpMax;
							stk.shakeUntil = 0;
							stk.popFrames = kStakePopFrames;
						}
					}
					if (stk.popFrames > 0) {
						stk.popFrames--;
					}
				}

				// 画面に出す文字
				for (int i = 0; i < kMaxPopup; i++) {
					if (popups[i].on == true) {
						popups[i].t++;
						if (popups[i].t >= kPopupFrames) {
							popups[i].on = false;
						}
					}
				}

				// 抜けて飛ぶ杭
				for (int i = 0; i < kStakePeelMax; i++) {
					if (stakePeels[i].on == true) {
						stakePeels[i].x += stakePeels[i].vx;
						stakePeels[i].y += stakePeels[i].vy;
						stakePeels[i].vx *= 0.97f;
						stakePeels[i].vy += 0.6f;
						stakePeels[i].t++;
						if (stakePeels[i].t >= kPeelPieceFrames) {
							stakePeels[i].on = false;
						}
					}
				}

				// 1フレーム前の入力を保存
				preTentHold[0] = tentHold[0];
				preTentHold[1] = tentHold[1];
				preJumpHold = jumpHold;
				preReleaseHold = releaseHold;

				///
				/// ↑更新処理ここまで
				///

				///
			}
			if (draw) {
				/// ↓描画処理ここから
				///

				// 背景
				// stage draws background, preserve sample character and tentacle rendering.

				auto RoundPx = [](float v) -> int {
					return static_cast<int>(floorf(v + 0.5f));
					};

				// 絵をcx, cy・size・ang で描く
				auto DrawRotSprite = [&](int tex, float cx, float cy, float size, float ang, int srcX, int srcY, int srcW, int srcH, unsigned int col) {
					float half = size * 0.5f;
					float c = cosf(ang);
					float sn = sinf(ang);
					Novice::DrawQuad(
						RoundPx(cx + (-half) * c - (-half) * sn), RoundPx(cy + (-half) * sn + (-half) * c),
						RoundPx(cx + half * c - (-half) * sn), RoundPx(cy + half * sn + (-half) * c),
						RoundPx(cx + (-half) * c - half * sn), RoundPx(cy + (-half) * sn + half * c),
						RoundPx(cx + half * c - half * sn), RoundPx(cy + half * sn + half * c),
						srcX, srcY, srcW, srcH, tex, col);
					};

				// L/R表示をcx, cyに描く(setIdx 0=L 1=R)。
				// 触手の色で色分け。
				auto DrawLRBadge = [&](int setIdx, float cx, float cy, float size, unsigned int col) {
					if (lrTex >= 0) {
						DrawRotSprite(lrTex, cx, cy, size, 0.0f, setIdx * kLRSpriteSize, 0, kLRSpriteSize, kLRSpriteSize, col);
					}
					else {
						int rad = static_cast<int>(size * 0.5f);
						Novice::DrawEllipse(RoundPx(cx), RoundPx(cy), rad, rad, 0.0f, col, kFillModeSolid);
					}
					};

				// L/Rの周りを回る半円矢印2本(ang:回転角)。絵が無ければ丸枠
				auto DrawCycleRing = [&](float cx, float cy, float size, float ang, unsigned int col) {
					if (cycleTex >= 0) {
						DrawRotSprite(cycleTex, cx, cy, size, ang, 0, 0, kCycleSpriteSize, kCycleSpriteSize, col);
					}
					else {
						float rad = size * 0.5f;
						Novice::DrawEllipse(RoundPx(cx), RoundPx(cy), static_cast<int>(rad), static_cast<int>(rad), 0.0f, col, kFillModeWireFrame);
					}
					};

				// 点線の輪(外れた杭の跡)
				auto DrawDashRing = [&](float cx, float cy, float r, float rot, unsigned int col) {
					const int dashN = 16;
					for (int i = 0; i < dashN; i++) {
						float a0 = rot + i * (2.0f * kPi / dashN);
						float a1 = a0 + (2.0f * kPi / dashN) * 0.55f;
						for (int th = 0; th < 2; th++) {
							float rr = r + th;
							Novice::DrawLine(RoundPx(cx + cosf(a0) * rr), RoundPx(cy + sinf(a0) * rr),
								RoundPx(cx + cosf(a1) * rr), RoundPx(cy + sinf(a1) * rr), col);
						}
					}
					};

				// Floor supplied by stage scene.

				// 杭(ひっかけポイント)
				for (int si = 0; si < kStakeNum; si++) {
					Stake& stk = stakes[si];
					int px = RoundPx(stk.pos.x) - kTileSize / 2;
					int py = RoundPx(stk.pos.y) - kTileSize / 2;
					// 外れた杭の跡:薄い杭の影 + 点線の輪 + リポップまでの残り秒数を、元の場所に重ねて出す
					if (stk.goneFrames > 0) {
						int goneLeft = stk.goneFrames;
						float goneProg = 1.0f - static_cast<float>(goneLeft) / kStakeRespawnFrames;
						float mcx = static_cast<float>(px + kTileSize / 2);
						float mcy = static_cast<float>(py + kTileSize / 2);
						bool isSoon = (goneLeft <= 120);   // 残り2秒を切ったら点滅
						DrawStakeSprite(px, py, kTileSize, 0xC8C8C800 | static_cast<unsigned int>(50 + static_cast<int>(90.0f * goneProg)));
						unsigned int ringA = (isSoon == true && (frameCount / 6) % 2 == 0) ? 255 : 150;
						DrawDashRing(mcx, mcy, kGripRadius + 6.0f, frameCount * 0.02f, 0xFFFFFF00 | ringA);
						int goneSec = (goneLeft + 59) / 60;
						if (goneSec > 99) { goneSec = 99; }
						if (numTex >= 0) {
							int goneDigits = 1;
							if (goneSec >= 10) { goneDigits = 2; }
							int goneW = kNumSpriteW * kNumScale;
							int goneH = kNumSpriteH * kNumScale;
							int goneX0 = RoundPx(mcx) - (goneDigits * goneW + (goneDigits - 1) * kNumGap) / 2;
							int goneY0 = RoundPx(mcy) - goneH / 2;
							unsigned int goneCol = 0xFFFFFFFF;
							if (isSoon == true) {
								goneCol = 0xFFD746FF;
							}
							for (int pass = 0; pass < 2; pass++) {   // 0:黒い影 1:本体
								int off = 0;
								unsigned int c = goneCol;
								if (pass == 0) {
									off = 2;
									c = 0x000000D0u;
								}
								for (int d = 0; d < goneDigits; d++) {
									int digit = goneSec;
									if (goneDigits == 2) {
										if (d == 0) { digit = goneSec / 10; }
										else { digit = goneSec % 10; }
									}
									int dx = goneX0 + d * (goneW + kNumGap) + off;
									int dy = goneY0 + off;
									Novice::DrawQuad(dx, dy, dx + goneW, dy, dx, dy + goneH, dx + goneW, dy + goneH,
										digit * kNumSpriteW, 0, kNumSpriteW, kNumSpriteH, numTex, c);
								}
							}
						}
						else {
							Novice::ScreenPrintf(RoundPx(mcx) - 8, RoundPx(mcy) - 8, "%d", goneSec);
						}
						continue;
					}

					// 杭:絵があれば絵、無ければ仮の茶色い丸
					// 耐久が減るほど赤黒く濃くなってひびが入る。0に近いと点滅。引き伸ばされた直後は震える
					float hpRate = stk.hp / kStakeHpMax;
					if (hpRate < 0.0f) { hpRate = 0.0f; }
					if (hpRate > 1.0f) { hpRate = 1.0f; }
					float dmgRate = 1.0f - hpRate;
					float stakeBright = kStakeDamagedTint + (1.0f - kStakeDamagedTint) * hpRate;
					unsigned int stakeTint = (static_cast<unsigned int>(255.0f * (0.45f + 0.55f * stakeBright)) << 24) | (static_cast<unsigned int>(255.0f * stakeBright) << 16) | (static_cast<unsigned int>(255.0f * stakeBright * 0.9f) << 8) | 0xFF;
					unsigned int fallbackTint = (static_cast<unsigned int>(139.0f * (0.45f + 0.55f * stakeBright)) << 24) | (static_cast<unsigned int>(90.0f * stakeBright) << 16) | (static_cast<unsigned int>(43.0f * stakeBright) << 8) | 0xFF;
					int shakeX = 0;
					if (frameCount < stk.shakeUntil) {
						shakeX = (frameCount % 2 == 0) ? static_cast<int>(kStakeShakePx) : -static_cast<int>(kStakeShakePx);
					}
					float mcx = static_cast<float>(px + kTileSize / 2 + shakeX);
					float mcy = static_cast<float>(py + kTileSize / 2);

					// リポップ直後:大きくなって、少し行き過ぎてから落ち着く
					float popScale = 1.0f;
					if (stk.popFrames > 0) {
						float pt = 1.0f - static_cast<float>(stk.popFrames) / kStakePopFrames;
						popScale = (pt < 0.7f) ? 1.15f * (pt / 0.7f) : 1.15f - 0.15f * ((pt - 0.7f) / 0.3f);
						if (popScale < 0.05f) { popScale = 0.05f; }
						float burstR = kGripRadius + 24.0f * pt;
						Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), static_cast<int>(burstR), static_cast<int>(burstR), 0.0f, 0xFFFFFF00 | static_cast<unsigned int>(200.0f * (1.0f - pt)), kFillModeWireFrame);
					}
					int drawSize = static_cast<int>(kTileSize * popScale);
					int drawX = px + (kTileSize - drawSize) / 2 + shakeX;
					int drawY = py + (kTileSize - drawSize) / 2;
					if (DrawStakeSprite(drawX, drawY, drawSize, stakeTint) == false) {
						int fr = static_cast<int>(kGripRadius * popScale);
						Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), fr, fr, 0.0f, fallbackTint, kFillModeSolid);
					}

					if (hpRate < 1.0f) {
						int gr = static_cast<int>(kGripRadius);
						// 傷んだ分だけ赤黒い色を重ねる
						Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), gr, gr, 0.0f, 0x3C000000 | static_cast<unsigned int>(140.0f * dmgRate), kFillModeSolid);
						// 耐久が3割を切ったら赤く点滅(もうすぐ外れる)
						if (hpRate < 0.3f && (frameCount / 4) % 2 == 0) {
							Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), gr, gr, 0.0f, 0xFF281450, kFillModeSolid);
						}
						// ひび割れ:傷むほど本数が増える(最大4本)
						static const float kCrack[4][6] = {
							{ -2.0f, -17.0f,  3.0f,  -8.0f, -4.0f,  -1.0f },
							{ 10.0f,  -6.0f,  4.0f,   0.0f,  9.0f,   9.0f },
							{ -12.0f,  4.0f, -5.0f,   6.0f, -8.0f,  15.0f },
							{ 3.0f,    8.0f, -1.0f,  12.0f,  6.0f,  17.0f },
						};
						int crackN = static_cast<int>(dmgRate * 4.99f);
						for (int c = 0; c < crackN; c++) {
							int cx0 = RoundPx(mcx + kCrack[c][0]);
							int cy0 = RoundPx(mcy + kCrack[c][1]);
							int cx1 = RoundPx(mcx + kCrack[c][2]);
							int cy1 = RoundPx(mcy + kCrack[c][3]);
							int cx2 = RoundPx(mcx + kCrack[c][4]);
							int cy2 = RoundPx(mcy + kCrack[c][5]);
							for (int th = 0; th < 2; th++) {
								Novice::DrawLine(cx0 + th, cy0, cx1 + th, cy1, 0x200800FFu);
								Novice::DrawLine(cx1 + th, cy1, cx2 + th, cy2, 0x200800FFu);
							}
						}
						// 耐久バー
						int barW = kTileSize - 8;
						unsigned int barCol = (hpRate > 0.6f) ? 0x60E060FFu : ((hpRate > 0.3f) ? 0xF0C030FFu : 0xE04040FFu);
						Novice::DrawBox(px + 3, py + 1, barW + 2, 7, 0.0f, 0x000000C0u, kFillModeSolid);
						Novice::DrawBox(px + 4, py + 2, static_cast<int>(barW * hpRate), 5, 0.0f, barCol, kFillModeSolid);
					}
				}

				// 引っ張ってる触手だけ明るくする(引っ張ってない触手は普通の色のまま)
				bool isPulling[2];
				bool isPeelReady[2];   // この触手を引っ張ればもう片方の杭を引きはがせる
				unsigned int drawCol[2];
				unsigned int glowCol[2];
				for (int h = 0; h < 2; h++) {
					isPulling[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase == kGripPull);
					isPeelReady[h] = false;
				}

				for (int h = 0; h < 2; h++) {
					int holdH = 1 - h;   // 引きはがされる側の触手
					if (sets[holdH].isActive == false || sets[holdH].state != kSetOut || sets[holdH].gripPhase != kGripHold) {
						continue;
					}
					if (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind) {
						float gapX = sets[holdH].gripPos.x - sets[h].gripPos.x;
						float gapY = sets[holdH].gripPos.y - sets[h].gripPos.y;
						float gapLen = sqrtf(gapX * gapX + gapY * gapY);
						//2つの杭が kPeelNeedLen 以上離れてて、掛かってるロープの長さより遠い(引くとロープが限界まで伸びる)
						if (gapLen >= kPeelNeedLen && gapLen > sets[holdH].ropeLen) {
							isPeelReady[h] = true;
						}
					}
				}
				float peelWave = 0.5f + 0.5f * sinf(frameCount * kPeelReadyPulse);
				for (int h = 0; h < 2; h++) {
					float cr = static_cast<float>((kTentColor[h] >> 24) & 0xFF);
					float cg = static_cast<float>((kTentColor[h] >> 16) & 0xFF);
					float cb = static_cast<float>((kTentColor[h] >> 8) & 0xFF);
					unsigned int ca = 0xFF;
					if (isPulling[h] == true) {
						cr += (255.0f - cr) * kPullBright;   // 白に近づけて明るく
						cg += (255.0f - cg) * kPullBright;
						cb += (255.0f - cb) * kPullBright;
					}
					drawCol[h] = (static_cast<unsigned int>(cr) << 24) | (static_cast<unsigned int>(cg) << 16) | (static_cast<unsigned int>(cb) << 8) | ca;
					glowCol[h] = (drawCol[h] & 0xFFFFFF00u);
				}
				float glowWave = 0.5f + 0.5f * sinf(frameCount * kPullGlowSpeed);
				unsigned int glowAlpha = static_cast<unsigned int>(kPullGlowAlphaMin + (kPullGlowAlphaMax - kPullGlowAlphaMin) * glowWave);

				// ゲート(触手の色で出す)
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false || ts.gateOpen <= 0.0f) {
						continue;
					}
					float cx = ts.gatePos.x;
					float cy = ts.gatePos.y;

					// 杭に口を向ける細長いゲート
					float nx = ts.dir.x;
					float ny = ts.dir.y;
					float bx = -ny;
					float by = nx;
					float hw = kGateHalfW * (0.3f + 0.7f * ts.gateOpen);
					float hh = kGateHalfLen * ts.gateOpen;
					Novice::DrawQuad(
						static_cast<int>(cx - nx * hw - bx * hh), static_cast<int>(cy - ny * hw - by * hh),
						static_cast<int>(cx + nx * hw - bx * hh), static_cast<int>(cy + ny * hw - by * hh),
						static_cast<int>(cx - nx * hw + bx * hh), static_cast<int>(cy - ny * hw + by * hh),
						static_cast<int>(cx + nx * hw + bx * hh), static_cast<int>(cy + ny * hw + by * hh),
						0, 0, kGateSpriteW, kGateSpriteH, gateTex, drawCol[s]);
				}

				// 曲線を kTentacleStep px ごとに区切って、区切りごとに絵を貼る(太さは絵の真ん中を削って変える)
				// 区切りの点ごとに向き(法線)と太さを1回だけ計算。隣り合う区切りで点と幅を共有するので境目に隙間ができない
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false) {
						continue;
					}
					// 0回目:普通に描く 1回目:引っ張ってる触手だけ加算で重ねて光らせる
					int passCount = (isPulling[s] == true || isPeelReady[s] == true) ? 2 : 1;
					for (int pass = 0; pass < passCount; pass++) {
						unsigned int tentCol = drawCol[s];
						if (pass == 1) {
							Novice::SetBlendMode(kBlendModeAdd);
							tentCol = glowCol[s] | glowAlpha;
						}
						Tentacle& tn = ts.t;
						if (tn.len >= 2.0f) {
							// 短い間は細くする
							float widthRate = tn.len / 80.0f;
							if (widthRate > 1.0f) {
								widthRate = 1.0f;
							}

							// 曲線を根元から先端へ kTentacleStep px ごとに区切る
							float sampleX[kTentacleMaxSample];
							float sampleY[kTentacleMaxSample];
							int sampleCount = 1;
							sampleX[0] = pts[s][0].x;
							sampleY[0] = pts[s][0].y;
							float pathLen = 0.0f;
							float need = kTentacleStep;
							for (int i = 0; i < kTentacleSeg; i++) {
								float ax = pts[s][i].x;
								float ay = pts[s][i].y;
								float bx = pts[s][i + 1].x;
								float by = pts[s][i + 1].y;
								float segLen = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
								pathLen += segLen;
								float used = 0.0f;
								while (segLen - used >= need && sampleCount < kTentacleMaxSample - 2) {
									used += need;
									sampleX[sampleCount] = ax + (bx - ax) * (used / segLen);
									sampleY[sampleCount] = ay + (by - ay) * (used / segLen);
									sampleCount++;
									need = kTentacleStep;
								}
								need -= segLen - used;
							}

							// 最後の区切りから先端までの端数
							int lastRows = static_cast<int>((kTentacleStep - need) / kPixelScale + 0.5f);
							if (lastRows > kTentacleStepRows) {
								lastRows = kTentacleStepRows;
							}
							int stepCount = sampleCount - 1;
							if (lastRows >= 1) {
								sampleX[sampleCount] = pts[s][kTentacleSeg].x;
								sampleY[sampleCount] = pts[s][kTentacleSeg].y;
								sampleCount++;
								stepCount++;
							}

							// 区切りの点ごとの法線(曲線に直角)と太さ
							float perpX[kTentacleMaxSample];
							float perpY[kTentacleMaxSample];
							float halfSrcF[kTentacleMaxSample];   // 太さ(ドット数・片側)
							for (int n = 0; n < sampleCount; n++) {
								int prev = (n > 0) ? n - 1 : 0;
								int next = (n + 1 < sampleCount) ? n + 1 : sampleCount - 1;
								float vx = sampleX[next] - sampleX[prev];
								float vy = sampleY[next] - sampleY[prev];
								float vl = sqrtf(vx * vx + vy * vy);
								if (vl < 0.001f) {
									vx = 1.0f;
									vy = 0.0f;
									vl = 1.0f;
								}
								perpX[n] = -vy / vl;
								perpY[n] = vx / vl;

								// 今の長さに対して根元から先端へ細くなる(伸びてる途中も先端は鋭い)
								float t = (pathLen > 1.0f) ? (n * kTentacleStep) / pathLen : 1.0f;
								if (t > 1.0f) {
									t = 1.0f;
								}
								float h = kTentacleBaseHalfSrc * (1.0f - t) + kTentacleTipHalfSrc * t;
								h *= widthRate;
								if (h < 1.0f) {
									h = 1.0f;
								}
								halfSrcF[n] = h;
							}

							// 区切りごとに絵を左右2枚の四角形で貼る(上端は点n、下端は点n+1の値)
							for (int n = 0; n < stepCount; n++) {
								float wTop = halfSrcF[n] * kPixelScale;
								float wBot = halfSrcF[n + 1] * kPixelScale;
								int halfSrc = static_cast<int>((halfSrcF[n] + halfSrcF[n + 1]) * 0.5f + 0.5f);
								if (halfSrc < 1) {
									halfSrc = 1;
								}
								int srcHalfW = halfSrc * kPixelScale;

								int srcY = (n * kTentacleStepRows * kPixelScale) % kTentacleSpriteH;
								int rows = kTentacleStepRows;
								if (n == stepCount - 1 && lastRows >= 1) {
									rows = lastRows;
								}
								int srcRows = rows * kPixelScale;

								float cx0 = sampleX[n];
								float cy0 = sampleY[n];
								float cx1 = sampleX[n + 1];
								float cy1 = sampleY[n + 1];

								// 左半分
								Novice::DrawQuad(
									RoundPx(cx0 - perpX[n] * wTop), RoundPx(cy0 - perpY[n] * wTop),
									RoundPx(cx0), RoundPx(cy0),
									RoundPx(cx1 - perpX[n + 1] * wBot), RoundPx(cy1 - perpY[n + 1] * wBot),
									RoundPx(cx1), RoundPx(cy1),
									0, srcY, srcHalfW, srcRows, tentacleTex, tentCol);

								// 右半分
								Novice::DrawQuad(
									RoundPx(cx0), RoundPx(cy0),
									RoundPx(cx0 + perpX[n] * wTop), RoundPx(cy0 + perpY[n] * wTop),
									RoundPx(cx1), RoundPx(cy1),
									RoundPx(cx1 + perpX[n + 1] * wBot), RoundPx(cy1 + perpY[n + 1] * wBot),
									kTentacleSpriteW - srcHalfW, srcY, srcHalfW, srcRows, tentacleTex, tentCol);
							}
						}
						if (pass == 1) {
							Novice::SetBlendMode(kBlendModeNormal);
						}
					}
				}

				// プレイヤー画像だけ描画倍率を1.0倍に固定する。
				// 1コマ64x64pxの画像は、ゲーム画面でも64x64pxで表示する。
				// kPlayerW/Hは物理・当たり判定用のため変更しない。
				{
					const float kPlayerDrawScale = AssetEditor::Scale(
						AssetEditor::PlayerSlotForHandle(playerTex)); // ImGuiの表示倍率（物理には影響しない）
					const AssetEditor::FrameSource frameSource = AssetEditor::GetFrame(
						AssetEditor::PlayerSlotForHandle(playerTex), animationFrame);
					if (playerTex >= 0) {
						const float drawW = static_cast<float>(frameSource.w) * kPlayerDrawScale;
						const float drawH = static_cast<float>(frameSource.h) * kPlayerDrawScale;
						const float sideX = (player.isFacingRight ? 1.0f : -1.0f) * drawW * 0.5f;
						const float cx = player.pos.x;
						const float drawBottomY = player.pos.y + kPlayerHalfH;
						const float drawTopY = drawBottomY - drawH;
						Novice::DrawQuad(
							static_cast<int>(cx - sideX), static_cast<int>(drawTopY),
							static_cast<int>(cx + sideX), static_cast<int>(drawTopY),
							static_cast<int>(cx - sideX), static_cast<int>(drawBottomY),
							static_cast<int>(cx + sideX), static_cast<int>(drawBottomY),
							frameSource.x, frameSource.y, frameSource.w, frameSource.h,
							playerTex, 0xFFFFFFFF);
					}
				}

				// L/R表示:プレイヤーの左下にL、右下にR(触手の色で色分け)。
				// 杭に着いてる間はそのL/Rの周りを半円矢印2本が回る
				for (int s = 0; s < 2; s++) {
					float sideSign = (s == 0) ? -1.0f : 1.0f;
					float lrX = player.pos.x + sideSign * kLRUiOffsetX;
					float lrY = player.pos.y + kLRUiOffsetY;
					if (lrRingFade[s] > 0.0f) {
						float ringScale = 0.6f + 0.4f * lrRingFade[s];   // 出る時に大きくなる
						unsigned int ringAlpha = static_cast<unsigned int>(255.0f * lrRingFade[s]);
						unsigned int ringCol = (drawCol[s] & 0xFFFFFF00u) | ringAlpha;
						DrawCycleRing(lrX, lrY, kLRRingSize * ringScale, lrRingAng[s], ringCol);
					}
					DrawLRBadge(s, lrX, lrY, kLRUiSize, drawCol[s]);
				}

				// 入力方向の矢印:プレイヤーの周りに入力方向を指して出る(狙ってる杭があれば黄色)
				if (aimArrowAlpha > 0.0f) {
					float ax = player.pos.x + cosf(aimArrowAng) * kAimArrowRadius;
					float ay = player.pos.y + sinf(aimArrowAng) * kAimArrowRadius;
					float half = kAimArrowSize * 0.5f;
					float c = cosf(aimArrowAng);
					float sn = sinf(aimArrowAng);
					unsigned int arrowAlpha = static_cast<unsigned int>(255.0f * aimArrowAlpha);
					unsigned int arrowColor = ((isGripLock == true) ? 0xFFE040FFu : 0xFFFFFFFFu) & 0xFFFFFF00u;
					arrowColor |= arrowAlpha;
					if (aimArrowTex >= 0) {
						Novice::DrawQuad(
							RoundPx(ax + (-half) * c - (-half) * sn), RoundPx(ay + (-half) * sn + (-half) * c),
							RoundPx(ax + half * c - (-half) * sn), RoundPx(ay + half * sn + (-half) * c),
							RoundPx(ax + (-half) * c - half * sn), RoundPx(ay + (-half) * sn + half * c),
							RoundPx(ax + half * c - half * sn), RoundPx(ay + half * sn + half * c),
							0, 0, kAimArrowSpriteSize, kAimArrowSpriteSize, aimArrowTex, arrowColor);
					}
					else {
						Novice::DrawEllipse(RoundPx(ax), RoundPx(ay), static_cast<int>(half * 0.5f), static_cast<int>(half * 0.5f), 0.0f, arrowColor, kFillModeSolid);
					}
				}

				// 触手が掴んでる杭に、その触手の色の輪とL/Rを出す。引っ張り中は輪が脈打つ
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false || ts.state != kSetOut || ts.gripPhase == kGripUnwind) {
						continue;
					}
					float ringR = kGripRadius + 10.0f;
					if (ts.gripPhase == kGripPull) {
						ringR += 4.0f * sinf(frameCount * 0.5f);
					}
					bool isReadyRing = (isPeelReady[s] == true);
					if (isReadyRing == true) {
						ringR += 3.0f + 5.0f * peelWave;   // 巻き取れる:輪が膨らんで脈打つ
					}
					int ringX = RoundPx(ts.gripPos.x);
					int ringY = RoundPx(ts.gripPos.y);
					Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR), static_cast<int>(ringR), 0.0f, drawCol[s], kFillModeWireFrame);
					Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 1, static_cast<int>(ringR) + 1, 0.0f, drawCol[s], kFillModeWireFrame);
					if (ts.gripPhase == kGripPull) {
						// 引っ張ってる杭は輪を太く(外側にもう2本)
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 2, static_cast<int>(ringR) + 2, 0.0f, drawCol[s], kFillModeWireFrame);
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 3, static_cast<int>(ringR) + 3, 0.0f, drawCol[s], kFillModeWireFrame);
					}
					if (isReadyRing == true) {
						// 輪を太く(外側にもう2本)
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 2, static_cast<int>(ringR) + 2, 0.0f, drawCol[s], kFillModeWireFrame);
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 3, static_cast<int>(ringR) + 3, 0.0f, drawCol[s], kFillModeWireFrame);
					}
					DrawLRBadge(s, static_cast<float>(ringX), ringY - ringR - kLRMarkSize * 0.5f - 2.0f, kLRMarkSize, drawCol[s]);

				}

				// 自動照準マーク:届く範囲の杭に薄いマーク、狙ってる杭には回る大きいマーク(次に撃つ触手の色。狙ってる杭にはL/R)
				for (int i = 0; i < gripCandCount; i++) {
					bool isTarget = (isGripLock == true && gripCand[i] == gripLockIdx);
					float mx = stakes[gripCand[i]].pos.x;
					float my = stakes[gripCand[i]].pos.y;
					float half = (isTarget ? kGripMarkSize : kGripMarkSizeSub) * 0.5f;
					float ang = 0.0f;
					unsigned int markBase = 0xFFFFFFFFu;
					if (gripMarkSet >= 0) {
						markBase = kTentColor[gripMarkSet];
					}
					unsigned int markColor = (markBase & 0xFFFFFF00u) | 0x70u;
					if (isTarget == true) {
						half *= 1.0f + 0.08f * sinf(frameCount * 0.25f);   // 脈打つ
						ang = frameCount * 0.04f;                           // ゆっくり回る
						markColor = markBase;
						unsigned int badgeColor = (gripMarkSet >= 0) ? kTentColor[gripMarkSet] : 0xFFFFFFFFu;
						if (gripMarkSet >= 0) {
							DrawLRBadge(gripMarkSet, mx, my + half + kLRMarkSize * 0.5f + 2.0f, kLRMarkSize, badgeColor);
						}
					}
					if (gripMarkTex >= 0) {
						float c = cosf(ang);
						float sn = sinf(ang);
						Novice::DrawQuad(
							RoundPx(mx + (-half) * c - (-half) * sn), RoundPx(my + (-half) * sn + (-half) * c),
							RoundPx(mx + half * c - (-half) * sn), RoundPx(my + half * sn + (-half) * c),
							RoundPx(mx + (-half) * c - half * sn), RoundPx(my + (-half) * sn + half * c),
							RoundPx(mx + half * c - half * sn), RoundPx(my + half * sn + half * c),
							0, 0, kGripMarkSpriteSize, kGripMarkSpriteSize, gripMarkTex, markColor);
					}
					else {
						// 絵が無い時の仮のマーク(丸枠)
						Novice::DrawEllipse(RoundPx(mx), RoundPx(my), static_cast<int>(half), static_cast<int>(half), 0.0f, markColor, kFillModeWireFrame);
					}
				}

				// 抜けて飛ぶ杭(だんだん薄く)
				for (int i = 0; i < kStakePeelMax; i++) {
					if (stakePeels[i].on == false) {
						continue;
					}
					float stakeFade = 1.0f - static_cast<float>(stakePeels[i].t) / kPeelPieceFrames;
					unsigned int stakeColor = 0xFFFFFF00u | static_cast<unsigned int>(255.0f * stakeFade);
					int spx = RoundPx(stakePeels[i].x);
					int spy = RoundPx(stakePeels[i].y);
					if (DrawStakeSprite(spx - kTileSize / 2, spy - kTileSize / 2, kTileSize, stakeColor) == false) {
						unsigned int fallbackColor = 0x8B5A2B00u | static_cast<unsigned int>(255.0f * stakeFade);
						Novice::DrawEllipse(spx, spy, static_cast<int>(kGripRadius), static_cast<int>(kGripRadius), 0.0f, fallbackColor, kFillModeSolid);
					}
				}

				// 杭を引き抜いた時の輪
				for (int i = 0; i < kMaxPopup; i++) {
					if (popups[i].on == false) {
						continue;
					}
					float sx = popups[i].x;
					float sy = popups[i].y;
					if (popups[i].kind == 0) {
						// 成功:杭のあった場所から輪が広がる
						float burstR = 14.0f + popups[i].t * 3.5f;
						unsigned int burstA = static_cast<unsigned int>(220.0f * (1.0f - static_cast<float>(popups[i].t) / kPopupFrames));
						Novice::DrawEllipse(RoundPx(sx), RoundPx(sy), static_cast<int>(burstR), static_cast<int>(burstR), 0.0f, 0x78FFA000 | burstA, kFillModeWireFrame);
						Novice::DrawEllipse(RoundPx(sx), RoundPx(sy), static_cast<int>(burstR) + 1, static_cast<int>(burstR) + 1, 0.0f, 0x78FFA000 | burstA, kFillModeWireFrame);
					}
				}

				// 触手の状態(左下):L/Rのバッジ + 状態の文字(ScreenPrintf)
				for (int h = 0; h < 2; h++) {
					float hudY = static_cast<float>(kWindowHeight - 58 + h * 30);
					DrawLRBadge(h, 22.0f, hudY + 12.0f, 24.0f, kTentColor[h]);
					int stateFrame = 0;   // READY
					if (sets[h].isActive == true) {
						if (sets[h].gripPhase == kGripReach) {
							stateFrame = 1;   // SHOOT
						}
						else if (sets[h].gripPhase == kGripHold) {
							stateFrame = 2;   // HOOKED
						}
						else if (sets[h].gripPhase == kGripPull) {
							stateFrame = 3;   // PULL
						}
						else {
							stateFrame = 4;   // RETURN
						}
					}
					static const char* stateText[5] = { "READY", "SHOOT", "HOOKED", "PULL", "RETURN" };
					Novice::ScreenPrintf(44, RoundPx(hudY) + 4, "%s", stateText[stateFrame]);
				}


				///
				/// ↑描画処理ここまで
			}
			///



		}

		snapshot.position = { player.pos.x,720.0f - player.pos.y };
		snapshot.velocity = { player.Velocity.x,-player.Velocity.y };
		snapshot.gripping = false;
		snapshot.pulling = false;
		snapshot.gripIndex = -1;
		for (int h = 0; h < 2; ++h) {
			snapshot.handGripIndex[h] =
				(sets[h].isActive && sets[h].state == kSetOut &&
					sets[h].gripPhase != kGripUnwind) ? sets[h].gripIdx : -1;
			snapshot.handPulling[h] = snapshot.handGripIndex[h] >= 0 &&
				sets[h].gripPhase == kGripPull;
		}
		if (update) snapshot.peeledStake = stakeRemovedThisFrame;
		for (int s = 0;s < 2;++s) {
			if (sets[s].isActive && sets[s].state == kSetOut &&
				sets[s].gripPhase != kGripUnwind) {
				snapshot.gripping = true;
				snapshot.gripIndex = sets[s].gripIdx;
				if (sets[s].gripPhase == kGripPull) snapshot.pulling = true;
			}
		}
	}


}

namespace ExactSampleAlternateBattle {
	const int kWindowWidth = 1280;
	const int kWindowHeight = 720;
	const int kPixelScale = 2;                       // 絵の拡大率
	const int kTileSize = 32 * kPixelScale;          // 床・杭の絵1枚の大きさ(px)
	const float kFloorY = 656.0f;                    // 床の上面のY。左右と上の端も壁
	const int kStakeNum = 24;                         // 杭の数(配置は kStakePos)

	struct Vector2 {
		float x;
		float y;
	};

	const char kWindowTitle[] = "触手ウーマン(仮)";

	const float kPi = 3.14159265f;

	//プレイヤー
	const float kPlayerW = 64.0f;
	const float kPlayerH = 128.0f;
	const float kPlayerHalfW = kPlayerW / 2.0f;
	const float kPlayerHalfH = kPlayerH / 2.0f;
	const int kPlayerSpriteW = 32;
	const int kPlayerSpriteH = 64;
	const int kAnimationFrameCount = 6;
	const int kAnimationSpeed = 5;


	// 操作
	const PadButton kPadTentL = kPadButton8;         // L2
	const PadButton kPadTentR = kPadButton9;         // R2
	const PadButton kPadTentL2 = kPadButton14;       // 触手L用の予備ボタン
	const PadButton kPadTentR2 = kPadButton15;       // 触手R用の予備ボタン
	const PadButton kPadJump = kPadButton12;         // X
	const PadButton kPadRelease = kPadButton10;      // A
	const float kStickYSign = 1.0f;                  // スティックを上に倒して狙いが下になるなら -1.0f
	const float kStickDead = 0.25f;                  // スティックのデッドゾーン(0〜1)

	// 触手の色
	const unsigned int kTentColor[2] = { 0xFF78BCFF, 0x62E07AFF };   // 0=L(ピンク) 1=R(緑)

	// 触手とゲート
	const int kTentacleSeg = 96;                     // 触手のパーツ数
	const int kTentacleSpriteW = 16 * kPixelScale;
	const int kTentacleSpriteH = 32 * kPixelScale;
	const float kTentacleBaseHalfSrc = 8.0f;         // 根元の太さ(ドット数・片側)
	const float kTentacleTipHalfSrc = 1.0f;          // 先端の太さ
	const int kTentacleStepRows = 4;                 // 絵を縦に何行ずつ貼るか
	const float kTentacleStep = static_cast<float>(kTentacleStepRows * kPixelScale);
	const int kTentacleMaxSample = 192;
	const int kRetractFrames = 8;
	const int kGateOpenFrames = 6;
	const int kGateCloseFrames = 6;
	const float kGateBackX = 96.0f;
	const float kGateBackY = 112.0f;
	const float kGateFollow = 0.25f;
	const float kGateSplit = 30.0f;                  // 左右のゲートのずらし量(px)
	const int kGateSpriteW = 32 * kPixelScale;
	const int kGateSpriteH = 96 * kPixelScale;
	const float kGateHalfW = kGateSpriteW / 2.0f;
	const float kGateHalfLen = kGateSpriteH / 2.0f;

	const float kKnockFriction = 0.92f;              // 横の反動の減衰率(毎フレーム)
	const int kKnockHoldFrames = 6;                  // 反動が減り始めるまでのフレーム数
	const float kKnockInputRate = 0.25f;             // 反動中の左右入力の効き(1.0で通常)

	// 杭
	const float kGripRadius = 18.0f;                 // 杭の半径
	const int kGripHalfSrc = 2;                      // 巻き付き部分の触手の太さ(目安)
	const float kGripWireHalf = static_cast<float>(kGripHalfSrc * kPixelScale);   // 巻き付き部分の太さ
	const float kGripCoilGap = 4.0f;                 // 1周ごとに増える半径(px)
	const float kGripTurns = 1.0f;                   // 杭に巻く周数
	const int kGripUnwindFrames = 14;                // 巻き解きにかかるフレーム数
	const float kGripThroughSpeed = 15.0f;           // 巻き取り切って杭の向こう側へ跳ぶ速さ(大きいほど遠くまで抜ける)
	const float kGripReleaseDist = 40.0f;            // 杭にこの距離まで近づいたら巻き取り終わり(px)
	const int kGripPayoutFrames = 48;                // 巻き取り切って向こうへ跳んでる間、触手を離さず飛ぶ向きへ伸ばし続けるフレーム数
	const float kGripReturnSpeed = 3.0f;             // 向こうへ跳んでる間、杭から遠ざかる速さがこれを下回ったらすぐ巻き戻す(px/f)
	const float kGripPayoutMax = 300.0f;             // 触手(ロープ)がここまで伸びたらそれ以上伸びない(ここから先はピンと張る)
	const int kGripPullMax = 40;                     // 引っ張りの最大フレーム数(壁に引っかかった時用)
	const float kGripLockCos = 0.5f;                 // 自動照準の角度(cos。0.5で左右60度)
	const int kGripMarkSpriteSize = 32 * kPixelScale;
	const float kGripMarkSize = 72.0f;               // 狙い中のマークのサイズ
	const float kGripMarkSizeSub = 44.0f;            // 届く範囲にあるだけの杭のマークのサイズ
	const int kMaxGripCand = 24;                     // 自動照準の候補の最大数

	// 杭の耐久と引きはがし
	const float kStakeHpMax = 120.0f;                // 杭の耐久
	const int kStakeFullReels = 4;                   // 杭が外れる巻き取り回数
	const float kStakeReelRefDist = 200.0f;          // この距離以上から巻き切ると1回分減る。近いと割合で減る
	const float kPeelNeedLen = 400.0f;               // 引きはがしに必要な2つの杭の距離
	const float kPeelReadyPulse = 0.3f;               // 引きはがせる時の輪の脈動速度
	const float kPullBright = 0.35f;                 // 引っ張り中の触手の明るさ(白に寄せる割合)
	const float kPullGlowSpeed = 0.3f;               // 引っ張り中の光の脈動速度
	const float kPullGlowAlphaMin = 60.0f;           // 光の濃さの最小
	const float kPullGlowAlphaMax = 140.0f;          // 光の濃さの最大
	const int kYankGraceFrames = 24;                 // 引っ張った直後にロープへ引っかからない猶予(f)
	const int kStakeRespawnFrames = 600;             // 外れた杭のリポップまでのフレーム数
	const int kStakePopFrames = 24;                  // リポップ演出のフレーム数
	const int kStakeShakeFrames = 6;                 // 耐久が減った時に震えるフレーム数
	const float kStakeShakePx = 3.0f;                // 震える幅
	const float kStakeDamagedTint = 0.25f;           // 耐久が0に近い杭の暗さ
	const int kPeelPieceFrames = 50;                 // 抜けた杭が飛んで消えるまでのフレーム数
	const float kPeelPieceSpeed = 14.0f;             // 抜けた杭の初速(px/f)
	const int kStakePeelMax = 8;                     // 抜けた杭の同時表示数
	const int kMaxPopup = 8;                         // 文字の同時表示数
	const int kPopupFrames = 50;                     // 文字の表示フレーム数

	// 入力方向の矢印
	const float kAimArrowRadius = 96.0f;             // プレイヤー中心から矢印までの距離
	const float kAimArrowSize = 40.0f;               // 矢印のサイズ
	const int kAimArrowSpriteSize = 16 * kPixelScale; // 1コマのサイズ
	const float kAimArrowFollow = 0.35f;             // 矢印が入力方向へ追従する割合(1フレームで残りのどれだけ進むか)

	// 左下・右下のL/R表示
	const int kLRSpriteSize = 32;                    // ui_lr.png 1コマのサイズ
	const int kCycleSpriteSize = 64;                 // ui_cycle.png のサイズ
	const float kLRUiOffsetX = 58.0f;                // プレイヤー中心からL/Rまでの横距離
	const float kLRUiOffsetY = 40.0f;                // プレイヤー中心からL/Rまでの縦距離(下)
	const float kLRUiSize = 36.0f;                   // L/Rのサイズ
	const float kLRRingSize = 72.0f;                 // 回る矢印のサイズ
	const float kLRRingSpeed = 0.12f;                // 矢印の回転速度
	const float kLRRingPullSpeed = 0.28f;            // 引っ張り中の回転速度
	const float kLRRingFade = 0.25f;                 // 矢印の出現・消滅の速さ
	const float kLRMarkSize = 26.0f;
	// 数字スプライト
	const int kNumSpriteW = 7;                       // 1コマの幅
	const int kNumSpriteH = 9;                       // 1コマの高さ
	const int kNumScale = 2;                         // 拡大率
	const int kNumGap = 3;                           // 数字の間隔


	// ひっかけの段階
	const int kGripReach = 0;                        // 伸びてる
	const int kGripHold = 1;                         // 巻き付いてぶら下がり(ロープ)
	const int kGripPull = 2;                         // 引っ張り中(ボタン押しっぱなし)
	const int kGripUnwind = 3;                       // 巻き解いてしまってる

	// 触手セットの状態
	const int kSetOut = 1;                           // 出てる
	const int kSetRetract = 2;                       // 戻り中(ゲートが閉じる)

	// 構造体
	struct Player {
		Vector2 pos;
		Vector2 Velocity;
		bool isFacingRight;
	};

	// 触手1本
	struct Tentacle {
		float len;            // 現在の長さ
		float phase;          // ゆれの位相
		float retractFrom;    // 戻り始めた時の長さ
	};

	// 1回のひっかけ(ゲート1つ+触手1本)。0=L 1=R
	struct TentacleSet {
		bool isActive;
		int state;            // kSetOut / kSetRetract
		int timer;
		Vector2 gatePos;
		Vector2 gateOffset;   // ゲートの位置(プレイヤー中心から)
		Vector2 dir;          // 杭の向き(長さ1)
		float gateOpen;       // ゲートの開き(0〜1)
		int gripPhase;        // kGripReach / kGripHold / kGripPull / kGripUnwind
		int gripTimer;        // 引っ張り開始からのフレーム数
		Vector2 gripPos;      // 杭の中心
		int gripIdx;          // 掴んでる杭の番号(stakesの添字)
		float gripSpin;       // 巻く向き(±1)
		float gripTense;      // 張り具合(0〜1)。張ってる間はゆれない
		Vector2 pullDir;      // 引っ張る向き(離すとこの向きへ飛ぶ)
		float ropeLen;        // ロープの長さ(これ以上離れられない。引っ張ると縮む)
		float pullStartDist;  // 巻き始めの杭までの距離(巻いた割合の計算用)
		float pullProg;       // 巻いた割合(0〜1)。増えた分だけ耐久が減る
		Tentacle t;           // ゲートから出る触手
	};

	// 杭(ひっかけポイント)
	struct Stake {
		Vector2 pos;          // 杭の中心(画面座標)
		float hp;             // 耐久
		int shakeUntil;       // frameCount がこの値になるまで震える
		int goneFrames;       // 外れてからリポップまでの残りフレーム(0なら外れてない)
		int popFrames;        // リポップ演出の残りフレーム
	};

	// 杭の配置(1画面にまばらに。中心座標)。数を変えたら kStakeNum も変える
	const Vector2 kStakePos[kStakeNum] = {
		{ 170.0f, 420.0f },
		{ 420.0f, 230.0f },
		{ 640.0f, 440.0f },
		{ 660.0f, 120.0f },
		{ 900.0f, 260.0f },
		{ 1110.0f, 440.0f },
		{ 1190.0f, 150.0f },
	};

	// 抜けて飛ぶ杭
	struct PeelStake {
		bool on;
		float x;              // ワールド座標
		float y;
		float vx;
		float vy;
		int t;                // 飛び始めてからのフレーム数
	};

	// 画面に出す文字
	struct StakePopup {
		bool on;
		float x;              // ワールド座標(杭の位置)
		float y;
		int t;
		int kind;             // 0:引きはがし成功 1:ロープが短くて失敗 2:巻きで外れた
	};

	// ひっかけ中の触手の経路
	struct GripPath {
		Vector2 gate;      // 根元
		Vector2 center;    // 杭の中心
		Vector2 tangent;   // 渦の始点(接点)
		float spin;        // 巻く向き
		float r0;          // 巻き終わりの半径(杭+触手の太さ)
		float rEntry;      // 巻き始めの半径
		float tauMax;      // 渦の角度の合計
		float thetaT;      // 接点の角度
		float legLen;      // 直線部分の長さ
		float spiralLen;   // 渦部分の長さ
		float total;       // 全長
	};

	// ゲートから杭までの経路を計算(渦の巻き始めの角度も)
	static void BuildGripPath(const Vector2& gate, const Vector2& center, float spin, GripPath& gp) {
		gp.gate = gate;
		gp.center = center;
		gp.spin = spin;
		gp.r0 = kGripRadius + kGripWireHalf;
		gp.tauMax = kGripTurns * 2.0f * kPi;
		// 外側ほど1周ごとに kGripCoilGap ずつ半径が増える
		gp.rEntry = gp.r0 + kGripTurns * kGripCoilGap;

		// ゲートから円の中心へのベクトル
		float ux = gate.x - center.x;
		float uy = gate.y - center.y;
		float d = sqrtf(ux * ux + uy * uy);
		if (d < 0.001f) {
			ux = 1.0f;
			uy = 0.0f;
			d = 0.001f;
		}
		else {
			ux /= d;
			uy /= d;
		}
		// ゲートから円へ引いた接線の接点の角度
		float phi = 0.0f;
		if (d > gp.rEntry) {
			phi = acosf(gp.rEntry / d);
		}
		// 接点の角度 = ゲートから見た円の中心の角度 + 接線の角度
		gp.thetaT = atan2f(uy, ux) + spin * phi;
		// 接点の座標
		gp.tangent.x = center.x + gp.rEntry * cosf(gp.thetaT);
		gp.tangent.y = center.y + gp.rEntry * sinf(gp.thetaT);
		// ゲートから接点までの距離
		float lx = gp.tangent.x - gate.x;
		float ly = gp.tangent.y - gate.y;
		// 直線部分の長さ
		gp.legLen = sqrtf(lx * lx + ly * ly);
		gp.spiralLen = gp.tauMax * (gp.rEntry + gp.r0) * 0.5f;
		gp.total = gp.legLen + gp.spiralLen;
	}

	// 根元から s(px) 進んだ位置
	static Vector2 GripPathPoint(const GripPath& gp, float s) {
		Vector2 p;
		if (s <= gp.legLen) {
			float t = (gp.legLen > 0.001f) ? s / gp.legLen : 1.0f;
			p.x = gp.gate.x + (gp.tangent.x - gp.gate.x) * t;
			p.y = gp.gate.y + (gp.tangent.y - gp.gate.y) * t;
			return p;
		}
		float sp = s - gp.legLen;
		if (sp > gp.spiralLen) {
			sp = gp.spiralLen;
		}
		// 渦の道のり sp から回転角 tau を求める(半径が角度に比例して縮む)
		float a = (gp.rEntry - gp.r0) / (2.0f * gp.tauMax);
		float tau = 0.0f;
		if (a < 0.00001f) {
			tau = sp / gp.rEntry;
		}
		else {
			float disc = gp.rEntry * gp.rEntry - 4.0f * a * sp;
			if (disc < 0.0f) {
				disc = 0.0f;
			}
			tau = (gp.rEntry - sqrtf(disc)) / (2.0f * a);
		}
		float r = gp.rEntry + (gp.r0 - gp.rEntry) * (tau / gp.tauMax);
		float th = gp.thetaT + gp.spin * tau;
		p.x = gp.center.x + r * cosf(th);
		p.y = gp.center.y + r * sinf(th);
		return p;
	}


	void Frame(bool update, bool draw, bool reset, const ::Vector2* anchors, int anchorCount, ExactPlayerBattle::Snapshot& snapshot) {
		static bool initialized = false;

		// Novice is initialized by the existing game.

		static char keys[256] = { 0 };
		static char preKeys[256] = { 0 };

		if (!initialized) srand(static_cast<unsigned int>(time(nullptr)));

		// スプライト読み込み
		int playerIdleTex = AssetEditor::Handle(AssetEditor::PlayerIdle);
		int playerWalkTex = AssetEditor::Handle(AssetEditor::PlayerWalk);
		int playerJumpTex = AssetEditor::Handle(AssetEditor::PlayerJump);
		int playerFallTex = AssetEditor::Handle(AssetEditor::PlayerFall);
		int tentacleTex = AssetEditor::Handle(AssetEditor::Tentacle);
		int gateTex = AssetEditor::Handle(AssetEditor::Gate);
		static int blockTex = Novice::LoadTexture("./resources/texture/player/video_sample/chip_block.png");

		// 自動照準のマーク(無ければ -1 のまま。仮の丸で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int gripMarkTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/grip_mark.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// 入力方向の矢印(無ければ -1。仮の丸で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int aimArrowTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/aim_arrow.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// L/R表示(無ければ -1。仮の丸で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int lrTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/ui_lr.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// L/Rの周りを回る半円矢印2本(無ければ -1。仮の丸枠で描く)
		// 1フレームごとにLoadTextureするとテクスチャ管理が溢れるため、初回のみ読み込む。
		static const int cycleTex = []() -> int {
			const char* path = "./resources/texture/player/video_sample/ui_cycle.png";
			FILE* fp = nullptr;
			if (fopen_s(&fp, path, "rb") != 0 || !fp) return -1;
			fclose(fp);
			return Novice::LoadTexture(path);
			}();

		// 数字スプライト(無ければ -1。ScreenPrintf で出す)
		static auto LoadOptionalTexture = [](const char* path) -> int {
			FILE* texFile = nullptr;
			fopen_s(&texFile, path, "rb");
			if (texFile != nullptr) {
				fclose(texFile);
				return Novice::LoadTexture(path);
			}
			return -1;
			};
		static int numTex = LoadOptionalTexture("./resources/texture/player/video_sample/ui_number.png");

		// 杭の絵(無ければ -1。仮の茶色い丸で描く)
		static int stakeTex = LoadOptionalTexture("./resources/texture/player/video_sample/chip_grip.png");

		// 杭の絵を左上(x, y)・サイズ size(px)で描く。絵が無ければ何もせず false を返す(呼ぶ側で仮の丸を描く)
		static auto DrawStakeSprite = [&](int x, int y, int size, unsigned int color) -> bool {
			if (stakeTex < 0) {
				return false;
			}
			Novice::DrawQuad(x, y, x + size, y, x, y + size, x + size, y + size,
				0, 0, kTileSize, kTileSize, stakeTex, color);
			return true;
			};



		// 杭(画面に固定)。引き伸ばすと耐久が減って、0で抜ける
		static Stake stakes[kStakeNum];
		static auto ResetStakes = [&]() {
			for (int i = 0; i < kStakeNum; i++) {
				stakes[i].pos = kStakePos[i];
				stakes[i].hp = kStakeHpMax;
				stakes[i].shakeUntil = 0;
				stakes[i].goneFrames = 0;
				stakes[i].popFrames = 0;
			}
			};
		if (!initialized) ResetStakes();

		// プレイヤー
		static Player player = { {200.0f, kFloorY - kPlayerHalfH}, {0.0f, 0.0f}, true };
		static bool jumpReady = true;                // ジャンプが使えるか(着地・杭にひっかける・杭から離れると回復)

		static int animationFrame = 0;
		static int animationTimer = 0;
		static int playerTex = playerIdleTex;


		// 触手
		static TentacleSet sets[2];                  // 0=L 1=R
		if (!initialized) memset(sets, 0, sizeof(sets));
		static float knockX = 0.0f;                  // 離した時などの横の反動
		static int knockTimer = 0;                   // 反動が減り始めるまでの残りフレーム
		static Vector2 reelPos[2] = { {0.0f, 0.0f}, {0.0f, 0.0f} };   // 各触手が最後に巻いた杭の位置(引きはがしの距離判定用)
		static int reelGrace[2] = { 0, 0 };          // 各触手が巻いた直後の残りフレーム(もう片方のロープが引っかからず外れる猶予)

		// 1フレーム前の入力
		static bool preTentHold[2] = { false, false };
		static bool preJumpHold = false;
		static bool preReleaseHold = false;
		static bool isPadDebug = false;              // F1でコントローラーのボタン番号を表示

		// ひっかけポイント
		static int gripMarkSet = -1;                 // 狙いマークを出してる触手(0:L 1:R -1:なし)
		static int gripCand[kMaxGripCand];           // 自動照準の候補(届く範囲の杭)の番号
		static int gripCandCount = 0;
		static bool isGripLock = false;              // 狙ってる杭があるか
		static int gripLockIdx = 0;                  // 狙ってる杭の番号

		// 入力方向の矢印
		static float aimArrowAng = 0.0f;             // 矢印の現在の角度(プレイヤー中心から)
		static float aimArrowAlpha = 0.0f;           // 矢印の濃さ(0〜1)。入力が無いと消える

		// L/R表示:杭に着いてる間まわりを回る矢印
		static float lrRingAng[2] = { 0.0f, 0.0f };  // 回転角
		static float lrRingFade[2] = { 0.0f, 0.0f }; // 出てる度合い(0〜1)

		// 触手を解除(引っ張り中ならその向きへ飛ぶ)。そのあと巻きを解いてしまう
		static auto UnwindSet = [&](int idx) {
			TentacleSet& us = sets[idx];
			if (us.isActive == false || us.state != kSetOut || us.gripPhase == kGripUnwind) {
				return;
			}
			if (us.gripPhase == kGripPull) {
				player.Velocity.y = us.pullDir.y * PlayerTuning::values.gripLaunchSpeed;
				knockX = us.pullDir.x * PlayerTuning::values.gripLaunchSpeed;
				knockTimer = kKnockHoldFrames;
				jumpReady = true;
			}
			us.gripPhase = kGripUnwind;
			};

		// 画面の左右の端にぶつかるまで横へ動く(ぶつかったら true)
		static auto MoveX = [&](float amount) -> bool {
			player.pos.x += amount;
			if (player.pos.x < kPlayerHalfW) {
				player.pos.x = kPlayerHalfW;
				return true;
			}
			if (player.pos.x > kWindowWidth - kPlayerHalfW) {
				player.pos.x = kWindowWidth - kPlayerHalfW;
				return true;
			}
			return false;
			};

		// 床か天井にぶつかるまで縦へ動く(ぶつかったら true)
		static auto MoveY = [&](float amount) -> bool {
			player.pos.y += amount;
			if (player.pos.y + kPlayerHalfH > kFloorY) {
				player.pos.y = kFloorY - kPlayerHalfH;
				return true;
			}
			if (player.pos.y - kPlayerHalfH < 0.0f) {
				player.pos.y = kPlayerHalfH;
				return true;
			}
			return false;
			};

		// 触手の形(パーツの座標)。更新で計算して描画で使う
		static Vector2 pts[2][kTentacleSeg + 1];
		if (!initialized) memset(pts, 0, sizeof(pts));

		static PeelStake stakePeels[kStakePeelMax];   // 抜けて飛ぶ杭
		if (!initialized) memset(stakePeels, 0, sizeof(stakePeels));
		static int stakePeelNext = 0;                 // 次に使う番号

		static StakePopup popups[kMaxPopup];          // PEEL!! などの文字
		if (!initialized) memset(popups, 0, sizeof(popups));
		static int popupNext = 0;

		// 抜けた杭を飛ばす
		static auto SpawnPeel = [&](float x, float y, float vx, float vy) {
			PeelStake& pk = stakePeels[stakePeelNext];
			pk.on = true;
			pk.t = 0;
			pk.x = x;
			pk.y = y;
			pk.vx = vx;
			pk.vy = vy;
			stakePeelNext = (stakePeelNext + 1) % kStakePeelMax;
			};

		// 杭を外してリポップのカウントを始める
		static int stakeRemovedThisFrame = -1;
		static auto RemoveStake = [&](int idx) {
			stakeRemovedThisFrame = idx;
			stakes[idx].goneFrames = kStakeRespawnFrames;
			stakes[idx].popFrames = 0;
			stakes[idx].hp = kStakeHpMax;
			};

		// 文字を出す
		static auto AddPopup = [&](float x, float y, int kind) {
			StakePopup& pp = popups[popupNext];
			pp.on = true;
			pp.x = x;
			pp.y = y;
			pp.t = 0;
			pp.kind = kind;
			popupNext = (popupNext + 1) % kMaxPopup;
			};

		static int frameCount = 0;


		if (!initialized || reset) {
			player = { {420.0f, kFloorY - kPlayerHalfH}, {0.0f,0.0f}, true };
			jumpReady = true;
			memset(sets, 0, sizeof(sets));
			memset(pts, 0, sizeof(pts));
			memset(stakePeels, 0, sizeof(stakePeels));
			memset(popups, 0, sizeof(popups));
			preTentHold[0] = preTentHold[1] = false;
			preJumpHold = preReleaseHold = false;
			knockX = 0;knockTimer = 0;frameCount = 0;
			reelGrace[0] = reelGrace[1] = 0;
			ResetStakes();
			initialized = true;
		}
		// 画面外へ出たボスに接続中の触手だけ解除する。
		if (update && ExactPlayerBattle::releaseBossGripsRequested) {
			for (int h = 0; h < 2; ++h) {
				if (sets[h].isActive && sets[h].state == kSetOut &&
					sets[h].gripIdx >= ExactPlayerBattle::BossAnchorStart()) {
					sets[h].isActive = false;
					sets[h].gripIdx = -1;
					sets[h].gripTense = 0.0f;
					reelGrace[h] = 0;
				}
			}
		}

		if (update) {
			snapshot.yankTarget = -1;
			snapshot.yankGap = 0.0f;
			for (int h = 0; h < 2; ++h) {
				snapshot.loosenTarget[h] = -1;
				snapshot.loosenDelta[h] = 0.0f;
			}
		}
		if (anchors && anchorCount > 0 && update) {
			for (int i = 0;i < kStakeNum;++i) {
				if (i < anchorCount) {
					// Boss uses a world Y-up coordinate system; sample uses screen Y-down.
					stakes[i].pos = { anchors[i].x, 720.0f - anchors[i].y };
					if (i >= ExactPlayerBattle::BossAnchorStart()) {
						// Boss screws remain available until the armor system removes them.
						stakes[i].goneFrames = (anchors[i].x > 10000.0f) ? 999999 : 0;
					}
				}
				else {
					stakes[i].goneFrames = 999999;
				}
			}
			// 第2ステージの流れる背景GPは触手の接続位置も追従させる。
			// 画面端でGPが反対側へループした場合、触手だけ安全に解除する。
			for (int h = 0; h < 2; ++h) {
				const int idx = sets[h].gripIdx;
				if (!sets[h].isActive || idx < 0 || idx >= anchorCount ||
					sets[h].gripPhase == kGripUnwind) continue;
				if (idx < ExactPlayerBattle::movingAnchorCount) {
					const Vector2 newPos = stakes[idx].pos;
					const float dx = newPos.x - sets[h].gripPos.x;
					const float dy = newPos.y - sets[h].gripPos.y;
					// 一瞬で大きく座標が変わるのはGPのループか消滅。
					if (newPos.x > 10000.0f || dx * dx + dy * dy > 200.0f * 200.0f) {
						sets[h].isActive = false;
						sets[h].gripIdx = -1;
						sets[h].gripTense = 0.0f;
						reelGrace[h] = 0;
					}
					else {
						sets[h].gripPos = newPos;
					}
				}
				else if (idx >= ExactPlayerBattle::BossAnchorStart()) {
					// ボスGPは従来通り機体に追従。
					sets[h].gripPos = stakes[idx].pos;
				}
			}
		}
		{



			if (update) {
				stakeRemovedThisFrame = -1;
				memcpy(preKeys, keys, 256);
				Novice::GetHitKeyStateAll(keys);

				///
				/// ↓更新処理ここから
				///
				frameCount++;

				// コントローラー(未接続ならスティック0・ボタン未押下)
				float padX = 0.0f;
				float padY = 0.0f;
				{
					int stickRawX = 0;
					int stickRawY = 0;
					Novice::GetAnalogInputLeft(0, &stickRawX, &stickRawY);
					padX = stickRawX / 32768.0f;
					padY = stickRawY / 32768.0f * kStickYSign;
					if (sqrtf(padX * padX + padY * padY) < kStickDead) {
						padX = 0.0f;
						padY = 0.0f;
					}
				}

				// 操作(キーボードとパッドをまとめる)
				bool tentHold[2];
				tentHold[0] = (keys[DIK_Q] != 0 || keys[DIK_J] != 0 || Novice::IsPressButton(0, kPadTentL) || Novice::IsPressButton(0, kPadTentL2));
				tentHold[1] = (keys[DIK_E] != 0 || keys[DIK_K] != 0 || Novice::IsPressButton(0, kPadTentR) || Novice::IsPressButton(0, kPadTentR2));
				bool jumpHold = (keys[DIK_SPACE] != 0 || Novice::IsPressButton(0, kPadJump));
				bool releaseHold = (keys[DIK_LSHIFT] != 0 || keys[DIK_RSHIFT] != 0 || Novice::IsPressButton(0, kPadRelease));
				float inX = padX;   // 移動と狙い
				float inY = padY;
				if (keys[DIK_D] != 0) { inX += 1.0f; }
				if (keys[DIK_A] != 0) { inX -= 1.0f; }
				if (keys[DIK_S] != 0) { inY += 1.0f; }
				if (keys[DIK_W] != 0) { inY -= 1.0f; }
				{
					float inLen = sqrtf(inX * inX + inY * inY);
					if (inLen > 1.0f) {
						inX /= inLen;
						inY /= inLen;
					}
				}
				if (keys[DIK_F1] != 0 && preKeys[DIK_F1] == 0) {
					isPadDebug = !isPadDebug;
				}
				// 押した瞬間
				bool tentPress[2];
				for (int h = 0; h < 2; h++) {
					tentPress[h] = (tentHold[h] == true && preTentHold[h] == false);
				}
				bool jumpPress = (jumpHold == true && preJumpHold == false);
				bool releasePress = (releaseHold == true && preReleaseHold == false);

				// プレイヤーの移動
				player.Velocity.x = inX * PlayerTuning::values.moveSpeed;
				if (inX > 0.1f) {
					player.isFacingRight = true;
				}
				else if (inX < -0.1f) {
					player.isFacingRight = false;
				}

				// 向こうへ跳んでる間は横の勢いを削らない(縦は重力で落ちるのに横だけ止まると、空中で止まって見える)
				bool isPayout = false;
				for (int h = 0; h < 2; h++) {
					if (sets[h].isActive == true && sets[h].gripPhase == kGripHold && sets[h].gripTimer > 0) {
						isPayout = true;
					}
				}

				// 横の反動(触手を離した時などの勢い。飛んでる間は逆向きの入力が弱まり、反動は徐々に減る)
				if (knockX != 0.0f) {
					float inputX = player.Velocity.x;
					if (inputX * knockX < 0.0f) {
						float strength = fabsf(knockX) / PlayerTuning::values.gripLaunchSpeed;   // 離した直後は1.0、反動が終わる頃は0.0
						if (strength > 1.0f) {
							strength = 1.0f;
						}
						inputX *= 1.0f - strength * (1.0f - kKnockInputRate);
					}
					player.Velocity.x = inputX + knockX;

					// 最初の kKnockHoldFrames フレームは保って、その後減らす
					if (knockTimer > 0) {
						knockTimer--;
					}
					else if (isPayout == false) {
						knockX *= kKnockFriction;
						if (fabsf(knockX) < 0.5f) {
							knockX = 0.0f;
						}
					}
				}

				// 足元が床か
				bool isGround = (player.pos.y + kPlayerHalfH + 1.0f >= kFloorY);

				// 着地でジャンプ回復・反動も止める
				if (isGround == true) {
					jumpReady = true;
					if (knockTimer <= 0) {
						knockX = 0.0f;
					}
				}

				// ジャンプ
				if (jumpPress == true && jumpReady == true) {
					player.Velocity.y = -PlayerTuning::values.jumpPower;
					jumpReady = false;
				}
				if (jumpHold == false && preJumpHold == true) {
					if (player.Velocity.y < -PlayerTuning::values.jumpCut) {
						player.Velocity.y = -PlayerTuning::values.jumpCut;
					}
				}

				// 狙う向き。何も倒してなければ向いてる方向
				float aimLen = sqrtf(inX * inX + inY * inY);
				bool hasAimDir = (aimLen > 0.01f);
				float aimDx = 0.0f;
				float aimDy = 0.0f;
				if (hasAimDir == true) {
					aimDx = inX / aimLen;
					aimDy = inY / aimLen;
				}

				// 狙う杭の探し方:届く範囲の杭のうち、狙う向きの左右60度以内で、近くて向きが合うもの。
				// hasExclude が true なら exIdx の杭は除外(もう片方が掴んでる杭)。candIdx には届く範囲のものを全部入れる
				auto FindLock = [&](bool hasExclude, int exIdx, int* candIdx, int& candCount, int& lockIdx) -> bool {
					candCount = 0;
					lockIdx = 0;
					bool isLock = false;
					float adx = aimDx;
					float ady = aimDy;
					float lockCos = kGripLockCos;
					if (hasAimDir == false) {
						// 何も倒してない時:向いてる方向を基準に、どの向きの杭でも狙える(近くて向きが合うものを優先)
						adx = player.isFacingRight ? 1.0f : -1.0f;
						ady = 0.0f;
						lockCos = -1.1f;
						// もう片方が掴んでたら、その反対側を基準にする
						if (hasExclude == true) {
							float awayX = player.pos.x - stakes[exIdx].pos.x;
							float awayY = player.pos.y - stakes[exIdx].pos.y;
							float awayLen = sqrtf(awayX * awayX + awayY * awayY);
							if (awayLen > 1.0f) {
								adx = awayX / awayLen;
								ady = awayY / awayLen;
							}
						}
					}

					// 近くて向きが合う杭を探す。1.0e9f はスコアの初期値(大きいほど悪い)
					float bestScore = 1.0e9f;
					for (int i = 0; i < kStakeNum; i++) {
						if (stakes[i].goneFrames > 0) {
							continue;   // 外れた杭は狙えない
						}
						if (hasExclude == true && i == exIdx) {
							continue;
						}
						float vx = stakes[i].pos.x - player.pos.x;
						float vy = stakes[i].pos.y - player.pos.y;
						float dist = sqrtf(vx * vx + vy * vy);
						// 背景GPは従来の照準判定を維持し、ボスGPだけアシストする。
						const bool bossGp = (i >= ExactPlayerBattle::BossAnchorStart());
						const float baseRange = PlayerTuning::values.lockRange;
						const float lockRange = baseRange * (bossGp ? BossCombatEditor::values.bossGripRangeMultiplier : 1.0f);
						if (dist > lockRange || dist < 40.0f) {
							continue;
						}
						if (candCount < kMaxGripCand) {
							candIdx[candCount] = i;
							candCount++;
						}
						float cosA = (vx * adx + vy * ady) / dist;
						const float targetCos = bossGp && hasAimDir ? BossCombatEditor::values.bossGripAimCos : lockCos;
						if (cosA >= targetCos) {
							// 近いほど、向きが合うほど優先
							float score = dist + (1.0f - cosA) * 400.0f;
							if (bossGp) score -= BossCombatEditor::values.bossGripPriorityBonus;
							if (score < bestScore) {
								bestScore = score;
								isLock = true;
								lockIdx = i;
							}
						}
					}
					return isLock;
					};

				// 触手が出てるか(伸び・ロープ・引っ張り中。しまい中は含まない)
				bool isOut[2];
				for (int h = 0; h < 2; h++) {
					isOut[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind);
				}

				// 狙いマーク:まだ出してない触手が狙う杭を出す(もう片方が掴んでる杭は除く)
				gripCandCount = 0;
				isGripLock = false;
				{
					int markSet = -1;
					for (int h = 0; h < 2; h++) {
						if (isOut[h] == false) {
							markSet = h;
							break;
						}
					}
					gripMarkSet = markSet;
					if (markSet >= 0) {
						int markOther = 1 - markSet;
						bool markHasEx = isOut[markOther];
						int markExIdx = sets[markOther].gripIdx;
						isGripLock = FindLock(markHasEx, markExIdx, gripCand, gripCandCount, gripLockIdx);
					}
				}

				// 入力方向の矢印:入力方向へプレイヤーの周りを回り込む(近いほうまわり)
				if (hasAimDir == true) {
					float targetAng = atan2f(inY, inX);
					if (aimArrowAlpha <= 0.01f) {
						aimArrowAng = targetAng;   // 出始めはその向きにいきなり出す
					}
					float diffAng = targetAng - aimArrowAng;
					while (diffAng > kPi) { diffAng -= 2.0f * kPi; }
					while (diffAng < -kPi) { diffAng += 2.0f * kPi; }
					aimArrowAng += diffAng * kAimArrowFollow;
					aimArrowAlpha += 0.3f;
					if (aimArrowAlpha > 1.0f) { aimArrowAlpha = 1.0f; }
				}
				else {
					aimArrowAlpha -= 0.2f;
					if (aimArrowAlpha < 0.0f) { aimArrowAlpha = 0.0f; }
				}

				// L/Rの矢印:杭に着いてる間(ロープ・引っ張り中)だけ出て回る
				for (int h = 0; h < 2; h++) {
					bool isHooked = (sets[h].isActive == true && sets[h].state == kSetOut
						&& (sets[h].gripPhase == kGripHold || sets[h].gripPhase == kGripPull));
					if (isHooked == true) {
						lrRingAng[h] += (sets[h].gripPhase == kGripPull) ? kLRRingPullSpeed : kLRRingSpeed;
						if (lrRingAng[h] > 2.0f * kPi) { lrRingAng[h] -= 2.0f * kPi; }
						lrRingFade[h] += kLRRingFade;
						if (lrRingFade[h] > 1.0f) { lrRingFade[h] = 1.0f; }
					}
					else {
						lrRingFade[h] -= kLRRingFade;
						if (lrRingFade[h] <= 0.0f) {
							lrRingFade[h] = 0.0f;
							lrRingAng[h] = 0.0f;
						}
					}
				}

				// 触手の発射・解除
				// Aボタン(LSHIFT):出てる触手を全部解除
				if (releasePress == true) {
					for (int h = 0; h < 2; h++) {
						UnwindSet(h);
					}
					for (int h = 0; h < 2; h++) {
						isOut[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind);
					}
				}

				//1回目で射出して掴む(押しっぱなしで引っ張る)。ロープ状態でもう一度押すと巻き取り
				for (int h = 0; h < 2; h++) {
					if (tentPress[h] == false) {
						continue;
					}
					if (isOut[h] == true) {
						if (sets[h].gripPhase == kGripHold) {
							// ロープでぶら下がり中にもう一度押したら離さず杭へ巻き取る(押してる間だけ。離すとロープに戻る)
							// 杭の反対側へ抜けたあとでも同じ触手でまた巻ける(交互にするのは別の杭へ移る時だけ)
							TentacleSet& rs2 = sets[h];
							float reelDx = rs2.gripPos.x - player.pos.x;
							float reelDy = rs2.gripPos.y - player.pos.y;
							float reelDist = sqrtf(reelDx * reelDx + reelDy * reelDy);
							if (reelDist > 1.0f) {
								rs2.pullDir.x = reelDx / reelDist;
								rs2.pullDir.y = reelDy / reelDist;
							}
							rs2.gripPhase = kGripPull;
							rs2.gripTimer = 0;
							rs2.pullStartDist = reelDist;
							rs2.pullProg = 0.0f;
						}
						else {
							UnwindSet(h);
							isOut[h] = false;
						}
						continue;
					}

					// もう片方が掴んでる杭は狙わない。左右で別々の杭に掛けられる
					int other = 1 - h;
					bool hasEx = isOut[other];
					int exIdx = sets[other].gripIdx;
					int shotCand[kMaxGripCand];
					int shotCandCount = 0;
					int shotIdx = 0;
					bool isShotLock = FindLock(hasEx, exIdx, shotCand, shotCandCount, shotIdx);
					if (isShotLock == false) {
						continue;
					}

					// ゲートは杭の反対側。触手1本が杭へ伸びて巻き付く
					TentacleSet& ts = sets[h];
					memset(&ts, 0, sizeof(ts));
					ts.isActive = true;
					ts.state = kSetOut;
					ts.timer = 0;
					ts.gripPhase = kGripReach;
					ts.gripIdx = shotIdx;
					ts.gripPos = stakes[shotIdx].pos;
					float toX = ts.gripPos.x - player.pos.x;
					float toY = ts.gripPos.y - player.pos.y;
					float toLen = sqrtf(toX * toX + toY * toY);
					if (toLen < 1.0f) {
						toLen = 1.0f;
					}
					ts.dir.x = toX / toLen;
					ts.dir.y = toY / toLen;
					ts.pullDir = ts.dir;
					ts.ropeLen = toLen;
					ts.gripSpin = (ts.dir.x >= 0.0f) ? 1.0f : -1.0f;
					float sideSign = (h == 0) ? -1.0f : 1.0f;   // 左の触手は左側、右の触手は右側のゲートから出る
					ts.gateOffset.x = -ts.dir.x * kGateBackX - ts.dir.y * sideSign * kGateSplit;
					ts.gateOffset.y = -ts.dir.y * kGateBackY + ts.dir.x * sideSign * kGateSplit;
					ts.gatePos.x = player.pos.x + ts.gateOffset.x;
					ts.gatePos.y = player.pos.y + ts.gateOffset.y;
					ts.t.phase = static_cast<float>(rand() % 628) / 100.0f;
					isOut[h] = true;
				}

				// プレイヤー(重力)
				player.Velocity.y += PlayerTuning::values.gravity;
				if (player.Velocity.y > PlayerTuning::values.maxFallSpeed) {
					player.Velocity.y = PlayerTuning::values.maxFallSpeed;
				}

				// 引っ張り中は重力もキー入力も無視して杭へ一直線(2本とも引いてるなら向きを平均)
				int pullCount = 0;
				float pullSumX = 0.0f;
				float pullSumY = 0.0f;
				for (int h = 0; h < 2; h++) {
					TentacleSet& ps = sets[h];
					if (ps.isActive == false || ps.gripPhase != kGripPull) {
						continue;
					}
					float pullDx = ps.gripPos.x - player.pos.x;
					float pullDy = ps.gripPos.y - player.pos.y;
					float pullDist = sqrtf(pullDx * pullDx + pullDy * pullDy);
					if (pullDist > 1.0f) {
						ps.pullDir.x = pullDx / pullDist;
						ps.pullDir.y = pullDy / pullDist;
					}
					pullSumX += ps.pullDir.x;
					pullSumY += ps.pullDir.y;
					pullCount++;
				}
				// 触手ごとに、巻いてる間とそのあとの勢いで飛んでる間を数える(もう片方の引きはがし判定用)
				for (int h = 0; h < 2; h++) {
					if (sets[h].isActive == true && sets[h].gripPhase == kGripPull) {
						reelGrace[h] = kYankGraceFrames;
						reelPos[h] = sets[h].gripPos;
					}
					else if (reelGrace[h] > 0) {
						reelGrace[h]--;
					}
				}
				if (pullCount > 0) {
					pullSumX /= pullCount;
					pullSumY /= pullCount;
					player.Velocity.x = pullSumX * PlayerTuning::values.gripPullSpeed;
					player.Velocity.y = pullSumY * PlayerTuning::values.gripPullSpeed;
					knockX = 0.0f;
					knockTimer = 0;
					if (pullSumX > 0.1f) {
						player.isFacingRight = true;
					}
					else if (pullSumX < -0.1f) {
						player.isFacingRight = false;
					}
				}

				// 移動(壁で止まる)
				MoveX(player.Velocity.x);
				if (MoveY(player.Velocity.y) == true) {
					player.Velocity.y = 0.0f;
				}

				// 掴んでる間はその長さより遠くへ離れられない
				// もう片方が巻いてる間は巻きが優先。
				// 2つの杭の距離が kPeelNeedLen 以上なら杭が抜ける。足りなければ触手が外れる
				for (int h = 0; h < 2; h++) {
					TentacleSet& rs = sets[h];
					if (rs.isActive == false) {
						continue;
					}
					float ropeDx = player.pos.x - rs.gripPos.x;
					float ropeDy = player.pos.y - rs.gripPos.y;
					float ropeDist = sqrtf(ropeDx * ropeDx + ropeDy * ropeDy);
					if (rs.gripPhase == kGripPull) {
						// 引っ張られて近づく間はロープも縮む
						if (ropeDist < rs.ropeLen) {
							rs.ropeLen = ropeDist;
						}
						continue;
					}
					if (rs.gripPhase != kGripHold || ropeDist <= rs.ropeLen || ropeDist < 1.0f) {
						continue;
					}

					// 引きはがし判定は、もう片方が巻いてる時だけ。自分で巻いて近づいただけでは判定しない
					int otherIdx = 1 - h;
					bool isYank = ((sets[otherIdx].isActive == true && sets[otherIdx].gripPhase == kGripPull) || reelGrace[otherIdx] > 0);
					if (isYank == true) {
						// 限界まで伸びたら自動で外れる。もう片方の杭とこの杭が遠ければ抜く
						float peelGapX = rs.gripPos.x - reelPos[otherIdx].x;
						float peelGapY = rs.gripPos.y - reelPos[otherIdx].y;
						const float gap = sqrtf(peelGapX * peelGapX + peelGapY * peelGapY);
						if (rs.gripIdx >= ExactPlayerBattle::BossAnchorStart()) {
							// Boss screws are never destroyed by the sample's stake HP.
							// Emit the same two-arm tug for the armor tension controller.
							if (sets[otherIdx].gripIdx >= 0 &&
								sets[otherIdx].gripIdx < ExactPlayerBattle::BossAnchorStart()) {
								snapshot.yankTarget = rs.gripIdx;
								snapshot.yankGap = gap;
							}
							stakes[rs.gripIdx].shakeUntil = frameCount + kStakeShakeFrames;
						}
						else if (gap >= kPeelNeedLen) {
							SpawnPeel(rs.gripPos.x, rs.gripPos.y, ropeDx / ropeDist * kPeelPieceSpeed, ropeDy / ropeDist * kPeelPieceSpeed - 3.0f);
							RemoveStake(rs.gripIdx);
							AddPopup(rs.gripPos.x, rs.gripPos.y, 0);
						}
						else {
							stakes[rs.gripIdx].shakeUntil = frameCount + kStakeShakeFrames;
							AddPopup(rs.gripPos.x, rs.gripPos.y, 1);
						}
						rs.gripPhase = kGripUnwind;
						continue;
					}

					// 長さの所まで引き戻して、外向きの勢いを消す
					float ropeNx = ropeDx / ropeDist;
					float ropeNy = ropeDy / ropeDist;
					if (rs.gripTimer > 0) {
						// 巻き取り切って向こうへ跳んでる間:触手は離さず、飛ぶ向き(杭の反対側)へ伸びていく。減速は重力だけ(山なりに飛ぶ)
						// 山なりの頂点で杭から遠ざかる速さが落ちた時か、ロープが伸びきった時は、時間切れを待たずに終わらせる(往復のラグをなくす)
						float outSpeed = knockX * ropeNx + player.Velocity.y * ropeNy;
						if ((outSpeed < kGripReturnSpeed || ropeDist >= kGripPayoutMax) && rs.gripTimer < kGripPayoutFrames - 6 && rs.gripTimer > 1) {
							rs.gripTimer = 1;
						}
						if (ropeDist < kGripPayoutMax) {
							rs.ropeLen = ropeDist;
							continue;
						}
					}
					float ropeTargetX = rs.gripPos.x + ropeNx * rs.ropeLen;
					float ropeTargetY = rs.gripPos.y + ropeNy * rs.ropeLen;
					MoveX(ropeTargetX - player.pos.x);
					MoveY(ropeTargetY - player.pos.y);
					float outSpeed = player.Velocity.x * ropeNx + player.Velocity.y * ropeNy;
					if (outSpeed > 0.0f) {
						player.Velocity.y -= ropeNy * outSpeed;
						if (knockX * ropeNx > 0.0f) {
							knockX = 0.0f;
						}
					}
				}



				// プレイヤーのアニメーション
				if (isGround == false && player.Velocity.y < 0.0f) {
					playerTex = playerJumpTex;
				}
				else if (isGround == false) {
					playerTex = playerFallTex;
				}
				else if (player.Velocity.x != 0.0f) {
					playerTex = playerWalkTex;
				}
				else {
					playerTex = playerIdleTex;
				}

				animationTimer++;
				if (animationTimer >= kAnimationSpeed) {
					animationTimer = 0;
					animationFrame++;
					if (animationFrame >= kAnimationFrameCount) {
						animationFrame = 0;
					}
				}


				// 触手とゲートの更新
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false) {
						continue;
					}

					ts.timer++;

					// ゲートはプレイヤーに少し遅れてついてくる
					{
						float followX = player.pos.x + ts.gateOffset.x;
						float followY = player.pos.y + ts.gateOffset.y;
						// 画面外に出ないよう制限
						if (followX < 16.0f) { followX = 16.0f; }
						if (followX > kWindowWidth - 16.0f) { followX = kWindowWidth - 16.0f; }
						if (followY < 16.0f) { followY = 16.0f; }
						if (followY > kWindowHeight - 16.0f) { followY = kWindowHeight - 16.0f; }
						ts.gatePos.x += (followX - ts.gatePos.x) * kGateFollow;
						ts.gatePos.y += (followY - ts.gatePos.y) * kGateFollow;
					}

					if (ts.state == kSetOut) {
						// ゲートの開き
						ts.gateOpen = static_cast<float>(ts.timer) / kGateOpenFrames;
						if (ts.gateOpen > 1.0f) {
							ts.gateOpen = 1.0f;
						}

						// ゲートの口は常に杭のほうを向く(2つは左右にずらす)
						float gx = ts.gripPos.x - ts.gatePos.x;
						float gy = ts.gripPos.y - ts.gatePos.y;
						float gl = sqrtf(gx * gx + gy * gy);
						if (gl > 1.0f) {
							ts.dir.x = gx / gl;
							ts.dir.y = gy / gl;
						}
						float sideSign = (s == 0) ? -1.0f : 1.0f;
						ts.gateOffset.x = -ts.dir.x * kGateBackX - ts.dir.y * sideSign * kGateSplit;
						ts.gateOffset.y = -ts.dir.y * kGateBackY + ts.dir.x * sideSign * kGateSplit;

						GripPath gp;
						BuildGripPath(ts.gatePos, ts.gripPos, ts.gripSpin, gp);

						Tentacle& gt = ts.t;   // 杭に巻き付く触手
						if (ts.gripPhase == kGripReach) {
							// 杭の接点までまっすぐ伸びる
							gt.len += PlayerTuning::values.gripReachSpeed;
							if (gt.len >= gp.legLen) {
								gt.len = gp.legLen;
								// ボタン押しっぱなしなら引っ張る。離してたらロープでつながる
								float reachDx = ts.gripPos.x - player.pos.x;
								float reachDy = ts.gripPos.y - player.pos.y;
								float reachDist = sqrtf(reachDx * reachDx + reachDy * reachDy);
								if (reachDist > 1.0f) {
									ts.pullDir.x = reachDx / reachDist;
									ts.pullDir.y = reachDy / reachDist;
								}
								ts.ropeLen = reachDist;
								jumpReady = true;
								if (tentHold[s] == true) {
									ts.gripPhase = kGripPull;
									ts.gripTimer = 0;
									ts.pullStartDist = reachDist;
									ts.pullProg = 0.0f;
								}
								else {
									ts.gripPhase = kGripHold;
								}
							}
						}
						else if (ts.gripPhase == kGripHold) {
							// 巻き付いたまま。伸びきってる間はピンと張る
							gt.len = gp.total;
							if (ts.gripTimer > 0) {
								ts.gripTimer--;   // 向こうへ跳んでる間、触手を伸ばし続ける残りフレーム
								if (ts.gripTimer == 0 && tentHold[s] == true) {
									// 長押し中:向こう側へ跳び終わったら、また杭へ巻き取る(押してる間、杭を挟んで往復する)
									float backDx = ts.gripPos.x - player.pos.x;
									float backDy = ts.gripPos.y - player.pos.y;
									float backDist = sqrtf(backDx * backDx + backDy * backDy);
									if (backDist > 1.0f) {
										ts.pullDir.x = backDx / backDist;
										ts.pullDir.y = backDy / backDist;
									}
									ts.gripPhase = kGripPull;
									ts.gripTimer = 0;
									ts.pullStartDist = backDist;
									ts.pullProg = 0.0f;
								}
							}
							float holdDx = player.pos.x - ts.gripPos.x;
							float holdDy = player.pos.y - ts.gripPos.y;
							float holdDist = sqrtf(holdDx * holdDx + holdDy * holdDy);
							float tenseTarget = (holdDist >= ts.ropeLen - 3.0f) ? 1.0f : 0.0f;
							ts.gripTense += (tenseTarget - ts.gripTense) * 0.3f;
						}
						else if (ts.gripPhase == kGripPull) {
							// ボタンを離したら勢いを残してロープに戻る。杭に近づいたら離して杭の向きへ飛ぶ
							ts.gripTimer++;
							float pullDx = ts.gripPos.x - player.pos.x;
							float pullDy = ts.gripPos.y - player.pos.y;
							float pullDist = sqrtf(pullDx * pullDx + pullDy * pullDy);

							// 巻きのダメージ:巻いた割合の2乗 × 距離
							// 距離の割合はkStakeReelRefDist 以上から巻けば1.0。
							// 杭の手前まで巻き切る長押しを kStakeFullReels 回やると耐久がちょうど0
							bool isDropped = false;
							{
								float reelSpan = ts.pullStartDist - kGripReleaseDist;
								if (reelSpan > 1.0f) {
									float reelProg = (ts.pullStartDist - pullDist) / reelSpan;
									if (reelProg > 1.0f) { reelProg = 1.0f; }
									if (reelProg > ts.pullProg) {
										float distRate = reelSpan / kStakeReelRefDist;
										if (distRate > 1.0f) { distRate = 1.0f; }
										if (distRate < 0.2f) { distRate = 0.2f; }
										float reelDmg = kStakeHpMax / kStakeFullReels * distRate * (reelProg * reelProg - ts.pullProg * ts.pullProg);
										ts.pullProg = reelProg;
										Stake& gs = stakes[ts.gripIdx];
										if (ts.gripIdx >= ExactPlayerBattle::BossAnchorStart()) {
											snapshot.loosenTarget[s] = ts.gripIdx;
											snapshot.loosenDelta[s] += reelDmg / kStakeHpMax;
										}
										else {
											gs.hp -= reelDmg;
										}
										if (reelDmg > 0.05f) {
											gs.shakeUntil = frameCount + kStakeShakeFrames;
										}
										if (gs.hp <= 0.5f) {   // 計算誤差で少し残らないように
											// 外れる:引っ張りの向きへ少し飛びながら落ちる。勢いは残して触手をしまう
											SpawnPeel(ts.gripPos.x, ts.gripPos.y, static_cast<float>(rand() % 5 - 2), -5.0f);
											RemoveStake(ts.gripIdx);
											AddPopup(ts.gripPos.x, ts.gripPos.y, 2);
											player.Velocity.y = ts.pullDir.y * PlayerTuning::values.gripLaunchSpeed;
											knockX = ts.pullDir.x * PlayerTuning::values.gripLaunchSpeed;
											knockTimer = kKnockHoldFrames;
											jumpReady = true;
											ts.gripPhase = kGripUnwind;
											isDropped = true;
										}
									}
								}
							}

							if (isDropped == true) {
								// もう外れた
							}
							else if (tentHold[s] == false || ts.gripTimer >= kGripPullMax) {
								player.Velocity.y = ts.pullDir.y * PlayerTuning::values.gripPullSpeed;
								knockX = ts.pullDir.x * PlayerTuning::values.gripPullSpeed;
								knockTimer = kKnockHoldFrames;
								ts.ropeLen = pullDist;
								ts.gripTimer = 0;
								ts.gripPhase = kGripHold;
							}
							else if (pullDist < kGripReleaseDist) {
								// 巻き取り切った:勢いのまま杭の向こう側へ跳ぶ。触手は離さず、飛ぶ向きへ伸びていく
								player.Velocity.y = ts.pullDir.y * kGripThroughSpeed;
								knockX = ts.pullDir.x * kGripThroughSpeed;
								knockTimer = kKnockHoldFrames;
								jumpReady = true;
								ts.ropeLen = pullDist;
								ts.gripTimer = kGripPayoutFrames;
								ts.gripPhase = kGripHold;
							}
						}
						else if (ts.gripPhase == kGripUnwind) {
							// 巻きを解きながらゲートへしまう
							gt.len -= gp.total / kGripUnwindFrames;
							if (gt.len <= 0.0f) {
								gt.len = 0.0f;
								ts.state = kSetRetract;
								ts.timer = 0;
								ts.t.retractFrom = ts.t.len;
								continue;
							}
						}

						// 引っ張り中:長さは変えない。ゲートが杭に近づいて直線部分が短くなった分、先端が杭に巻き付く。ピンと張ってゆれない
						if (ts.gripPhase == kGripPull) {
							if (gt.len > gp.total) {
								gt.len = gp.total;
							}
							if (gt.len < gp.legLen) {
								gt.len = gp.legLen;
							}
							ts.gripTense += 0.25f;
							if (ts.gripTense > 1.0f) { ts.gripTense = 1.0f; }
						}
						else if (ts.gripPhase == kGripUnwind) {
							ts.gripTense -= 0.15f;
							if (ts.gripTense < 0.0f) { ts.gripTense = 0.0f; }
						}

					}
					else if (ts.state == kSetRetract) {
						// 触手をゲートへ戻す
						if (ts.timer <= kRetractFrames) {
							float p = static_cast<float>(ts.timer) / kRetractFrames;
							// 触手の長さを戻す
							ts.t.len = ts.t.retractFrom * (1.0f - p * p);
						}
						else {
							// 触手を完全にしまう
							ts.t.len = 0.0f;
							// ゲートを閉じる
							ts.gateOpen = 1.0f - static_cast<float>(ts.timer - kRetractFrames) / kGateCloseFrames;
							if (ts.gateOpen <= 0.0f) {
								ts.gateOpen = 0.0f;
								ts.isActive = false;
							}
						}
					}
				}

				// 触手の形を計算
				// s:触手セットの番号(1セット1本)
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false) {
						continue;
					}
					// ゲート → 杭の接点 → 渦、の経路に沿って点を並べる
					Tentacle& tn = ts.t;
					GripPath gp;
					BuildGripPath(ts.gatePos, ts.gripPos, ts.gripSpin, gp);
					float visLen = tn.len;
					if (visLen > gp.total) {
						visLen = gp.total;
					}
					for (int i = 0; i <= kTentacleSeg; i++) {
						float sArc = visLen * static_cast<float>(i) / kTentacleSeg;
						Vector2 p = GripPathPoint(gp, sArc);
						// 直線部分だけゆらす(両端は動かさない。引っ張り中・ピンと張ってる間はゆれない)
						if (sArc < gp.legLen && gp.legLen > 1.0f) {
							float u = sArc / gp.legLen;
							float amp = 12.0f * sinf(kPi * u) * (1.0f - ts.gripTense);
							float side = sinf(frameCount * 0.15f + tn.phase + u * 6.0f) * amp;
							float lx = (gp.tangent.x - gp.gate.x) / gp.legLen;
							float ly = (gp.tangent.y - gp.gate.y) / gp.legLen;
							p.x += -ly * side;
							p.y += lx * side;
						}
						pts[s][i] = p;
					}
				}

				// 抜けた杭を元に戻す(デバッグ用)
				if (keys[DIK_R] != 0 && preKeys[DIK_R] == 0) {
					ResetStakes();
				}

				// 外れた杭のリポップ
				for (int i = 0; i < kStakeNum; i++) {
					Stake& stk = stakes[i];
					if (stk.goneFrames > 0) {
						stk.goneFrames--;
						if (stk.goneFrames == 0) {
							stk.hp = kStakeHpMax;
							stk.shakeUntil = 0;
							stk.popFrames = kStakePopFrames;
						}
					}
					if (stk.popFrames > 0) {
						stk.popFrames--;
					}
				}

				// 画面に出す文字
				for (int i = 0; i < kMaxPopup; i++) {
					if (popups[i].on == true) {
						popups[i].t++;
						if (popups[i].t >= kPopupFrames) {
							popups[i].on = false;
						}
					}
				}

				// 抜けて飛ぶ杭
				for (int i = 0; i < kStakePeelMax; i++) {
					if (stakePeels[i].on == true) {
						stakePeels[i].x += stakePeels[i].vx;
						stakePeels[i].y += stakePeels[i].vy;
						stakePeels[i].vx *= 0.97f;
						stakePeels[i].vy += 0.6f;
						stakePeels[i].t++;
						if (stakePeels[i].t >= kPeelPieceFrames) {
							stakePeels[i].on = false;
						}
					}
				}

				// 1フレーム前の入力を保存
				preTentHold[0] = tentHold[0];
				preTentHold[1] = tentHold[1];
				preJumpHold = jumpHold;
				preReleaseHold = releaseHold;

				///
				/// ↑更新処理ここまで
				///

				///
			}
			if (draw) {
				/// ↓描画処理ここから
				///

				// 背景
				// stage draws background, preserve sample character and tentacle rendering.

				auto RoundPx = [](float v) -> int {
					return static_cast<int>(floorf(v + 0.5f));
					};

				// 絵をcx, cy・size・ang で描く
				auto DrawRotSprite = [&](int tex, float cx, float cy, float size, float ang, int srcX, int srcY, int srcW, int srcH, unsigned int col) {
					float half = size * 0.5f;
					float c = cosf(ang);
					float sn = sinf(ang);
					Novice::DrawQuad(
						RoundPx(cx + (-half) * c - (-half) * sn), RoundPx(cy + (-half) * sn + (-half) * c),
						RoundPx(cx + half * c - (-half) * sn), RoundPx(cy + half * sn + (-half) * c),
						RoundPx(cx + (-half) * c - half * sn), RoundPx(cy + (-half) * sn + half * c),
						RoundPx(cx + half * c - half * sn), RoundPx(cy + half * sn + half * c),
						srcX, srcY, srcW, srcH, tex, col);
					};

				// L/R表示をcx, cyに描く(setIdx 0=L 1=R)。
				// 触手の色で色分け。
				auto DrawLRBadge = [&](int setIdx, float cx, float cy, float size, unsigned int col) {
					if (lrTex >= 0) {
						DrawRotSprite(lrTex, cx, cy, size, 0.0f, setIdx * kLRSpriteSize, 0, kLRSpriteSize, kLRSpriteSize, col);
					}
					else {
						int rad = static_cast<int>(size * 0.5f);
						Novice::DrawEllipse(RoundPx(cx), RoundPx(cy), rad, rad, 0.0f, col, kFillModeSolid);
					}
					};

				// L/Rの周りを回る半円矢印2本(ang:回転角)。絵が無ければ丸枠
				auto DrawCycleRing = [&](float cx, float cy, float size, float ang, unsigned int col) {
					if (cycleTex >= 0) {
						DrawRotSprite(cycleTex, cx, cy, size, ang, 0, 0, kCycleSpriteSize, kCycleSpriteSize, col);
					}
					else {
						float rad = size * 0.5f;
						Novice::DrawEllipse(RoundPx(cx), RoundPx(cy), static_cast<int>(rad), static_cast<int>(rad), 0.0f, col, kFillModeWireFrame);
					}
					};

				// 点線の輪(外れた杭の跡)
				auto DrawDashRing = [&](float cx, float cy, float r, float rot, unsigned int col) {
					const int dashN = 16;
					for (int i = 0; i < dashN; i++) {
						float a0 = rot + i * (2.0f * kPi / dashN);
						float a1 = a0 + (2.0f * kPi / dashN) * 0.55f;
						for (int th = 0; th < 2; th++) {
							float rr = r + th;
							Novice::DrawLine(RoundPx(cx + cosf(a0) * rr), RoundPx(cy + sinf(a0) * rr),
								RoundPx(cx + cosf(a1) * rr), RoundPx(cy + sinf(a1) * rr), col);
						}
					}
					};

				// Floor supplied by stage scene.

				// 杭(ひっかけポイント)
				for (int si = 0; si < kStakeNum; si++) {
					Stake& stk = stakes[si];
					int px = RoundPx(stk.pos.x) - kTileSize / 2;
					int py = RoundPx(stk.pos.y) - kTileSize / 2;
					// 外れた杭の跡:薄い杭の影 + 点線の輪 + リポップまでの残り秒数を、元の場所に重ねて出す
					if (stk.goneFrames > 0) {
						int goneLeft = stk.goneFrames;
						float goneProg = 1.0f - static_cast<float>(goneLeft) / kStakeRespawnFrames;
						float mcx = static_cast<float>(px + kTileSize / 2);
						float mcy = static_cast<float>(py + kTileSize / 2);
						bool isSoon = (goneLeft <= 120);   // 残り2秒を切ったら点滅
						DrawStakeSprite(px, py, kTileSize, 0xC8C8C800 | static_cast<unsigned int>(50 + static_cast<int>(90.0f * goneProg)));
						unsigned int ringA = (isSoon == true && (frameCount / 6) % 2 == 0) ? 255 : 150;
						DrawDashRing(mcx, mcy, kGripRadius + 6.0f, frameCount * 0.02f, 0xFFFFFF00 | ringA);
						int goneSec = (goneLeft + 59) / 60;
						if (goneSec > 99) { goneSec = 99; }
						if (numTex >= 0) {
							int goneDigits = 1;
							if (goneSec >= 10) { goneDigits = 2; }
							int goneW = kNumSpriteW * kNumScale;
							int goneH = kNumSpriteH * kNumScale;
							int goneX0 = RoundPx(mcx) - (goneDigits * goneW + (goneDigits - 1) * kNumGap) / 2;
							int goneY0 = RoundPx(mcy) - goneH / 2;
							unsigned int goneCol = 0xFFFFFFFF;
							if (isSoon == true) {
								goneCol = 0xFFD746FF;
							}
							for (int pass = 0; pass < 2; pass++) {   // 0:黒い影 1:本体
								int off = 0;
								unsigned int c = goneCol;
								if (pass == 0) {
									off = 2;
									c = 0x000000D0u;
								}
								for (int d = 0; d < goneDigits; d++) {
									int digit = goneSec;
									if (goneDigits == 2) {
										if (d == 0) { digit = goneSec / 10; }
										else { digit = goneSec % 10; }
									}
									int dx = goneX0 + d * (goneW + kNumGap) + off;
									int dy = goneY0 + off;
									Novice::DrawQuad(dx, dy, dx + goneW, dy, dx, dy + goneH, dx + goneW, dy + goneH,
										digit * kNumSpriteW, 0, kNumSpriteW, kNumSpriteH, numTex, c);
								}
							}
						}
						else {
							Novice::ScreenPrintf(RoundPx(mcx) - 8, RoundPx(mcy) - 8, "%d", goneSec);
						}
						continue;
					}

					// 杭:絵があれば絵、無ければ仮の茶色い丸
					// 耐久が減るほど赤黒く濃くなってひびが入る。0に近いと点滅。引き伸ばされた直後は震える
					float hpRate = stk.hp / kStakeHpMax;
					if (hpRate < 0.0f) { hpRate = 0.0f; }
					if (hpRate > 1.0f) { hpRate = 1.0f; }
					float dmgRate = 1.0f - hpRate;
					float stakeBright = kStakeDamagedTint + (1.0f - kStakeDamagedTint) * hpRate;
					unsigned int stakeTint = (static_cast<unsigned int>(255.0f * (0.45f + 0.55f * stakeBright)) << 24) | (static_cast<unsigned int>(255.0f * stakeBright) << 16) | (static_cast<unsigned int>(255.0f * stakeBright * 0.9f) << 8) | 0xFF;
					unsigned int fallbackTint = (static_cast<unsigned int>(139.0f * (0.45f + 0.55f * stakeBright)) << 24) | (static_cast<unsigned int>(90.0f * stakeBright) << 16) | (static_cast<unsigned int>(43.0f * stakeBright) << 8) | 0xFF;
					int shakeX = 0;
					if (frameCount < stk.shakeUntil) {
						shakeX = (frameCount % 2 == 0) ? static_cast<int>(kStakeShakePx) : -static_cast<int>(kStakeShakePx);
					}
					float mcx = static_cast<float>(px + kTileSize / 2 + shakeX);
					float mcy = static_cast<float>(py + kTileSize / 2);

					// リポップ直後:大きくなって、少し行き過ぎてから落ち着く
					float popScale = 1.0f;
					if (stk.popFrames > 0) {
						float pt = 1.0f - static_cast<float>(stk.popFrames) / kStakePopFrames;
						popScale = (pt < 0.7f) ? 1.15f * (pt / 0.7f) : 1.15f - 0.15f * ((pt - 0.7f) / 0.3f);
						if (popScale < 0.05f) { popScale = 0.05f; }
						float burstR = kGripRadius + 24.0f * pt;
						Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), static_cast<int>(burstR), static_cast<int>(burstR), 0.0f, 0xFFFFFF00 | static_cast<unsigned int>(200.0f * (1.0f - pt)), kFillModeWireFrame);
					}
					int drawSize = static_cast<int>(kTileSize * popScale);
					int drawX = px + (kTileSize - drawSize) / 2 + shakeX;
					int drawY = py + (kTileSize - drawSize) / 2;
					if (DrawStakeSprite(drawX, drawY, drawSize, stakeTint) == false) {
						int fr = static_cast<int>(kGripRadius * popScale);
						Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), fr, fr, 0.0f, fallbackTint, kFillModeSolid);
					}

					if (hpRate < 1.0f) {
						int gr = static_cast<int>(kGripRadius);
						// 傷んだ分だけ赤黒い色を重ねる
						Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), gr, gr, 0.0f, 0x3C000000 | static_cast<unsigned int>(140.0f * dmgRate), kFillModeSolid);
						// 耐久が3割を切ったら赤く点滅(もうすぐ外れる)
						if (hpRate < 0.3f && (frameCount / 4) % 2 == 0) {
							Novice::DrawEllipse(RoundPx(mcx), RoundPx(mcy), gr, gr, 0.0f, 0xFF281450, kFillModeSolid);
						}
						// ひび割れ:傷むほど本数が増える(最大4本)
						static const float kCrack[4][6] = {
							{ -2.0f, -17.0f,  3.0f,  -8.0f, -4.0f,  -1.0f },
							{ 10.0f,  -6.0f,  4.0f,   0.0f,  9.0f,   9.0f },
							{ -12.0f,  4.0f, -5.0f,   6.0f, -8.0f,  15.0f },
							{ 3.0f,    8.0f, -1.0f,  12.0f,  6.0f,  17.0f },
						};
						int crackN = static_cast<int>(dmgRate * 4.99f);
						for (int c = 0; c < crackN; c++) {
							int cx0 = RoundPx(mcx + kCrack[c][0]);
							int cy0 = RoundPx(mcy + kCrack[c][1]);
							int cx1 = RoundPx(mcx + kCrack[c][2]);
							int cy1 = RoundPx(mcy + kCrack[c][3]);
							int cx2 = RoundPx(mcx + kCrack[c][4]);
							int cy2 = RoundPx(mcy + kCrack[c][5]);
							for (int th = 0; th < 2; th++) {
								Novice::DrawLine(cx0 + th, cy0, cx1 + th, cy1, 0x200800FFu);
								Novice::DrawLine(cx1 + th, cy1, cx2 + th, cy2, 0x200800FFu);
							}
						}
						// 耐久バー
						int barW = kTileSize - 8;
						unsigned int barCol = (hpRate > 0.6f) ? 0x60E060FFu : ((hpRate > 0.3f) ? 0xF0C030FFu : 0xE04040FFu);
						Novice::DrawBox(px + 3, py + 1, barW + 2, 7, 0.0f, 0x000000C0u, kFillModeSolid);
						Novice::DrawBox(px + 4, py + 2, static_cast<int>(barW * hpRate), 5, 0.0f, barCol, kFillModeSolid);
					}
				}

				// 引っ張ってる触手だけ明るくする(引っ張ってない触手は普通の色のまま)
				bool isPulling[2];
				bool isPeelReady[2];   // この触手を引っ張ればもう片方の杭を引きはがせる
				unsigned int drawCol[2];
				unsigned int glowCol[2];
				for (int h = 0; h < 2; h++) {
					isPulling[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase == kGripPull);
					isPeelReady[h] = false;
				}

				for (int h = 0; h < 2; h++) {
					int holdH = 1 - h;   // 引きはがされる側の触手
					if (sets[holdH].isActive == false || sets[holdH].state != kSetOut || sets[holdH].gripPhase != kGripHold) {
						continue;
					}
					if (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind) {
						float gapX = sets[holdH].gripPos.x - sets[h].gripPos.x;
						float gapY = sets[holdH].gripPos.y - sets[h].gripPos.y;
						float gapLen = sqrtf(gapX * gapX + gapY * gapY);
						//2つの杭が kPeelNeedLen 以上離れてて、掛かってるロープの長さより遠い(引くとロープが限界まで伸びる)
						if (gapLen >= kPeelNeedLen && gapLen > sets[holdH].ropeLen) {
							isPeelReady[h] = true;
						}
					}
				}
				float peelWave = 0.5f + 0.5f * sinf(frameCount * kPeelReadyPulse);
				for (int h = 0; h < 2; h++) {
					float cr = static_cast<float>((kTentColor[h] >> 24) & 0xFF);
					float cg = static_cast<float>((kTentColor[h] >> 16) & 0xFF);
					float cb = static_cast<float>((kTentColor[h] >> 8) & 0xFF);
					unsigned int ca = 0xFF;
					if (isPulling[h] == true) {
						cr += (255.0f - cr) * kPullBright;   // 白に近づけて明るく
						cg += (255.0f - cg) * kPullBright;
						cb += (255.0f - cb) * kPullBright;
					}
					drawCol[h] = (static_cast<unsigned int>(cr) << 24) | (static_cast<unsigned int>(cg) << 16) | (static_cast<unsigned int>(cb) << 8) | ca;
					glowCol[h] = (drawCol[h] & 0xFFFFFF00u);
				}
				float glowWave = 0.5f + 0.5f * sinf(frameCount * kPullGlowSpeed);
				unsigned int glowAlpha = static_cast<unsigned int>(kPullGlowAlphaMin + (kPullGlowAlphaMax - kPullGlowAlphaMin) * glowWave);

				// ゲート(触手の色で出す)
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false || ts.gateOpen <= 0.0f) {
						continue;
					}
					float cx = ts.gatePos.x;
					float cy = ts.gatePos.y;

					// 杭に口を向ける細長いゲート
					float nx = ts.dir.x;
					float ny = ts.dir.y;
					float bx = -ny;
					float by = nx;
					float hw = kGateHalfW * (0.3f + 0.7f * ts.gateOpen);
					float hh = kGateHalfLen * ts.gateOpen;
					Novice::DrawQuad(
						static_cast<int>(cx - nx * hw - bx * hh), static_cast<int>(cy - ny * hw - by * hh),
						static_cast<int>(cx + nx * hw - bx * hh), static_cast<int>(cy + ny * hw - by * hh),
						static_cast<int>(cx - nx * hw + bx * hh), static_cast<int>(cy - ny * hw + by * hh),
						static_cast<int>(cx + nx * hw + bx * hh), static_cast<int>(cy + ny * hw + by * hh),
						0, 0, kGateSpriteW, kGateSpriteH, gateTex, drawCol[s]);
				}

				// 曲線を kTentacleStep px ごとに区切って、区切りごとに絵を貼る(太さは絵の真ん中を削って変える)
				// 区切りの点ごとに向き(法線)と太さを1回だけ計算。隣り合う区切りで点と幅を共有するので境目に隙間ができない
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false) {
						continue;
					}
					// 0回目:普通に描く 1回目:引っ張ってる触手だけ加算で重ねて光らせる
					int passCount = (isPulling[s] == true || isPeelReady[s] == true) ? 2 : 1;
					for (int pass = 0; pass < passCount; pass++) {
						unsigned int tentCol = drawCol[s];
						if (pass == 1) {
							Novice::SetBlendMode(kBlendModeAdd);
							tentCol = glowCol[s] | glowAlpha;
						}
						Tentacle& tn = ts.t;
						if (tn.len >= 2.0f) {
							// 短い間は細くする
							float widthRate = tn.len / 80.0f;
							if (widthRate > 1.0f) {
								widthRate = 1.0f;
							}

							// 曲線を根元から先端へ kTentacleStep px ごとに区切る
							float sampleX[kTentacleMaxSample];
							float sampleY[kTentacleMaxSample];
							int sampleCount = 1;
							sampleX[0] = pts[s][0].x;
							sampleY[0] = pts[s][0].y;
							float pathLen = 0.0f;
							float need = kTentacleStep;
							for (int i = 0; i < kTentacleSeg; i++) {
								float ax = pts[s][i].x;
								float ay = pts[s][i].y;
								float bx = pts[s][i + 1].x;
								float by = pts[s][i + 1].y;
								float segLen = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
								pathLen += segLen;
								float used = 0.0f;
								while (segLen - used >= need && sampleCount < kTentacleMaxSample - 2) {
									used += need;
									sampleX[sampleCount] = ax + (bx - ax) * (used / segLen);
									sampleY[sampleCount] = ay + (by - ay) * (used / segLen);
									sampleCount++;
									need = kTentacleStep;
								}
								need -= segLen - used;
							}

							// 最後の区切りから先端までの端数
							int lastRows = static_cast<int>((kTentacleStep - need) / kPixelScale + 0.5f);
							if (lastRows > kTentacleStepRows) {
								lastRows = kTentacleStepRows;
							}
							int stepCount = sampleCount - 1;
							if (lastRows >= 1) {
								sampleX[sampleCount] = pts[s][kTentacleSeg].x;
								sampleY[sampleCount] = pts[s][kTentacleSeg].y;
								sampleCount++;
								stepCount++;
							}

							// 区切りの点ごとの法線(曲線に直角)と太さ
							float perpX[kTentacleMaxSample];
							float perpY[kTentacleMaxSample];
							float halfSrcF[kTentacleMaxSample];   // 太さ(ドット数・片側)
							for (int n = 0; n < sampleCount; n++) {
								int prev = (n > 0) ? n - 1 : 0;
								int next = (n + 1 < sampleCount) ? n + 1 : sampleCount - 1;
								float vx = sampleX[next] - sampleX[prev];
								float vy = sampleY[next] - sampleY[prev];
								float vl = sqrtf(vx * vx + vy * vy);
								if (vl < 0.001f) {
									vx = 1.0f;
									vy = 0.0f;
									vl = 1.0f;
								}
								perpX[n] = -vy / vl;
								perpY[n] = vx / vl;

								// 今の長さに対して根元から先端へ細くなる(伸びてる途中も先端は鋭い)
								float t = (pathLen > 1.0f) ? (n * kTentacleStep) / pathLen : 1.0f;
								if (t > 1.0f) {
									t = 1.0f;
								}
								float h = kTentacleBaseHalfSrc * (1.0f - t) + kTentacleTipHalfSrc * t;
								h *= widthRate;
								if (h < 1.0f) {
									h = 1.0f;
								}
								halfSrcF[n] = h;
							}

							// 区切りごとに絵を左右2枚の四角形で貼る(上端は点n、下端は点n+1の値)
							for (int n = 0; n < stepCount; n++) {
								float wTop = halfSrcF[n] * kPixelScale;
								float wBot = halfSrcF[n + 1] * kPixelScale;
								int halfSrc = static_cast<int>((halfSrcF[n] + halfSrcF[n + 1]) * 0.5f + 0.5f);
								if (halfSrc < 1) {
									halfSrc = 1;
								}
								int srcHalfW = halfSrc * kPixelScale;

								int srcY = (n * kTentacleStepRows * kPixelScale) % kTentacleSpriteH;
								int rows = kTentacleStepRows;
								if (n == stepCount - 1 && lastRows >= 1) {
									rows = lastRows;
								}
								int srcRows = rows * kPixelScale;

								float cx0 = sampleX[n];
								float cy0 = sampleY[n];
								float cx1 = sampleX[n + 1];
								float cy1 = sampleY[n + 1];

								// 左半分
								Novice::DrawQuad(
									RoundPx(cx0 - perpX[n] * wTop), RoundPx(cy0 - perpY[n] * wTop),
									RoundPx(cx0), RoundPx(cy0),
									RoundPx(cx1 - perpX[n + 1] * wBot), RoundPx(cy1 - perpY[n + 1] * wBot),
									RoundPx(cx1), RoundPx(cy1),
									0, srcY, srcHalfW, srcRows, tentacleTex, tentCol);

								// 右半分
								Novice::DrawQuad(
									RoundPx(cx0), RoundPx(cy0),
									RoundPx(cx0 + perpX[n] * wTop), RoundPx(cy0 + perpY[n] * wTop),
									RoundPx(cx1), RoundPx(cy1),
									RoundPx(cx1 + perpX[n + 1] * wBot), RoundPx(cy1 + perpY[n + 1] * wBot),
									kTentacleSpriteW - srcHalfW, srcY, srcHalfW, srcRows, tentacleTex, tentCol);
							}
						}
						if (pass == 1) {
							Novice::SetBlendMode(kBlendModeNormal);
						}
					}
				}

				// プレイヤー画像だけ描画倍率を1.0倍に固定する。
				// 1コマ64x64pxの画像は、ゲーム画面でも64x64pxで表示する。
				// kPlayerW/Hは物理・当たり判定用のため変更しない。
				{
					const float kPlayerDrawScale = AssetEditor::Scale(
						AssetEditor::PlayerSlotForHandle(playerTex)); // ImGuiの表示倍率（物理には影響しない）
					const AssetEditor::FrameSource frameSource = AssetEditor::GetFrame(
						AssetEditor::PlayerSlotForHandle(playerTex), animationFrame);
					if (playerTex >= 0) {
						const float drawW = static_cast<float>(frameSource.w) * kPlayerDrawScale;
						const float drawH = static_cast<float>(frameSource.h) * kPlayerDrawScale;
						const float sideX = (player.isFacingRight ? 1.0f : -1.0f) * drawW * 0.5f;
						const float cx = player.pos.x;
						const float drawBottomY = player.pos.y + kPlayerHalfH;
						const float drawTopY = drawBottomY - drawH;
						Novice::DrawQuad(
							static_cast<int>(cx - sideX), static_cast<int>(drawTopY),
							static_cast<int>(cx + sideX), static_cast<int>(drawTopY),
							static_cast<int>(cx - sideX), static_cast<int>(drawBottomY),
							static_cast<int>(cx + sideX), static_cast<int>(drawBottomY),
							frameSource.x, frameSource.y, frameSource.w, frameSource.h,
							playerTex, 0xFFFFFFFF);
					}
				}

				// L/R表示:プレイヤーの左下にL、右下にR(触手の色で色分け)。
				// 杭に着いてる間はそのL/Rの周りを半円矢印2本が回る
				for (int s = 0; s < 2; s++) {
					float sideSign = (s == 0) ? -1.0f : 1.0f;
					float lrX = player.pos.x + sideSign * kLRUiOffsetX;
					float lrY = player.pos.y + kLRUiOffsetY;
					if (lrRingFade[s] > 0.0f) {
						float ringScale = 0.6f + 0.4f * lrRingFade[s];   // 出る時に大きくなる
						unsigned int ringAlpha = static_cast<unsigned int>(255.0f * lrRingFade[s]);
						unsigned int ringCol = (drawCol[s] & 0xFFFFFF00u) | ringAlpha;
						DrawCycleRing(lrX, lrY, kLRRingSize * ringScale, lrRingAng[s], ringCol);
					}
					DrawLRBadge(s, lrX, lrY, kLRUiSize, drawCol[s]);
				}

				// 入力方向の矢印:プレイヤーの周りに入力方向を指して出る(狙ってる杭があれば黄色)
				if (aimArrowAlpha > 0.0f) {
					float ax = player.pos.x + cosf(aimArrowAng) * kAimArrowRadius;
					float ay = player.pos.y + sinf(aimArrowAng) * kAimArrowRadius;
					float half = kAimArrowSize * 0.5f;
					float c = cosf(aimArrowAng);
					float sn = sinf(aimArrowAng);
					unsigned int arrowAlpha = static_cast<unsigned int>(255.0f * aimArrowAlpha);
					unsigned int arrowColor = ((isGripLock == true) ? 0xFFE040FFu : 0xFFFFFFFFu) & 0xFFFFFF00u;
					arrowColor |= arrowAlpha;
					if (aimArrowTex >= 0) {
						Novice::DrawQuad(
							RoundPx(ax + (-half) * c - (-half) * sn), RoundPx(ay + (-half) * sn + (-half) * c),
							RoundPx(ax + half * c - (-half) * sn), RoundPx(ay + half * sn + (-half) * c),
							RoundPx(ax + (-half) * c - half * sn), RoundPx(ay + (-half) * sn + half * c),
							RoundPx(ax + half * c - half * sn), RoundPx(ay + half * sn + half * c),
							0, 0, kAimArrowSpriteSize, kAimArrowSpriteSize, aimArrowTex, arrowColor);
					}
					else {
						Novice::DrawEllipse(RoundPx(ax), RoundPx(ay), static_cast<int>(half * 0.5f), static_cast<int>(half * 0.5f), 0.0f, arrowColor, kFillModeSolid);
					}
				}

				// 触手が掴んでる杭に、その触手の色の輪とL/Rを出す。引っ張り中は輪が脈打つ
				for (int s = 0; s < 2; s++) {
					TentacleSet& ts = sets[s];
					if (ts.isActive == false || ts.state != kSetOut || ts.gripPhase == kGripUnwind) {
						continue;
					}
					float ringR = kGripRadius + 10.0f;
					if (ts.gripPhase == kGripPull) {
						ringR += 4.0f * sinf(frameCount * 0.5f);
					}
					bool isReadyRing = (isPeelReady[s] == true);
					if (isReadyRing == true) {
						ringR += 3.0f + 5.0f * peelWave;   // 巻き取れる:輪が膨らんで脈打つ
					}
					int ringX = RoundPx(ts.gripPos.x);
					int ringY = RoundPx(ts.gripPos.y);
					Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR), static_cast<int>(ringR), 0.0f, drawCol[s], kFillModeWireFrame);
					Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 1, static_cast<int>(ringR) + 1, 0.0f, drawCol[s], kFillModeWireFrame);
					if (ts.gripPhase == kGripPull) {
						// 引っ張ってる杭は輪を太く(外側にもう2本)
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 2, static_cast<int>(ringR) + 2, 0.0f, drawCol[s], kFillModeWireFrame);
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 3, static_cast<int>(ringR) + 3, 0.0f, drawCol[s], kFillModeWireFrame);
					}
					if (isReadyRing == true) {
						// 輪を太く(外側にもう2本)
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 2, static_cast<int>(ringR) + 2, 0.0f, drawCol[s], kFillModeWireFrame);
						Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 3, static_cast<int>(ringR) + 3, 0.0f, drawCol[s], kFillModeWireFrame);
					}
					DrawLRBadge(s, static_cast<float>(ringX), ringY - ringR - kLRMarkSize * 0.5f - 2.0f, kLRMarkSize, drawCol[s]);

				}

				// 自動照準マーク:届く範囲の杭に薄いマーク、狙ってる杭には回る大きいマーク(次に撃つ触手の色。狙ってる杭にはL/R)
				for (int i = 0; i < gripCandCount; i++) {
					bool isTarget = (isGripLock == true && gripCand[i] == gripLockIdx);
					float mx = stakes[gripCand[i]].pos.x;
					float my = stakes[gripCand[i]].pos.y;
					float half = (isTarget ? kGripMarkSize : kGripMarkSizeSub) * 0.5f;
					float ang = 0.0f;
					unsigned int markBase = 0xFFFFFFFFu;
					if (gripMarkSet >= 0) {
						markBase = kTentColor[gripMarkSet];
					}
					unsigned int markColor = (markBase & 0xFFFFFF00u) | 0x70u;
					if (isTarget == true) {
						half *= 1.0f + 0.08f * sinf(frameCount * 0.25f);   // 脈打つ
						ang = frameCount * 0.04f;                           // ゆっくり回る
						markColor = markBase;
						unsigned int badgeColor = (gripMarkSet >= 0) ? kTentColor[gripMarkSet] : 0xFFFFFFFFu;
						if (gripMarkSet >= 0) {
							DrawLRBadge(gripMarkSet, mx, my + half + kLRMarkSize * 0.5f + 2.0f, kLRMarkSize, badgeColor);
						}
					}
					if (gripMarkTex >= 0) {
						float c = cosf(ang);
						float sn = sinf(ang);
						Novice::DrawQuad(
							RoundPx(mx + (-half) * c - (-half) * sn), RoundPx(my + (-half) * sn + (-half) * c),
							RoundPx(mx + half * c - (-half) * sn), RoundPx(my + half * sn + (-half) * c),
							RoundPx(mx + (-half) * c - half * sn), RoundPx(my + (-half) * sn + half * c),
							RoundPx(mx + half * c - half * sn), RoundPx(my + half * sn + half * c),
							0, 0, kGripMarkSpriteSize, kGripMarkSpriteSize, gripMarkTex, markColor);
					}
					else {
						// 絵が無い時の仮のマーク(丸枠)
						Novice::DrawEllipse(RoundPx(mx), RoundPx(my), static_cast<int>(half), static_cast<int>(half), 0.0f, markColor, kFillModeWireFrame);
					}
				}

				// 抜けて飛ぶ杭(だんだん薄く)
				for (int i = 0; i < kStakePeelMax; i++) {
					if (stakePeels[i].on == false) {
						continue;
					}
					float stakeFade = 1.0f - static_cast<float>(stakePeels[i].t) / kPeelPieceFrames;
					unsigned int stakeColor = 0xFFFFFF00u | static_cast<unsigned int>(255.0f * stakeFade);
					int spx = RoundPx(stakePeels[i].x);
					int spy = RoundPx(stakePeels[i].y);
					if (DrawStakeSprite(spx - kTileSize / 2, spy - kTileSize / 2, kTileSize, stakeColor) == false) {
						unsigned int fallbackColor = 0x8B5A2B00u | static_cast<unsigned int>(255.0f * stakeFade);
						Novice::DrawEllipse(spx, spy, static_cast<int>(kGripRadius), static_cast<int>(kGripRadius), 0.0f, fallbackColor, kFillModeSolid);
					}
				}

				// 杭を引き抜いた時の輪
				for (int i = 0; i < kMaxPopup; i++) {
					if (popups[i].on == false) {
						continue;
					}
					float sx = popups[i].x;
					float sy = popups[i].y;
					if (popups[i].kind == 0) {
						// 成功:杭のあった場所から輪が広がる
						float burstR = 14.0f + popups[i].t * 3.5f;
						unsigned int burstA = static_cast<unsigned int>(220.0f * (1.0f - static_cast<float>(popups[i].t) / kPopupFrames));
						Novice::DrawEllipse(RoundPx(sx), RoundPx(sy), static_cast<int>(burstR), static_cast<int>(burstR), 0.0f, 0x78FFA000 | burstA, kFillModeWireFrame);
						Novice::DrawEllipse(RoundPx(sx), RoundPx(sy), static_cast<int>(burstR) + 1, static_cast<int>(burstR) + 1, 0.0f, 0x78FFA000 | burstA, kFillModeWireFrame);
					}
				}

				// 触手の状態(左下):L/Rのバッジ + 状態の文字(ScreenPrintf)
				for (int h = 0; h < 2; h++) {
					float hudY = static_cast<float>(kWindowHeight - 58 + h * 30);
					DrawLRBadge(h, 22.0f, hudY + 12.0f, 24.0f, kTentColor[h]);
					int stateFrame = 0;   // READY
					if (sets[h].isActive == true) {
						if (sets[h].gripPhase == kGripReach) {
							stateFrame = 1;   // SHOOT
						}
						else if (sets[h].gripPhase == kGripHold) {
							stateFrame = 2;   // HOOKED
						}
						else if (sets[h].gripPhase == kGripPull) {
							stateFrame = 3;   // PULL
						}
						else {
							stateFrame = 4;   // RETURN
						}
					}
					static const char* stateText[5] = { "READY", "SHOOT", "HOOKED", "PULL", "RETURN" };
					Novice::ScreenPrintf(44, RoundPx(hudY) + 4, "%s", stateText[stateFrame]);
				}




				///
				/// ↑描画処理ここまで
			}
			///



		}

		snapshot.position = { player.pos.x,720.0f - player.pos.y };
		snapshot.velocity = { player.Velocity.x,-player.Velocity.y };
		snapshot.gripping = false;
		snapshot.pulling = false;
		snapshot.gripIndex = -1;
		for (int h = 0; h < 2; ++h) {
			snapshot.handGripIndex[h] =
				(sets[h].isActive && sets[h].state == kSetOut &&
					sets[h].gripPhase != kGripUnwind) ? sets[h].gripIdx : -1;
			snapshot.handPulling[h] = snapshot.handGripIndex[h] >= 0 &&
				sets[h].gripPhase == kGripPull;
		}
		if (update) snapshot.peeledStake = stakeRemovedThisFrame;
		for (int s = 0;s < 2;++s) {
			if (sets[s].isActive && sets[s].state == kSetOut &&
				sets[s].gripPhase != kGripUnwind) {
				snapshot.gripping = true;
				snapshot.gripIndex = sets[s].gripIdx;
				if (sets[s].gripPhase == kGripPull) snapshot.pulling = true;
			}
		}
	}


}


namespace ExactPlayerBattle {
	static bool alternate = false;
	static bool pendingReset = true;
	static Snapshot current = {};
	static int bossAnchorStart = 24;
	void RequestReleaseBossGrips() { releaseBossGripsRequested = true; }
	void SetBossAnchorStart(int index) { bossAnchorStart = index; }
	void SetMovingAnchorCount(int count) { movingAnchorCount = count > 0 ? count : 0; }
	int BossAnchorStart() { return bossAnchorStart; }
	void Reset() { pendingReset = true; current = {}; }
	void SetAlternate(bool value) { if (alternate != value) { alternate = value; pendingReset = true; } }
	bool IsAlternate() { return alternate; }
	void Update(const ::Vector2* anchors, int count) {
		if (alternate) ExactSampleAlternateBattle::Frame(true, false, pendingReset, anchors, count, current);
		else ExactSampleNonAlternateBattle::Frame(true, false, pendingReset, anchors, count, current);
		releaseBossGripsRequested = false;
		pendingReset = false;
	}
	void Draw() {
		if (alternate) ExactSampleAlternateBattle::Frame(false, true, false, nullptr, 0, current);
		else ExactSampleNonAlternateBattle::Frame(false, true, false, nullptr, 0, current);
	}
	Snapshot GetSnapshot() { return current; }
}
