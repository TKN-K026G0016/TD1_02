#pragma once
#include "CloudStageSettings.h"
#include "GripPathMath.h"
// 雲ステージ専用の状態。更新と描画で同じメンバー変数を使う。
namespace CloudRuntime {
	using namespace CloudStageSettings;
	struct Vector2 {
		float x;
		float y;
	};

	struct Player {
		Vector2 pos;
		Vector2 Velocity;
		bool isFacingRight;
	};

	struct Tentacle {
		float len;            // いまの長さ
		float phase;          // ゆらゆらの位相
		float retractFrom;    // 戻り始めた時の長さ
	};

	struct TentacleSet {
		bool isActive;
		int state;            // kSetOut / kSetRetract
		int timer;
		Vector2 gatePos;
		Vector2 gateOffset;   // プレイヤーの中心から見た、ゲートのいたい場所
		Vector2 dir;          // ひっかけ先の向き(長さ1)
		float gateOpen;       // ゲートの開き具合(0〜1)
		int gripPhase;        // kGripReach / kGripHold / kGripPull / kGripUnwind
		int gripTimer;        // 引っ張りが始まってからのフレーム数
		Vector2 gripPos;      // ひっかけ先の中心
		int gripTarget;       // ひっかけ先(-1:雲 / 0以上:杭の番号 / -2:もう追いかけない)
		float gripSpin;       // 巻く向き(+1 / -1)
		float gripTense;      // 張り具合(0〜1)。張っている間は触手がゆれない
		Vector2 pullDir;      // 引っ張っている向き(離した時に、この向きへ飛ぶ)
		float ropeLen;        // ロープの長さ(これより遠くへは離れられない。引っ張ると縮む)
		float pullStartDist;  // 巻き始めた時の、ひっかけ先までの距離(巻いた割合の計算用)
		float pullProg;       // 巻いた割合(0〜1)。増えた分だけ杭の耐久度が減る
		Tentacle t;           // ゲートから出る触手
	};

	struct Sprite {
		int handle;
		float w, h;
	};

	struct Drop {
		float x, y;
		bool on;
	};

	struct Bullet {
		float x, y, vx, vy;
		bool on;
	};

	struct Frog {
		float x, y, rx, ry;
	};

	struct CloudPiece {
		float x, y, vx, vy, s;
	};

	struct CloudPart {
		bool alive;
	};

	struct PeelPiece {
		float x, y, vx, vy, s;
		int t;
		bool on;
	};
	using GripPath = GripPathMath::Path<Vector2>;
	int CloudTexture(const char* path);
	int CloudOptionalTexture(const char* path);
	void BuildGripPath(const Vector2& gate, const Vector2& center, float spin, GripPath& gp);
	Vector2 GripPathPoint(const GripPath& gp, float distance);
	float Lerp(float a, float b, float t);
	float Clamp01(float t);
	float EaseIn(float t);
	float EaseOut(float t);
	float EaseInOut(float t);
	int ClampAlpha(float a);
	Sprite LoadSprite(const char* path, float w, float h);
	void DrawSpriteC(const Sprite& s, float cx, float cy, float w, float h, float a);
	void DrawEyeC(const Sprite& s, float cx, float cy, float d, float a);
	void Fire(Bullet* b, int count, float x, float y, float angle, float speed);

	struct CloudBoss {
		CloudBoss();
		void Update();
		void Draw();
	private:

		// 画像の読み込み(Resourcesフォルダに入れる。サイズは元画像のピクセル数)
		Sprite sprBg = LoadSprite("./resources/cloud/bg.png", 1280.0f, 720.0f);
		Sprite sprCloud = LoadSprite("./resources/cloud/cloud.png", 240.0f, 120.0f);
		Sprite sprCloudDark = LoadSprite("./resources/cloud/cloud_dark.png", 240.0f, 120.0f);
		Sprite sprFrog = LoadSprite("./resources/cloud/frog.png", 240.0f, 204.0f);
		Sprite sprSun = LoadSprite("./resources/cloud/sun.png", 256.0f, 256.0f);
		Sprite sprOrbRed = LoadSprite("./resources/cloud/orb_red.png", 32.0f, 32.0f);
		Sprite sprOrbYellow = LoadSprite("./resources/cloud/orb_yellow.png", 32.0f, 32.0f);
		Sprite sprDrop = LoadSprite("./resources/cloud/drop.png", 6.0f, 24.0f);
		Sprite sprWater = LoadSprite("./resources/cloud/water.png", 64.0f, 64.0f);
		Sprite sprWaterShockA = LoadSprite("./resources/cloud/water_shock_a.png", 64.0f, 64.0f);
		Sprite sprWaterShockB = LoadSprite("./resources/cloud/water_shock_b.png", 64.0f, 64.0f);
		Sprite sprDangerLine = LoadSprite("./resources/cloud/danger_line.png", 8.0f, 64.0f);
		Sprite sprDangerBox = LoadSprite("./resources/cloud/danger_box.png", 140.0f, 140.0f);

		// 新しい絵(コマが並んだ絵なので、Sprite ではなくハンドルだけ持って、描く時にコマを切り出す)
		int frogShotTex = CloudTexture(kFrogShotPath);    // 吐き出されて飛んでいくカエル
		int boltTex = CloudTexture(kBoltPath);            // 雷
		int waterTopTex = CloudTexture(kWaterTopPath);    // 水の上の波
		int waterTopShockTex = CloudTexture(kWaterTopShockPath);   // 感電中の水の上の波(黄色)

		// 雲の目玉(ファイルが無ければ -1 のまま。仮の丸で描く)
		Sprite sprEye = { CloudOptionalTexture(kEyePath), static_cast<float>(kEyeSrcSize), static_cast<float>(kEyeSrcSize) };

		// キー入力結果を受け取る箱
		char keys[256] = { 0 };
		char preKeys[256] = { 0 };

		int frame = 0;

		// ダミーのプレイヤー(矢印キーで動かせる。突撃の危険マークや雷がここを狙う)
		float px = 640.0f;
		float py = 580.0f;

		// ボス(雲)
		float bossX = kCenterX - kRadiusX;
		float bossY = kStartY;
		float bossScale = 1.0f;
		float bossAlpha = 255.0f;

		// ボスの動き用
		float angle = kPi;       // 回転の角度
		float waveAngle = 0.0f;  // 波の角度
		float centerY = kStartY; // 回る中心の高さ(これが上に登っていく)

		// いまの攻撃(0=なし、1〜5)と、その経過フレーム
		int attack = 0;
		int timer = 0;

		// 雲の「にゅっ」と伸びるアニメーション(ものを出すとき、一瞬伸びる)
		int stretchT = 999;      // 伸び始めてからのフレーム(大きい値=伸びていない)
		float stretchDir = 0.0f; // 伸びる向き(+1=下へ出す、-1=上へ出す)

		// 1:雷
		float thunderX[3] = {};
		// 2:突撃の狙い位置と、突撃を始めたときの雲の位置
		float tx = 640.0f;
		float ty = 580.0f;
		float fromX = 640.0f;
		float fromY = 100.0f;
		// 3:雨と水と雷
		Drop drops[200] = {};
		float waterH = 0.0f;
		float lightX = 640.0f; // 雷を落とすx
		bool shock = false;    // 水が感電中か
		// 4:カエル
		Frog frogs[kFrogNum] = {};    // 画面に張り付くカエル(攻撃が終わっても張り付いたまま残る)
		bool frogsOn = false;  // 張り付きカエルがいるか
		int frogT = 0;         // 張り付きカエルの経過フレーム(跳ぶ→張り付く→剥がれる)
		bool flying[kFrogNum] = {};   // 吐き出されて飛んでいるカエル
		float fx[kFrogNum] = {};
		float fy[kFrogNum] = {};
		float fvx[kFrogNum] = {};
		// 5:太陽
		Bullet bullets[100] = {};
		float sunR = 0.0f;
		bool sunBroken = false;   // 太陽が壊された(耐久が0・引きはがし)。縮んで消えて、攻撃が終わる
		const float kSunX = 640.0f;
		const float kSunY = 240.0f;

		// 主人公(触手攻撃・ひっかけ)
		int playerIdleTex = CloudTexture("./resources/cloud/player_idle.png");
		int playerWalkTex = CloudTexture("./resources/cloud/player_walk.png");
		int playerJumpTex = CloudTexture("./resources/cloud/player_jump.png");
		int playerFallTex = CloudTexture("./resources/cloud/player_fall.png");
		int tentacleTex = CloudTexture("./resources/cloud/tentacle.png");
		int gateTex = CloudTexture("./resources/cloud/gate.png");

		// 自動照準のマーク(ファイルが無ければ -1 のまま。仮の丸で描く)
		int gripMarkTex = CloudOptionalTexture("./resources/cloud/grip_mark.png");

		// 入力方向の矢印(ファイルが無ければ -1 のまま。仮の三角で描く)
		int aimArrowTex = CloudOptionalTexture("./resources/cloud/aim_arrow.png");

		// L/R の表示(ファイルが無ければ -1 のまま。仮の丸と文字で描く)
		int lrTex = CloudOptionalTexture("./resources/cloud/ui_lr.png");

		// L/R のまわりを回る、半円形の矢印2本(ファイルが無ければ -1 のまま。仮の三角で描く)
		int cycleTex = CloudOptionalTexture("./resources/cloud/ui_cycle.png");

		Player player = { { 640.0f, 790.0f }, { 0.0f, 0.0f }, true };
		bool playerOn = false;                // 雲が上に着いたら、下から出てくる
		bool windOn = false;                  // 風で吹き上げられている最中か
		float windFade = 0.0f;                // 風の演出の濃さ(0〜1)。風が吹くとすぐ濃くなり、止むとゆっくり薄れる
		bool jumpReady = true;                // 空中ジャンプが1回使えるか(ひっかけ・風で戻る)
		int animationFrame = 0;
		int animationTimer = 0;
		int playerTex = playerIdleTex;

		TentacleSet sets[2]{};                  // 0=左の触手(L2) 1=右の触手(R2)
		float knockX = 0.0f;                  // 触手を離した時などの、横への反動
		int knockTimer = 0;                   // 反動が減り始めるまでの残りフレーム
		Vector2 reelPos[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };   // 各触手が最後に巻いた先(杭・雲)の位置(引きはがしの距離判定用)
		int reelGrace[2] = { 0, 0 };          // 各触手が巻いた直後の残りフレーム(もう片方のロープが引っかからず、外れる猶予)
		bool preTentHold[2] = { false, false };   // 1フレーム前の、触手L/Rのボタン
		bool preJumpHold = false;
		bool preReleaseHold = false;
		bool isPadDebug = false;              // TABキーで、押したコントローラーのボタン番号を表示

		CloudPart parts[kCloudPartNum] = {};  // 雲のかたまり

		PeelPiece peels[kCloudPartNum] = {};  // 剥がれて飛んでいくかたまり
		PeelPiece stakePeels[kStakePeelMax] = {};   // 抜けて飛んでいく杭
		int stakePeelNext = 0;                // 次に使う、抜けた杭の番号
		int gripMarkSet = -1;                 // 狙いのマークを出している触手(0:L / 1:R / -1:なし)

		int gripCandIdx[kMaxGripCand] = {};        // 自動照準の候補(-1:雲 / 0以上:杭の番号)
		int gripCandCount = 0;
		bool isGripLock = false;              // いま狙っているものがあるか
		int gripLockIdx = 0;                  // 狙っているもの(-1:雲 / 0以上:杭の番号)

		float aimArrowAng = 0.0f;
		float aimArrowAlpha = 0.0f;

		// L/R の表示:触手がひっかけポイントに着いている間、まわりを回る矢印
		float lrRingAng[2] = { 0.0f, 0.0f };  // 矢印の回転角
		float lrRingFade[2] = { 0.0f, 0.0f }; // 矢印の出ている度合い(0〜1)

		// 触手の形(パーツの座標)。更新で計算して、描画で使う
		Vector2 pts[2][kTentacleSeg + 1]{};

		// 杭(上から落ちてきて、下へスクロールする)
		// (最後の番号 kSunSlot は太陽。ループは kStakeMax までなので、太陽は杭の動き・描画には巻き込まれない)
		float stakeX[kStakeMax + 1] = {};
		float stakeY[kStakeMax + 1] = {};
		float stakeHp[kStakeMax + 1] = {};        // 杭の耐久度。引き伸ばされると減って、0になると抜ける
		int stakeShakeUntil[kStakeMax + 1] = {};  // 杭が震えるのは、frame がこの値に届くまで
		bool stakeOn[kStakeMax + 1] = {};
		int stakeTimer = 0;

		//雲の体力と、攻撃のタイミング
		int bossHp = kBossHpMax;
		int bossHitT = 0;                     // 雲がダメージで揺れている残りフレーム
		float bossChip = 0.0f;                // 巻いて削れた体力(かたまり何個分か)。体力バー = bossHp - bossChip。kCloudShedAt を超えるたび、かたまりが1つ自動で外れる(外れた分は引いて、体力バーは連続のまま)
		bool isClear = false;
		int clearT = 0;                       // 倒してからの経過フレーム
		float clearX = 0.0f;                  // 倒した時の雲の位置と大きさ(ここで止まる)
		float clearY = 0.0f;
		float clearScale = 1.0f;
		CloudPiece pieces[kScatterNum] = {};  // 四散する雲のかけら
		float eyeX = 0.0f;                    // 逃げていく目玉
		float eyeY = 0.0f;
		float eyeVy = 0.0f;                   // 目玉が登る速さ
		float eyeBaseY = 0.0f;                // 目玉のぐるぐるの中心の高さ(これが上へ登っていく)
		float eyeAngle = kPi;                 // 目玉のぐるぐるの角度
		float eyeScale = 1.0f;                // 目玉の奥行きによる大きさ(1.0倍が基準)
		int eyeT = 0;                         // 目玉が登り始めてからのフレーム
		int flyStake = -1;                    // プレイヤーを引っ張り上げる杭の番号(-1:まだ出していない)
		int eyeGoneT = 0;                     // 目玉が画面の上へ出ていってからのフレーム
		int endT = 0;                         // 目玉が画面の外へ出てからのフレーム(フェードアウト)
		int restTimer = kRestFrames;          // 次の攻撃までの休憩
		int lastAttack = 0;                   // 直前の攻撃(同じ攻撃が続かないようにする)
		bool attackRunning = false;           // 攻撃が始まって、まだ終わっていないか


		float padX = 0, padY = 0, stretchSX = 1, stretchSY = 1;
		bool showCloudEditor = false;
	};
}
