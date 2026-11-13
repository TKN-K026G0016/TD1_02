#pragma once
// ================================================================
// 【第1ステージ：ガラクタコプター】調整したい数値はここに集約。
// 外部画像・大量パーティクルなし。位置・速度・硬直で迫力を表現する。
// ワールド座標は上方向が +Y。
// ================================================================
namespace BossStage1 {
	// 【コプターの大きさ】BOXとコライダーをそろえて変更する。
	constexpr float kBossWidth = 256.0f;
	constexpr float kBossHeight = 256.0f;
	constexpr float kBossHitRadius = 118.0f;
	constexpr float kArmorHookSpacing = 43.0f;
	constexpr float kArmorHookHeight = 48.0f;
	constexpr int kMaxHp = 500;                  // 外装が剥がれた後のコアHP
	constexpr int kArmorPlates = 4;              // 外装の枚数
	constexpr int kArmorHitsPerPlate = 2;        // 通常攻撃による外装の揺れ周期（剥がすのはフック攻撃のみ）
	constexpr int kGripArmorDamage = 2;          // 旧版互換値：外装は専用のフック攻撃で剥がす
	constexpr int kNormalHitCooldown = 9;        // 攻撃ヒット間隔
	// 【触手のヒット感】外装に当たっただけでもボスの動きを短く止める。
	// 新しいパーティクルは出さず、Camera.cppの既存機能だけで表現する。
	constexpr int kArmorHitstopNormal = 2; // 通常攻撃が外装に当たったときの停止フレーム
	constexpr int kArmorHitstopSpin = 3;   // 空中回転攻撃が外装に当たったときの停止フレーム
	constexpr float kArmorShakeNormal = 1.2f; // 通常攻撃の画面揺れ強さ（小さめ）
	constexpr float kArmorShakeSpin = 2.6f;   // 回転攻撃の画面揺れ強さ
	constexpr int kArmorBreakHitstop = 5;
	// 【四隅ネジ】4つ全部外して初めて外装がめくれる。長い攻撃チャンスの後に吹き飛ばし。
	constexpr int kArmorCurlFrames = 38;
	constexpr int kArmorStunFrames = 210;
	constexpr int kArmorWakeFrames = 35;
	constexpr float kWakeBlastRadius = 700.0f;
	constexpr float kWakeBlastPushX = 13.0f;
	constexpr float kWakeBlastPushDown = 18.0f;        // 破壊時の停止フレーム（ボスだけ）
	constexpr int kImpactHitstop = 4;            // プレス接地の停止フレーム
	constexpr int kIdleFrames = 30;              // 攻撃間隔。行動後はホームへ戻り次の予兆へ
	// 【5技の予兆】同じ長さだと単調なので個別に調整可能。
	constexpr int kRushWarningFrames = 37; // 地上突進: ワイヤーで避ける準備
	constexpr int kPressWarningFrames = 42; // プレス: 影と落下範囲が見える時間
	constexpr int kShotWarningFrames = 30; // 射撃: 予兆後に2セットの弾
	constexpr int kToyWarningFrames = 34; // コマ/ヨーヨー: 投擲前の構え
	constexpr float kShotPredictFrames = 9.0f; // 射撃の狙い: 現在の横移動を少し先読み
	constexpr float kShotReturnSpeed = 4.5f; // 発砲後の反動から戻る速さ
	constexpr float kYoYoRange = 490.0f; // ヨーヨーの伸び
	constexpr float kYoYoHeightWave = 38.0f; // 上下に揺れる高さ
	constexpr int kYoYoDurationFrames = 98; // ヨーヨーが伸びて戻るフレーム数
	constexpr int kWarningFrames = 35;           // 予兆
	constexpr float kRushMaxSpeed = 22.0f; // 大きくなったボスの重量感が出る加速突進       // 突進の最大速度
	constexpr float kRushAccel = 1.9f; // 予兆は残し、突進に入ったら素早く加速          // 徐々に加速する
	constexpr int kRushFrames = 56; // 固定高度で進む最大時間。早く突進が終わる場合もある
	constexpr float kRushOvershoot = 225.0f; // プレイヤーの位置を越えて走り抜ける距離              // 突進の持続時間
	constexpr int kRushBrakeFrames = 10;         // 突進後の短い失速（反動）
	constexpr int kWireDodgeStunFrames = 78;     // ワイヤーで突進を避けたときの反撃可能時間
	constexpr float kRushGroundY = 64.0f + kBossHeight * 0.5f; // 【重要】突進は床スレスレの固定高度のみ。空中突進なし。
	constexpr float kPressDiveAccel = 2.2f;      // プレス落下加速度
	constexpr float kPressDiveMaxSpeed = 42.0f; // 大きな機体の急降下感  // プレス落下速度上限
	constexpr int kPressStunFrames = 47;         // プレス後、弱点攻撃チャンス
	constexpr int kToyDuration = 120;             // コマ／ヨーヨー攻撃の時間
	constexpr float kToyRadius = 23.0f; // コマ/ヨーヨーの見た目と当たり判定を拡大          // コマの当たり判定
	constexpr int kToyDamage = 12;
	constexpr int kBulletDamage = 13;
	constexpr float kArenaLeft = 100.0f;
	constexpr float kArenaRight = 1210.0f;
	constexpr float kFloorY = 64.0f;
	constexpr float kCoreBonusDamage = 1.65f;    // プレス硬直中は攻撃威力上昇
	// true: 初回は「射撃→突進→プレス→コマ→ヨーヨー」を順に見せる。
	// false: 初回から位置関係・距離を中心に攻撃を選択する。
	// 【AI】最初から状況判断に任せる。trueにすれば従来の初回5回固定順に戻せる。
	constexpr bool kScriptedTutorialFirstFive = false;
	// 【AIの変化】同じ攻撃になりにくくする補正。数値が大きいほど行動が散る。
	constexpr float kAIRepeatPenalty = 47.0f;
	constexpr float kAIPreviousTwoPenalty = 19.0f;
	constexpr float kAIJitter = 11.0f; // 少しだけ選択に幅を持たせる（攻撃の予兆は必ず出す）
	// 【攻撃の手応え】追加描画はせず、ダメージと硬直だけを変える。
	constexpr int kNormalComboWindow = 100;
	constexpr int kNormalComboBonusPerHit = 3;
	constexpr int kWireLaunchBonusDamage = 5;
	constexpr int kSpinCounterStunFrames = 52;
	constexpr int kSpinCounterCooldown = 110;
}
