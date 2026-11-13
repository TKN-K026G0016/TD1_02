#pragma once
// ================================================================
// 【操作・ゲームバランスの調整場所】
// ここを変えると移動・ジャンプ・触手攻撃・グラップルの感触が変わる。
// ワールド座標は「上が+Y」。画像リソース不要。
// ================================================================
namespace PortalConfig {
	constexpr float kFloor = 64.0f;
	constexpr float kWorldCeiling = 575.0f; // 【固定画面】画面上へ抜けない高さ（ジャンプは維持）

	// 【移動】地上ではキビキビ、空中では速度を残して方向修正可能。
	constexpr float kMoveSpeed = 6.4f;
	constexpr float kGroundAcceleration = 1.40f;
	constexpr float kGroundBrake = 1.85f;
	constexpr float kAirAcceleration = 0.78f;
		constexpr float kAirBrake = 0.16f;
		// 【録画のような慣性】ワイヤー射出直後は横速度をすぐ通常走行速度に戻さない。
		constexpr int kLaunchMomentumFrames = 8; // 【慣性】射出後の勢いを保つフレーム。大きいほど滑る
	constexpr float kLaunchMomentumDrag = 0.7f; // 【慣性】ニュートラル時の減速率。小さいほど止まりやすい
	constexpr float kLaunchAirSteer = 2.5f; // 【操作感】射出後、同じ方向へ加速する強さ
	constexpr float kLaunchAirReverseBrake = 2.6f; // 【操作感】逆方向入力で素早くブレーキ・切り返し
	constexpr float kLaunchAirMaxX = 24.0f; // 射出中の横速度上限
	constexpr float kGravity = 0.58f;
	constexpr float kJumpSpeed = 17.8f; // 【ジャンプの高さ】↑で高くなる。SPACE単独でジャンプ
	constexpr float kJumpReleaseMultiplier = 0.94f; // 【短押しと長押しの差】0.94=短押しでも高め
	constexpr int kJumpBufferFrames = 8;  // 【先行入力】着地前にSPACEを押してもジャンプ予約
	constexpr int kCoyoteFrames = 7;      // 足場を離れた直後もジャンプ可能

	// ================================================================
	// 【新操作：スティック2本でアクション完結】
	// 左スティック: 左右移動 / 上でジャンプ。右スティック: 倒すだけで8方向触手攻撃。
	// 方向をロックしたまま数フレーム保持したときのみ移動杭を掴む。
	// 外装フックは保持で張力をため、右スティックを戻して剥がす。
	// キーボードは従来どおりA/D・矢印・SPACE・Jのまま。
	// ================================================================
	constexpr float kReach = 760.0f;  // 【広い射程】510→650。ロック角度は広げすぎない
	constexpr float kLeftStickJumpThreshold = 0.60f; // 上に倒すとジャンプ
	constexpr float kRightStickPressThreshold = 0.38f; // 右スティックを倒したと認める強さ
	constexpr float kRightStickReleaseThreshold = 0.22f; // 戻し側。入力の細かな揺れを防ぐ
	constexpr int kRightStickRepeatFrames = 17; // 右を倒し続けた場合の連続攻撃の間隔
	constexpr int kRightStickChangeMinFrames = 5; // 右を切り替えたときの最短攻撃間隔
	constexpr int kStickGrappleHoldFrames = 7; // ロックした杭を9F狙い続けると自動接続
	constexpr float kGrappleSpinRadius = 96.0f; // 【軽い回転攻撃】グラップル中、触手の周囲に攻撃判定
	// 【攻撃が出ない問題】触手の描画枠が埋まっていても、攻撃判定は独立して発動する。
	// ボスが大きくなった分、通常触手の前方リーチも広げる。
	constexpr float kAttackReach = 345.0f; // プレイヤー中心から先端までの距離
	constexpr float kNormalAttackThickness = 30.0f; // 攻撃線分の当たり判定の太さ
	constexpr int kNormalAttackFrames = 14; // 攻撃判定が有効な時間
	constexpr int kNormalAttackActiveStart = 1; // 何フレーム目から当たるか
	constexpr int kNormalAttackActiveEnd = 11; // 何フレーム目まで当たるか
	// 【攻撃優先】固定杭は適度に吸着、ボス外装は意識して狙う必要がある。
	// 【直した原因①】8方向キーでもボスのフックを捕捉しやすくする。
	// 右スティックは細かく狙えるが、キーボードの45度間隔でも届く程度に補助する。
	// ※ 明示的な照準が必要なので、A/Dで走りながらJを押しても勝手に掴まない。
	constexpr float kArmorAimDot = 0.74f; // 約28度以内。ただし横ずれ距離でも制限する
	constexpr float kArmorAimSideMax = 170.0f; // キーボード8方向でも狙える距離補助
	constexpr float kAnchorAimDot = 0.70f;  // 【グラップル補助】約28度。狙った杭の保持が条件
	constexpr float kAnchorLockSideMin = 100.0f; // 近くで少し照準がずれても捕捉できる
	constexpr float kAnchorLockSideMax = 205.0f; // 遠距離でも補助は最大112px
	constexpr float kAnchorLockSideRatio = 0.38f; // 射程とともに横ずれ許容を広げる

	constexpr float kAnchorNearbyRadius = 105.0f; // 旧設定：誤グラップル防止のため現在は使用しない
	constexpr float kAnchorAimWeight = 230.0f;   // 旧設定：選択処理では現在使用しない
	constexpr float kPullAcceleration = 2.55f; // 自動巻き取りの加速
	constexpr float kMaxPullSpeed = 24.0f; // 最大巻き取り速度
	// 【自動射出】杭までこの距離に近づいたら、止まらずワイヤーを外して飛ぶ。
	// 【引っ掛かり対策】杭を目前にして減速する前に、通過速度を見て自動射出する。
	constexpr float kAutoLaunchDistance = 89.0f; // 基本の離脱距離
	constexpr float kAutoLaunchLeadFrames = 1.05f; // 接近速度×このフレーム数を先読み
	constexpr float kAutoLaunchMaxLead = 28.0f; // 遠すぎる位置から射出しない上限
	constexpr float kPullMomentumKeep = 0.985f; // 杭の直前でも慣性を残す（旧:0.91）
	// 【新しいシームレスワイヤー】杭へ最接近する少し前に解除。射出時に向き/高さを作り直さない。
	constexpr int kGrappleMinFrames = 4; // 接続直後に誤解除しない最低フレーム数
	constexpr float kGrappleReleaseForwardDot = 0.05f; // 進行方向への投影が正の場合に通過判定
	constexpr float kGrappleExitSpeedBonus = 1.0f; // 【射出の爽快感】現在速度と同じ方向へ加算する1px/F
	constexpr float kGrappleExitMaxSpeed = 25.0f; // 【固定画面】射出速度の上限。方向は変更しない
	constexpr float kGrappleStallSpeed = 5.5f; // 遅すぎる時だけ元の接続方向へ最低限進ませる
	constexpr float kGrappleStallImpulse = 2.0f; // 低速救済。高速時には一切加算しない
	// 旧kLaunchSpeed/kLaunchUpBoost/kLaunchMinUpSpeedは互換のため残しているが、
	// 自動射出では使用しない（上向き強制をなくして引っ掛かりを防止）。
	constexpr float kLaunchSpeed = 12.0f; // 旧設定：自動射出では使用しない
	constexpr float kLaunchUpBoost = 2.5f; // 旧設定：自動射出では使用しない
	constexpr float kLaunchMinUpSpeed = 1.5f; // 旧設定：強制上昇をなくしたため使用しない
	constexpr float kLaunchMaxUpSpeed = 14.0f; // 【固定画面】上へ飛びすぎない上限
	constexpr float kLaunchVerticalCarry = 0.30f; // 巻き取り中の上向き速度を少しだけ引き継ぐ割合
	constexpr float kLaunchSteer = 3.0f; // 【射出】接続時の照準方向へ少し押す。A/D入力は使用しない
	constexpr float kGrappleJumpForwardSpeed = 10.0f; // 【SPACEで早期離脱】後方へ飛ばない最低水平速度
	constexpr float kLaunchDirectionDeadZone = 0.22f; // 小さなスティック揺れでは射出方向を変えない
	constexpr float kLaunchAimDirectionDeadZone = 0.45f; // 接続時、横照準として認める最小値
	constexpr int kLaunchVisualFrames = 13;     // 弾き出し直後の演出期間
	constexpr int kLaunchRegripLockFrames = 9;  // 【同じ杭だけ】再キャッチ禁止。別の杭はすぐ掴める
	constexpr float kOneButtonJumpAimY = 0.65f; // 旧設定：上＋Jジャンプは廃止（互換のため保持）
	constexpr float kGripSwingAcceleration = 0.27f;
	constexpr float kDownAttackBoost = 11.5f;
	constexpr int kAttackCooldownFrames = 7; // Jの連打が効くまでの時間（攻撃だけは短め）
	constexpr int kAttackBufferFrames = 7;
	constexpr int kTentacleExtendFrames = 4; // 触手の接続応答速度（フレーム）
	constexpr int kTentacleRetractFrames = 9;
	constexpr int kGateCount = 4;
	// 【外装剥がし】取っ掛かりをJで狙ってから、触手で引き剥がすまでのフレーム。
	// 【装甲を剥がす駆け引き】フックを狙ってJを押し、そのまま長押しで張力をためる。
	// Jを離した時、許容範囲のフレームなら外装が剥がれる。早過ぎ/遅過ぎは失敗。
	// ボスに当たるだけで自動的に剥がす旧仕様にはしない。
	// 【直した原因②】張力が十分にたまったら、ボスが動いてもつないだまま。
	// ゲージが緑になった後は「離すまで」成功状態を維持。
	// 以前の26～39Fという13Fだけの成功猶予は、ボス戦中には厳しすぎた。
	constexpr int kArmorPullMinFrames = 22;    // 最低限、引っ張る必要のある時間
	constexpr int kArmorPullSweetStart = 22;  // 緑ゲージへ変化するタイミング
	constexpr int kArmorPullSweetEnd = 70;    // 表示用。長押しし過ぎても失敗にしない
	constexpr int kArmorPullMaxFrames = 70;   // 表示ゲージの上限。ここで自動解除はしない
	constexpr int kArmorPullFailCooldown = 9; // 失敗後もすぐ再挑戦できる
	constexpr int kArmorPeelWindupFrames = kArmorPullSweetStart; // 以前の設定名との互換

	// 【空中スピン攻撃】J押下、かつ杭を狙っていないとき発動。
	constexpr int kSpinFrames = 23;
	constexpr int kSpinActiveStart = 3;
	constexpr int kSpinActiveEnd = 19;
	constexpr float kSpinRadius = 90.0f;
	constexpr float kSpinLift = 1.8f;      // 発動時の落下ブレーキ
	// 【ヒットの手応え】追加の粒子を出さず、既存BOXの色/傾き・ボスの硬直で表現。
	constexpr int kHitConfirmFrames = 7; // 命中後、プレイヤーの色が変わるフレーム数
	constexpr int kHitPoseHoldFrames = 3; // 触手アニメーションの時間だけを短く止める（移動は止めない）
	constexpr float kNormalHitRecoil = 0.8f; // 通常攻撃の命中時に軽く手前へ反動
	constexpr float kSpinHitLift = 0.65f; // スピン命中時の小さな浮き上がり
	constexpr int kArmorPeelConfirmFrames = 11; // 外装を剥がしたときは少し長く強調する
	constexpr int kSpinDamage = 18;
	constexpr int kNormalDamage = 12;
	constexpr int kGrappleDamage = 7;    // グリップ中の触手にも攻撃判定

	// 【コントローラー】入力番号は既存Input.cppに合わせた。
	constexpr int kStickDeadZone = 8000;
	// コントローラー攻撃ボタンは廃止。右スティックを倒すだけで攻撃する。
	constexpr int kAttackButton = 12; // 旧互換定義（ゲーム内では入力に使わない）
	constexpr int kJumpButton = 10; // A / PSの×でもジャンプ可（左スティック上だけでも可）
	constexpr int kResetButton = 13;
}
