#pragma once
#include <Novice.h>
// ステージ3の調整値。雲ステージが使う画面座標系は変えない。
namespace CloudStageSettings {

	inline const float kPi = 3.14159265f;

	// ボスの登場からふわふわまでの設定
	inline const float kCenterX = 640.0f;     // 中心のx
	inline const float kStartY = 800.0f;      // 登り始めの高さ(画面下の外)
	inline const float kGoalY = 100.0f;       // 登りきったときの高さ(上の方)
	inline const float kClimbSpeed = 3.0f;    // 登る速さの最大(1フレームあたり)
	inline const float kRadiusX = 300.0f;     // 登っている間の横の回転半径
	inline const float kRadiusY = 40.0f;      // 登っている間の奥行きによる縦のゆれ
	inline const float kPeriod = 120.0f;      // 登っている間の1周のフレーム数
	inline const float kBaseSize = 40.0f;     // 基準の大きさ(これが雲の大きさ1.0倍)
	inline const float kScaleRange = 0.4f;    // 登っている間、手前で+40%、奥で-40%
	inline const float kWaveWidth = 550.0f;   // 上に着いてからの横の振れ幅
	inline const float kWaveHeight = 100.0f;  // 上に着いてからの波の高さ
	inline const float kWavePeriod = 300.0f;  // 左右に1往復するフレーム数
	inline const float kWaveCount = 5.0f;     // 左右1往復のあいだに上下する回数(波の細かさ)
	inline const float kBlendZone = 100.0f;   // ゴールの何px手前から横揺れに切り替え始めるか
	inline const float kSlowRate = 0.03f;     // ゴール手前の減速の強さ

	// 攻撃の設定
	inline const float kWaterMax = 150.0f;    // 水が溜まる高さ(画面の下から。小さいほど低い)

	// 雲のボス戦用の設定
	inline const int kWindowWidth = 1280;
	inline const int kWindowHeight = 720;
	inline const int kPixelScale = 2;                       // 何倍書き出しか

	// 新しい絵(Resources フォルダに入れる)
	//  frog_shot.png      :256x64。64x64のコマが横に4つ。吐き出されて飛んでいくカエル(くるくる回る)。張り付くカエルは frog.png のまま
	//  bolt_anim.png      :512x128。128x128のコマが横に4つ。雷。雷の長さぶん、上下につなげて並べる
	//  water_top_wave.png :320x32。水の上の波。64x32のコマが横に5つ(波が立って崩れて静まるアニメ)。コマを順に切り替えて、同じ向きで横に並べる(左右のはしの高さをそろえてあるので、つなぎ目がずれない。水の本体 water.png / water_shock_a.png / water_shock_b.png は、波の絵の色に合わせてある)
	inline const char kFrogShotPath[] = "./resources/cloud/frog_shot.png";
	inline const int kFrogShotSrcSize = 64;                 // 1コマの大きさ(絵のpx)
	inline const int kFrogShotFrameNum = 4;                 // コマの数
	inline const int kFrogShotFrameSpeed = 4;               // 何フレームごとにコマを進めるか(小さいほど速く回る)
	inline const float kFrogShotSize = 64.0f;               // 画面に出す大きさ(px)。絵のまま(1倍)。大きくしたい時は 96.0f や 128.0f
	inline const float kFrogShotRiseSpeed = 9.0f;           // 吐き出されたカエルが上へ飛ぶ速さ(px/フレーム)。小さいほど画面に長く残る(元は14.0f)
	inline const char kBoltPath[] = "./resources/cloud/bolt_anim.png";
	inline const int kBoltSrcSize = 128;                    // 1コマの大きさ(絵のpx)
	inline const int kBoltFrameNum = 4;                     // コマの数
	inline const int kBoltFrameSpeed = 5;                   // 何フレームごとにコマを変えるか(バチバチの速さ)
	inline const int kBoltScale = 1;                        // 雷の絵の拡大率(1で絵のまま。2にすると太く大きくなる)
	inline const char kWaterTopPath[] = "./resources/cloud/water_top_wave.png";
	inline const char kWaterTopShockPath[] = "./resources/cloud/water_top_wave_shock.png";   // 感電中の波(同じ形で黄色)
	inline const int kWaterTopSrcW = 320;                   // 絵全体の横幅(px)
	inline const int kWaterTopFrameW = 64;                  // 1コマの横幅(px)。コマ数は kWaterTopSrcW / kWaterTopFrameW(コマの切れ目が違う時はここを直す)
	inline const int kWaterTopFrameNum = kWaterTopSrcW / kWaterTopFrameW;   // コマの数
	inline const int kWaterTopFrameSpeed = 8;               // 何フレームごとに波のコマを進めるか(小さいほど速い)
	inline const int kWaterTopSrcH = 32;                    // 絵の縦幅(px)
	inline const int kWaterTopScale = 1;                    // 波の絵の拡大率(1で絵のまま。大きいと感じたら1、もっと大きくしたいなら2)
	inline const int kWaterTopCrestRow = 18;                // 絵の上から何行目あたりが水面の高さか(波の山と谷の真ん中あたり)
	inline const int kWaterTopBodyRow = 30;                 // 絵の上から何行目より下は、水の本体(water.png)で埋めるか
	inline const float kWaterTopRiseH = 40.0f;              // 水がこの高さになるまでは、波が画面の下からせり上がってくる
	inline const float kWaterTopScroll = 1.0f;              // 波が横へ流れる速さ(px/フレーム)
	inline const float kWaterTopBob = 3.0f;                 // 波が上下にゆれる幅(px)

	inline const int kStakeMax = 40;                        // 杭の最大数
	inline const float kStakeSpeed = 2.5f;                  // 杭が下へスクロールする速さ(px/フレーム)
	inline const int kStakeInterval = 18;                   // 上から新しい杭が出るフレーム間隔
	inline const float kCloudLockRange = 560.0f;            // 雲に自動照準が届く距離(px)
	inline const float kCloudPriorityRange = 320.0f;        // 雲を杭より優先して狙う距離(px)。これより遠い時は杭が優先で、杭を狙えない時だけ雲を狙う
	// 雲を分けて剥ぎ取る
	inline const int kCloudPartNum = 8;                     // 雲を分けるかたまりの数。これが雲の体力(1つ剥ぎ取るごとに1減る)。kPartDef の行数と合わせる
	inline const int kBossHpMax = kCloudPartNum;
	inline const float kPartDef[kCloudPartNum][3] = {       // かたまりごとの { 横ズレ, 縦ズレ, 大きさ(cloud.png に対する倍率) }。ズレは雲の大きさ1.0倍の時のpx
		{ -80.0f, -6.0f, 0.50f }, { -40.0f, -30.0f, 0.55f }, { 0.0f, -38.0f, 0.60f }, { 40.0f, -30.0f, 0.55f },
		{ 80.0f, -6.0f, 0.50f }, { -40.0f, 22.0f, 0.55f }, { 0.0f, 28.0f, 0.60f }, { 40.0f, 22.0f, 0.55f },
	};
	inline const int kPeelPieceFrames = 50;                 // 剥がれた雲が飛んで消えるまでのフレーム数
	inline const float kPeelPieceSpeed = 14.0f;             // 剥がれた雲が飛んでいく初速(px/フレーム)
	inline const float kGateSplit = 30.0f;                  // 左右の触手のゲートを、横にずらす量(px)
	// 杭の耐久度と引きはがし(sankou.cpp と同じ動き)
	//  ・巻き取る(引っ張る)と、巻いた割合の2乗 × 距離の分だけ杭の耐久度が減る。巻き切る長押しを kStakeFullReels 回やると、杭が抜ける
	//  ・片方の触手で掴んだまま、もう片方の触手で引っ張って、2つの掴んでいる先(杭・雲)が kPeelNeedLen 以上離れていれば、掴んでいたほうが引きはがされる
	inline const float kStakeHpMax = 120.0f;                // 杭の耐久度
	inline const int kStakeFullReels = 4;                   // 通常の杭(sankou.cpp)が抜けるまでの巻き取り回数。このゲームの杭は下の kFallStakeFullReels を使う
	inline const int kFallStakeFullReels = 1;               // 上から落ちてくる杭が抜けるまでの巻き取り回数(このゲームだけの特別処理。1なら、巻き切る1回で壊れる)
	inline float kPeelNeedLen = 400.0f;               // 引きはがしに必要な、掴んでいる2つの先どうしの距離(px)
	inline float kCloudReelChip = 0.25f;              // 雲を1回巻き切った時に削れる体力(かたまり何個分か。1.0で1個剥がれる。0.25なら4回巻き切ると1個剥がれる)
	inline const float kCloudShedAt = 0.5f;                 // 巻きで削れた体力がこの値を超えるたび、周りの雲のかたまりが1つ自動で外れる(0.5=体力の割合を四捨五入した数だけ雲が残る。1.0なら削れ分が丸ごと1個たまってから)
	inline const int kCloudChipShakeFrames = 3;             // 巻いて雲が削れている間の、雲の揺れフレーム数
	inline const int kSunSlot = kStakeMax;                  // 太陽は、杭と同じ配列の最後の番号を使う(杭と同じように掴んで・巻いて・引きはがせる)
	inline const float kSunHpMax = 240.0f;                  // 太陽の耐久度(巻き切る1回で kStakeHpMax 減る。240 なら2回巻き切ると消える)
	inline const float kSunGripMinR = 30.0f;                // 太陽がこの大きさ(半径)以上の間だけ掴める(育つ前・縮んだあとは掴めない)
	inline const float kSunBreakShrink = 4.0f;              // 太陽が壊された時に縮む速さ(半径px/フレーム)
	inline const float kPeelReadyPulse = 0.3f;              // 引きはがせる時の、輪の脈打つ速さ
	inline const int kYankGraceFrames = 24;                 // 引っ張ったあと、もう片方のロープが引っかからず外れる猶予(フレーム)
	inline const int kStakeShakeFrames = 6;                 // 耐久度が減った時に、杭が震えるフレーム数
	inline const float kStakeShakePx = 3.0f;                // 杭が震える幅(px)
	inline const float kStakeDamagedTint = 0.35f;           // 耐久度が0に近い杭の暗さ(1.0 で変化なし。小さいほど赤黒くなる)
	inline const int kStakePeelMax = 8;                     // 抜けて飛んでいく杭を、同時に出せる数
	// 触手の色(どっちがどっちか分かるように、左右で色分けする。0xRRGGBBAA)
	inline const unsigned int kTentColor[2] = { 0xFF78BCFF, 0x62E07AFF };   // 0=L(左・ピンク) 1=R(右・緑)

	// 操作(キーボードとコントローラー)
	// キーボード:A/D=移動  W/S=狙いの上下  SPACE=ジャンプ  Q(またはJ)=触手L  E(またはK)=触手R  LSHIFT=触手を解除  TAB=コントローラーのボタン番号を表示
	// コントローラー:左スティック=移動・狙い  X=ジャンプ  L2(または14番)=触手L  R2(または15番)=触手R  A=触手を解除
	// ボタン番号は環境で違うことがあるので、合わない時はここを書き換える(TABキーで、押したボタンの番号が画面に出る)
	inline const PadButton kPadTentL = kPadButton8;         // L2
	inline const PadButton kPadTentR = kPadButton9;         // R2
	inline const PadButton kPadTentL2 = kPadButton14;       // 触手L用の予備ボタン
	inline const PadButton kPadTentR2 = kPadButton15;       // 触手R用の予備ボタン
	inline const PadButton kPadJump = kPadButton12;         // X
	inline const PadButton kPadRelease = kPadButton10;      // A
	inline const float kStickYSign = 1.0f;                  // スティックを上に倒した時に狙いが下になるなら -1.0f にする
	inline const float kStickDead = 0.25f;                  // スティックの遊び(0〜1)
	inline const int kBossHitFrames = 14;                   // 雲がダメージで揺れるフレーム数
	inline const float kWindDelay = 300.0f;                 // 画面の下の外へ、これだけ(px)落ちるまでは風が吹かない
	inline const float kWindPower = 26.0f;                  // 風で吹き上げる速さ
	inline const float kWindEndY = 360.0f;                  // 吹き上げられて、この高さ(画面の縦の真ん中)まで戻ったら風が止む
	inline const float kWindDrift = 0.04f;                  // 風で吹き上げられる間の、画面の真ん中(横)へ寄る割合
	inline const float kSpawnVy = 26.0f;                    // 下から出てくる時の、上への勢い
	inline int kRestFrames = 150;                     // 攻撃のあとの休憩フレーム数(4と5のあとは休憩なし)
	inline const int kFrogPerVolley = 1;                    // 1回の射出で出すカエルの数(2以上なら、左から右へ角度を等間隔に分けて出す。1なら射出ごとに左右交互)
	inline const int kFrogVolleyNum = 6;                    // 射出する回数(カエルの総数は 1回の数 × 回数 = 6匹)
	inline const int kFrogVolleyInterval = 12;              // 射出と射出の間のフレーム数
	inline const int kFrogNum = kFrogPerVolley * kFrogVolleyNum;   // 4の攻撃で出すカエルの数(吐き出すカエルも、張り付くカエルも同じ数)
	inline const int kFrogCols = 4;                         // 張り付くカエルを置く、画面のマスの列数(1マスに1匹。列×行 がカエルの数以上になるようにする)
	inline const int kFrogRows = 2;                         // 同じく行数
	static_assert(kFrogNum <= kFrogCols * kFrogRows, "カエルの数がマスより多い");
	inline const int kFrogStagger = 6;                      // 張り付くカエルが、1匹ごとに遅れて跳ぶ・剥がれるフレーム数
	inline const int kFrogStayFrames = 650;                 // カエルが張り付いてから、剥がれ始めるまでのフレーム数(大きいほど長く張り付く。元は400)
	inline const float kFrogStuckRadiusMin = 100.0f;        // 張り付くカエルの大きさ(半径px)の最小
	inline const float kFrogStuckRadiusRand = 50.0f;        // 大きさのばらつき
	inline const int kFrogEndFrames = kFrogStayFrames + (kFrogNum - 1) * kFrogStagger + 25; // 最後のカエルが剥がれきって消えるフレーム

	// 雲を倒した時の演出
	inline const int kClearHold = 20;                       // 倒してから、雲が中央へ動き出すまで止まっているフレーム数
	inline const int kClearMoveFrames = 40;                 // 雲が画面の中央へ動くフレーム数
	inline const int kClearScatterAt = kClearHold + kClearMoveFrames;   // 雲が四散するフレーム
	inline const float kClearCenterX = 640.0f;              // 雲が四散する場所(画面の中央)
	inline const float kClearCenterY = 360.0f;
	inline const int kScatterNum = 14;                      // 四散する雲のかけらの数
	inline const int kScatterFrames = 70;                   // かけらが消えるまでのフレーム数
	inline const float kScatterSpeedMin = 6.0f;             // かけらが飛び散る速さ(最小)
	inline const float kScatterSpeedMax = 16.0f;            // かけらが飛び散る速さ(最大)
	inline const float kScatterFriction = 0.93f;            // かけらの速さが1フレームごとに残る割合
	inline const float kEyeSize = 56.0f;                    // 目玉の大きさ(雲の大きさ1.0倍の時のpx)
	inline const char kEyePath[] = "./resources/cloud/eye.png";  // 目玉の絵のファイル名(違う名前なら変更)
	inline const int kEyeSrcSize = 64;                      // eye.png の元画像のピクセルサイズ(絵に合わせて変更)
	inline const int kEyeWaitFrames = 20;                   // 四散してから、目玉が逃げ始めるまでのフレーム数
	inline const int kEyeRampFrames = 90;                   // 目玉のぐるぐるが、中央から本来の大きさまで広がるフレーム数
	inline const float kEyeStartVy = 1.0f;                  // 目玉が登り始める上への速さ(px/フレーム)
	inline const float kEyeAccel = 0.04f;                   // 目玉が登る時の加速(1フレームごと)
	inline const int kFlyOutDelayFrames = 20;               // 目玉が画面の上へ出ていってから、プレイヤーが杭を狙い始めるまでのフレーム数
	inline const float kFlyOutShootVy = 5.0f;               // 上への勢いがこれ以下になったら、また杭へ触手を撃つ(px/フレーム)
	inline const float kFlyStakeY = 30.0f;                  // プレイヤーを引っ張り上げる杭の高さ(画面の上のほう。プレイヤーの真上に出る)
	inline const float kFlyOutStopY = -300.0f;              // 画面の上へ出て、ここまで来たら止まる(画面の外なので見えない)
	inline const int kEndFadeFrames = 90;                   // 目玉が画面の外へ出てから、真っ暗になるまでのフレーム数
	inline const float kEyeMaxVy = 4.5f;                    // 目玉の上への速さの最大(登場時の雲の最大は kClimbSpeed)

	// プレイヤー
	inline const float kPlayerW = 64.0f;
	inline const float kPlayerH = 128.0f;
	inline const float kPlayerHalfW = kPlayerW / 2.0f;
	inline const float kPlayerHalfH = kPlayerH / 2.0f;
	inline const int kPlayerSpriteW = 32;
	inline const int kPlayerSpriteH = 64;
	inline const int kAnimationFrameCount = 6;
	inline const int kAnimationSpeed = 5;

	inline const float kPlayerSpeed = 8.0f;
	inline const float kGravity = 0.8f;
	inline const float kMaxFallSpeed = 24.0f;
	inline const float kJumpPower = 20.0f;
	inline const float kJumpCut = 5.0f;

	// 触手とゲート
	inline const int kTentacleSeg = 96;                      // 触手1本を何個のパーツで描くか
	inline const int kTentacleSpriteW = 16 * kPixelScale;
	inline const int kTentacleSpriteH = 32 * kPixelScale;
	inline const float kTentacleBaseHalfSrc = 8.0f;         // 根元の太さ(絵のドット数・片側)
	inline const float kTentacleTipHalfSrc = 1.0f;          // 先っぽの太さ
	inline const int kTentacleStepRows = 4;                 // 絵を縦に何行ずつ区切って貼るか
	inline const float kTentacleStep = static_cast<float>(kTentacleStepRows * kPixelScale);
	inline const int kTentacleMaxSample = 192;
	inline const int kRetractFrames = 8;
	inline const int kGateOpenFrames = 6;
	inline const int kGateCloseFrames = 6;
	inline const float kGateBackX = 96.0f;
	inline const float kGateBackY = 112.0f;
	inline const float kGateFollow = 0.25f;
	inline const int kGateSpriteW = 32 * kPixelScale;
	inline const int kGateSpriteH = 96 * kPixelScale;
	inline const float kGateHalfW = kGateSpriteW / 2.0f;
	inline const float kGateHalfLen = kGateSpriteH / 2.0f;
	inline const float kKnockFriction = 0.92f;              // 横への反動が、1フレームごとに残る割合
	inline const int kKnockHoldFrames = 6;                  // 横への反動が、減り始めるまでに勢いを保つフレーム数
	inline const float kKnockInputRate = 0.25f;             // 反動で飛んでいる間の、左右キーの効き(1.0 で通常どおり)

	// ひっかけポイント(木の杭)
	inline const float kGripRadius = 18.0f;                 // 杭の半径(px)
	inline const int kGripHalfSrc = 2;                      // 巻き付いている部分の触手の太さの目安(絵のドット数・片側)
	inline const float kGripWireHalf = static_cast<float>(kGripHalfSrc * kPixelScale);   // 巻き付き部分の太さ(px・片側)
	inline const float kGripCoilGap = 4.0f;                 // 巻きの1周ごとに増える半径(px)
	inline const float kGripTurns = 1.0f;                   // 杭に何周巻くか
	inline const float kGripReachSpeed = 45.0f;             // 杭へ伸びる速さ(px/フレーム)
	inline const int kGripUnwindFrames = 14;                // 巻き解いて、しまうまでのフレーム数
	inline const float kGripPullSpeed = 22.0f;              // 杭へ引っ張られる速さ(px/フレーム)
	inline const float kGripLaunchSpeed = 26.0f;            // 離した時に、杭の向きへ飛ぶ速さ
	inline const float kGripReleaseDist = 40.0f;            // 杭にこの距離まで近づいたら離す(px)
	inline const int kGripPullMax = 40;                     // 引っ張りの最大フレーム数(壁に引っかかった時の保険)
	inline const float kGripLockRange = 400.0f;             // 自動照準が届く距離(px)
	inline const float kGripLockCos = 0.5f;                 // 自動照準が効く角度(cos。0.5 = 左右60度まで)
	inline const int kGripMarkSpriteSize = 32 * kPixelScale;
	inline const float kGripMarkSize = 72.0f;               // 狙っているマークを描く大きさ(px)
	inline const float kGripMarkSizeSub = 44.0f;            // 狙っていない(届く範囲にある)マークの大きさ(px)
	inline const int kMaxGripCand = 24;                     // 自動照準の候補の最大数

	//入力方向の矢印
	inline const float kAimArrowRadius = 96.0f;
	inline const float kAimArrowSize = 40.0f;
	inline const int kAimArrowSpriteSize = 16 * kPixelScale;
	inline const float kAimArrowFollow = 0.35f;

	// プレイヤーの左下・右下に出す L/R の表示(スプライト)
	//  ui_lr.png    :64x32。左のコマ=L、右のコマ=R(各32x32)。白地なので、触手の色(kTentColor)をかけて色分けする
	//  ui_cycle.png :64x64。半円形の矢印が2本(リサイクルマークのような形)。触手がひっかけポイントに着いている間、L/Rのまわりを回る
	inline const int kLRSpriteSize = 32;                    // ui_lr.png の1コマの大きさ(絵のpx)
	inline const int kCycleSpriteSize = 64;                 // ui_cycle.png の大きさ(絵のpx)
	inline const float kLRUiOffsetX = 58.0f;                // プレイヤーの中心から、L/Rまでの横の距離(px)。Lは左、Rは右
	inline const float kLRUiOffsetY = 40.0f;                // プレイヤーの中心から、L/Rまでの下への距離(px)
	inline const float kLRUiSize = 36.0f;                   // L/R を描く大きさ(px)
	inline const float kLRRingSize = 72.0f;                 // 回る矢印を描く大きさ(px)
	inline const float kLRRingSpeed = 0.12f;                // 矢印が回る速さ(ラジアン/フレーム。ひっかけ中)
	inline const float kLRRingPullSpeed = 0.28f;            // 引っ張っている間の、矢印が回る速さ
	inline const float kLRRingFade = 0.25f;                 // 矢印が出る・消える速さ(1フレームで増減する量)
	inline const float kLRMarkSize = 26.0f;                 // 杭のそばや狙いのマークに出す、小さなL/Rの大きさ(px)

	// 引っ張り中の見せ方(引っ張っている触手を明るく光らせる。引っ張っていない触手は、ふつうの色のまま)
	inline const float kPullBright = 0.45f;                 // 引っ張っている触手を、白へどれだけ近づけるか(0〜1)
	inline const int kPullGlowAlphaMin = 60;                // 光のふくらみ(重ね描き)の薄い時の濃さ
	inline const int kPullGlowAlphaMax = 150;               // 光のふくらみの濃い時の濃さ
	inline const float kPullGlowSpeed = 0.35f;              // 光が脈打つ速さ

	// ひっかけの段階
	inline const int kGripReach = 0;                        // 杭へ伸びている
	inline const int kGripHold = 1;                         // 巻き付いて、ロープのようにぶら下がっている
	inline const int kGripPull = 2;                         // プレイヤーを引っ張っている(ボタンを押し続けている間)
	inline const int kGripUnwind = 3;                       // 巻きを解いて、触手をしまっている

	// 触手セットの状態
	inline const int kSetOut = 1;                           // 出ている
	inline const int kSetRetract = 2;                       // 戻っている最中 → ゲートが閉じている最中

	// 構造体
}
