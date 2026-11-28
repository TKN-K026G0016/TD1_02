// 【このファイルの役割】BossBase.h — ステージ1・2共通のボス状態と対外API
// ボスのHP、位置、行動状態、外装、飛び道具などを宣言する。
// ============================================================================

#pragma once
#include "Vector2.h"
#include "BossVisualEffect.h"
#include "BossStage1Tuning.h"
#include "BossBattleSettings.h"

// 第1ステージの行動AI。本体・プロペラ・ジェット・パイプ・目を分割画像で描画する。
// StageがBossの実体を所有し、Player座標への読み取り専用ポインタを渡す。
enum class BossState { Idle, Warning, Rush, Recovery, Dizzy, ArmorCurl, ArmorFalling, ArmorImpact, ArmorStun, WakeBlast, Press, Shot, Toy, ScrewRepair, Stage2Special, Defeated };
enum class BossAttack { Press, Shot, Rush, Top, YoYo };

class Boss {
public:
	static constexpr int kMaxHp = BossStage1::kMaxHp;
	static constexpr float kHitRadius = BossStage1::kBossHitRadius; // 大きくした本体に合わせた円判定
	void Initialize(Vector2 spawnPos);
	// ゲーム起動時/F8リロード時にエディターで調整した5技とフック位置を反映。
// エディターで保存した戦闘設定を、このオブジェクトへ反映する。
	void SetBattleSettings(const BossBattleSettings& config) {
		if (config.Valid()) {
			battleSettings_ = config;
			if (hp_ > GetMaxHp()) hp_ = GetMaxHp();
		}
	}
	void SetTarget(const Vector2* playerPosition);
	// 【AI入力】速度・地上/空中・ワイヤーの使用状況を受け取る。
	// PlayerをBossに所有させない（既存のクラス分担を維持）。
	void SetTargetMotion(Vector2 velocity, bool grounded, bool usingWire);
	void Update();
	bool TriggerAttackForEditor(BossAttack attack); // 5技とも予兆から強制テスト
	bool TriggerFlybyForEditor(int style); // ステージ2：0水平/1高高度/2急上昇/3 S字
	bool BeginScrewRepair(int screwIndex);
	bool IsRepairingScrew() const { return state_ == BossState::ScrewRepair; }
	int RepairScrewIndex() const { return repairingScrew_; }

	//テクスチャの取得
	void LoadTexture(void) const;
	//描画処理
	void Draw() const;
	void DrawBolts() const;

	// グラップルの綱も外装剥がしに使う。第2引数はその判定。
	// 戻り値: このフレームにダメージ/外装ヒットが成立したらtrue。
	// 攻撃種の違い：通常=連続ヒット、スピン=カウンター、グラップル=外装剥がし。
	// 射出直後は攻撃の威力も少し上がる。互換性のため既存の引数も維持。
	bool TakeDamage(int amount, bool isGrappleHit = false,
		bool isSpinHit = false, bool fromWireLaunch = false);
	int TryHitPlayer(Vector2 playerPosition, float playerRadius, Vector2* hitSource);
	int GetHp() const { return hp_; }
	int GetMaxHp() const { return battleSettings_.bossMaxHp; }
	void SetCurrentHpForEditor(int value) {
		if (IsDefeated()) return; // 撃破済み状態を数値変更だけで復活させない
		hp_ = value < 1 ? 1 : (value > GetMaxHp() ? GetMaxHp() : value);
	}
	int GetArmorPlates() const { return armorPlates_; }
	int GetMaxArmorPlates() const { return BossStage1::kArmorPlates - 1; }
	bool IsCoreExposed() const { return armorPlates_ == 0 && AllAccessoriesRemoved(); }
	bool IsDefeated() const { return hp_ <= 0; }
	bool IsEnraged() const { return hp_ * 100 <= GetMaxHp() * battleSettings_.phase2HpPercent; }
	bool IsWeakPointOpen() const; // プレス硬直・ワイヤー回避後の失速中は弱点ダメージUP
	bool IsDizzy() const { return state_ == BossState::Dizzy || state_ == BossState::ArmorStun; }
	bool IsArmorStun() const { return state_ == BossState::ArmorStun; }
	bool IsArmorCurling() const { return state_ == BossState::ArmorCurl; }
	bool IsWaking() const { return state_ == BossState::WakeBlast; }
	// スタン復帰時の吹き飛ばしをStage側に一度だけ渡す。
	bool ConsumeWakeBlast();
	// 【連打イベント】右スティックを弾き直した瞬間だけ呼ぶ。通常の攻撃判定と独立。
	// 戻り値は今回の連打が有効だったか。
	bool RegisterMashHit();
	int GetMashCount() const { return mashCount_; }
	int GetMashTimeLeft() const {
		return state_ == BossState::ArmorStun ?
			BossStage1::kMashTimeFrames - stateFrame_ : 0;
	}
	bool IsMashActive() const { return state_ == BossState::ArmorStun; }
	bool IsArmorFalling() const { return state_ == BossState::ArmorFalling || state_ == BossState::ArmorImpact; }
	bool IsCoasterStage() const { return coasterStage_; }
	void StartCoasterStage();
	// ステージ2専用: 1=大型ミサイル、2=8の字カッター。予兆→攻撃→離脱を共有。
	int GetStage2SpecialMode() const { return state_ == BossState::Stage2Special ? stage2SpecialMode_ : 0; }
	int GetStage2SpecialFrame() const { return stateFrame_; }

	void NoteWireMotion(bool usingWire); // プレイヤーのワイヤー使用をAIへ報告
	Vector2 GetPosition() const { return position_; }
	Vector2 GetCoreHitPoint() const { return { position_.x + battleSettings_.coreOffset.x,position_.y + battleSettings_.coreOffset.y }; }
	float GetCoreHitRadius() const { return battleSettings_.coreRadius; }
	// 外装用のボス本体グリップポイント。プレイヤーが直接ワイヤーで剥がせる。
	// 【外装の取っ掛かり】3枚にそれぞれ照準ポイントを持たせる。
	// 到着時に外装を剥がし、プレイヤーもワイヤーで引き寄せる。
	// 本体1点は移動用（破壊不可）。プロペラ3点だけを剥がせる。
	bool IsArmorAttached(int plate) const;
	Vector2 GetArmorGripPoint(int plate) const;
	// ジェット4・パイプ1・目1。目は最後まで外れず弱点として残る。
	static constexpr int kAccessoryGripCount = 6;
	Vector2 GetAccessoryGripPoint(int index) const;
	bool IsAccessoryGripEnabled(int index) const;
	bool TryPeelAccessory(int index); // ジェット4個・パイプを引き剥がす。目は破壊不可
	bool AllAccessoriesRemoved() const;
	// プレイヤーがJでフックを引いている最中、板をたわませる
	void SetArmorStrain(int plate, float progress);
	bool TryPeelArmor(int plate); // ポイントを狙って触手で引いた時だけ外装を剥がす
	Vector2 GetArmorGripPoint() const { return GetArmorGripPoint(1); } // 旧API互換
	BossState GetState() const { return state_; }
	BossAttack GetSelectedAttack() const { return nextAttack_; }
	int GetAttackCount() const { return attackCount_; }
	int GetNormalCombo() const { return normalComboHits_; }

private:
	void DrawCopterArt(float bodyLean) const;
	void DrawCoasterArt(float bodyLean) const;
	enum class PressPhase { Align, Charge, Dive, Impact, Stun, Return };
	struct Bullet {
		Vector2 position = {}, velocity = {};
		int life = 0;
		bool active = false;
		bool debris = false; // プレスで落ちた部品だけ重力をかける
	};
	static constexpr int kMaxBullets = 12;

	BossBattleSettings battleSettings_ = {};
	Vector2 position_ = {}, flightDestination_ = {}, rushStart_ = {};
	Vector2 flightVelocity_ = {}; // 通常時のふわっとした空中移動の慣性
	int flightTimer_ = 0;       // 次の待機地点を選び直す間隔
	int flightDirection_ = 1;   // 位置変更時の予備方向
	const Vector2* targetPosition_ = nullptr;
	BossState state_ = BossState::Idle;
	BossAttack nextAttack_ = BossAttack::Shot;
	BossAttack lastAttack_ = BossAttack::Shot;
	PressPhase pressPhase_ = PressPhase::Align;
	BossVisualEffect visual_ = {};
	Bullet bullets_[kMaxBullets] = {};

	int repairingScrew_ = -1;
	int flybyStyle_ = 0;
	int stage2SpecialMode_ = 0;
	int forcedFlybyStyle_ = -1;
	float flybyStartY_ = 0.0f;
	float flybyProgress_ = 0.0f;
	int flybySequence_ = 0;
	int stage2LastSlot_ = -1; // ステージ2の直前攻撃を記録
	bool stage2FacingLeft_ = true; // 移動が遅くなっても機首方向を維持
	int hp_ = kMaxHp;
	// 第2段階は一度きり。再生中の連打でHP半分以下になったら強制復活する。
	bool coasterStage_ = false;
	bool armorRebuilt_ = false;
	bool rebuildOnWake_ = false;
	int mashCount_ = 0;
	float armorFallVelocity_ = 0.0f;

	bool wakeBlastPending_ = false; // 1フレームのみtrue
	int armorPlates_ = BossStage1::kArmorPlates - 1;
	int armorStrainPlate_ = -1; // 張力がかかっている板
	float armorStrain_ = 0.0f;   // 0～1。既存BOXの角度だけ変える
	int armorDamage_ = 0;  // 通常攻撃による揺れ/ダメージ演出用（外装は剥がれない）
	bool armorAttached_[BossStage1::kArmorPlates] = { true, true, true, true };
	Vector2 peeledPosition_[BossStage1::kArmorPlates] = {};
	bool accessoryAttached_[kAccessoryGripCount - 1] = { true,true,true,true,true };
	Vector2 detachedAccessoryPosition_[kAccessoryGripCount - 1] = {};
	int detachedAccessoryFrames_[kAccessoryGripCount - 1] = {};
	int peeledFrames_[BossStage1::kArmorPlates] = {}; // 外装破壊時だけBOX一枚が飛ぶ
	int hitCooldown_ = 0;
	int hitstop_ = 0; // ボスだけ停止。
	int stateFrame_ = 0;
	int pressFrame_ = 0;
	int attackCount_ = 0;
	int defeatFrame_ = 0;
	int nextBulletIndex_ = 0;
	// 【AI選択】5つの既存攻撃だけで変化を作るための記憶。
	Vector2 targetVelocity_ = {};
	bool targetGrounded_ = true;
	bool targetUsingWire_ = false;
	int targetStillFrames_ = 0;
	BossAttack previousAttack_ = BossAttack::Press;
	unsigned int aiRandomState_ = 0x9E3779B9u;
	int attackStaleness_[5] = {}; // 長く使っていない攻撃を少し優先する
	// 【プレイヤー攻撃への反応】追加の描画は使わない。
	int normalComboHits_ = 0;
	int normalComboTimer_ = 0;
	int spinCounterCooldown_ = 0;
	bool wireUsedDuringRush_ = false;  // 突進中/予兆中にワイヤーで回避行動を取ったか
	bool rushHitPlayer_ = false;       // 突進がプレイヤーに当たったか
	bool rushDodgeStunPending_ = false;// 回避成功したら帰還後に失速する
	int rushDirection_ = -1;
	float rushVelocity_ = 0.0f;
	float rushTargetY_ = BossStage1::kRushGroundY; // 地上突進専用：高さ固定
	float rushTargetX_ = 0.0f; // 【突進の狙い】予兆終了時に固定。追尾し続けない
	float pressTargetX_ = 0.0f, pressFallSpeed_ = 0.0f;
	Vector2 pressReturnStart_ = {}, pressReturnDestination_ = {};

	// コマ・ヨーヨーを共通の物理オブジェクト一個で表現する。
	Vector2 toyPosition_ = {}, toyVelocity_ = {};
	int toyDirection_ = -1;
	float toyAimY_ = 150.0f; // ヨーヨーが狙う高さ（攻撃開始時に固定）
	bool toyHasHitPlayer_ = false;
	bool topLanded_ = false;
	int topWallHits_ = 0;
	int topElapsed_ = 0;


	void UpdateFlight();
	void UpdateFlightStage1();
	void UpdateFlightStage2(); // 予兆・攻撃以外の空中移動
	void ChangeState(BossState next);
	void BeginArmorRebuild();

	void ChangePressPhase(PressPhase next);
	void ChooseAttack();
	void ChooseAttackStage1();
	void ChooseAttackStage2();
	void UpdatePress();
	void UpdatePressStage1();
	void UpdatePressStage2();
	void UpdateRushStage1();
	void UpdateRushStage2();
	void UpdateStage2Special();
	void UpdateToy();
	void Shoot(int spreadIndex = 0);
	void UpdateBullets();
};
