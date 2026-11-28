#include "BossCloud.h"
#include <map>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace CloudRuntime {

	// 再入場しても同じ画像を繰り返し読み込まない。
	int CloudTexture(const char* path) {
		static std::map<std::string, int> handles;
		auto it = handles.find(path);
		if (it != handles.end()) return it->second;
		int handle = Novice::LoadTexture(path);
		if (handle >= 0) handles.emplace(path, handle);
		return handle;
	}







	// 触手1本


	// 1回のひっかけ(ゲート1つ + 触手1本)。0=L 1=R の2セットを使う


	// ひっかけ中の触手の通り道:ゲート →(まっすぐ)→ 杭の外側の接点 →(だんだん内側へ渦)→ 杭に巻き付いた先っぽ

	void BuildGripPath(const Vector2& gate, const Vector2& center, float spin, GripPath& gp) {
		GripPathMath::Build(gate, center, spin, gp, kGripRadius, kGripWireHalf, kGripTurns, kGripCoilGap, kPi);
	}
	Vector2 GripPathPoint(const GripPath& gp, float distance) {
		return GripPathMath::Point(gp, distance);
	}

	// 小さな道具

	float Lerp(float a, float b, float t) { return a + (b - a) * t; }

	float Clamp01(float t) {
		if (t < 0.0f) {
			return 0.0f;
		}
		if (t > 1.0f) {
			return 1.0f;
		}
		return t;
	}

	// イージング(0〜1を渡すと、動きになめらかな加速・減速がつく)
	float EaseIn(float t) {
		t = Clamp01(t);
		return t * t;
	}
	float EaseOut(float t) {
		t = Clamp01(t);
		return 1.0f - (1.0f - t) * (1.0f - t);
	}
	float EaseInOut(float t) {
		t = Clamp01(t);
		return t * t * (3.0f - 2.0f * t);
	}

	int ClampAlpha(float a) {
		if (a < 0.0f) {
			return 0;
		}
		if (a > 255.0f) {
			return 255;
		}
		return int(a);
	}

	// スプライト

	// 画像1枚分の情報(テクスチャハンドルと、元画像のピクセルサイズ)


	Sprite LoadSprite(const char* path, float w, float h) {
		Sprite s;
		s.handle = CloudTexture(path);
		s.w = w;
		s.h = h;
		return s;
	}

	// 画像を「中心(cx,cy)基準」で、画面上の大きさ w×h、不透明度 a(0〜255)で描く
	void DrawSpriteC(const Sprite& s, float cx, float cy, float w, float h, float a) {
		if (w < 1.0f || h < 1.0f) {
			return;
		}
		unsigned int color = 0xFFFFFF00 | (unsigned int)ClampAlpha(a);
		Novice::DrawSprite(int(cx - w / 2.0f), int(cy - h / 2.0f), s.handle, w / s.w, h / s.h, 0.0f, color);
	}

	// 目玉を、中心(cx,cy)・直径 d・不透明度 a で描く(絵が無い時は仮の丸)
	void DrawEyeC(const Sprite& s, float cx, float cy, float d, float a) {
		if (d < 1.0f) {
			return;
		}
		if (s.handle >= 0) {
			DrawSpriteC(s, cx, cy, d, d, a);
			return;
		}
		unsigned int alpha = static_cast<unsigned int>(ClampAlpha(a));
		Novice::DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(d * 0.5f), static_cast<int>(d * 0.5f), 0.0f, 0xFFFFFF00 | alpha, kFillModeSolid);
		Novice::DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(d * 0.2f), static_cast<int>(d * 0.2f), 0.0f, 0x00000000 | alpha, kFillModeSolid);
	}







	// 雲が四散した時のかけら


	// 雲のかたまり(雲は、これが何個か集まってできている)


	// 剥がれて飛んでいく雲のかたまり


	// 空いている弾を1つ使って発射
	void Fire(Bullet* b, int count, float x, float y, float angle, float speed) {
		for (int i = 0; i < count; i++) {
			if (!b[i].on) {
				b[i].on = true;
				b[i].x = x;
				b[i].y = y;
				b[i].vx = cosf(angle) * speed;
				b[i].vy = sinf(angle) * speed;
				return;
			}
		}
	}

	int CloudOptionalTexture(const char* path) {
		FILE* file = nullptr;
		if (fopen_s(&file, path, "rb") != 0 || !file) return -1;
		fclose(file);
		return CloudTexture(path);
	}

	CloudBoss::CloudBoss() {
		for (int i = 0; i < kCloudPartNum; i++) {
			parts[i].alive = true;
		}
	}

	void CloudBoss::Update() {

		// キー入力を受け取る
		memcpy(preKeys, keys, 256);
		Novice::GetHitKeyStateAll(keys);
		if (keys[DIK_F5] && !preKeys[DIK_F5]) showCloudEditor = !showCloudEditor;

		// コントローラー(つながっていない時は、スティックは倒れていない・ボタンは押していない扱い)
		padX = 0.0f;
		padY = 0.0f;
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

		// 操作(キーボードとコントローラーをまとめる)
		bool tentHold[2];
		tentHold[0] = (keys[DIK_Q] != 0 || keys[DIK_J] != 0 || Novice::IsPressButton(0, kPadTentL) || Novice::IsPressButton(0, kPadTentL2));
		tentHold[1] = (keys[DIK_E] != 0 || keys[DIK_K] != 0 || Novice::IsPressButton(0, kPadTentR) || Novice::IsPressButton(0, kPadTentR2));
		bool jumpHold = (keys[DIK_SPACE] != 0 || Novice::IsPressButton(0, kPadJump));
		bool releaseHold = (keys[DIK_LSHIFT] != 0 || keys[DIK_RSHIFT] != 0 || Novice::IsPressButton(0, kPadRelease));
		float inX = padX;   // 移動と狙い(左スティック / A・D・W・S)
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
		if (keys[DIK_TAB] != 0 && preKeys[DIK_TAB] == 0) {
			isPadDebug = !isPadDebug;
		}
		// 押した瞬間
		bool tentPress[2];
		for (int h = 0; h < 2; h++) {
			tentPress[h] = (tentHold[h] == true && preTentHold[h] == false);
		}
		bool jumpPress = (jumpHold == true && preJumpHold == false);
		bool releasePress = (releaseHold == true && preReleaseHold == false);

		///
		/// ↓更新処理ここから
		///

		frame++;

		// Fキーでボスの動きを最初からやり直す
		if (keys[DIK_F] != 0 && preKeys[DIK_F] == 0) {
			angle = kPi;
			waveAngle = 0.0f;
			centerY = kStartY;
			attack = 0;
			timer = 0;
			waterH = 0.0f;
			shock = false;
			frogsOn = false;
			stretchT = 999;
			sunR = 0.0f;
			// 主人公・杭・体力・攻撃のタイミングも最初から
			playerOn = false;
			windOn = false;
			windFade = 0.0f;
			jumpReady = true;
			memset(sets, 0, sizeof(sets));
			memset(pts, 0, sizeof(pts));
			gripCandCount = 0;
			isGripLock = false;
			aimArrowAlpha = 0.0f;
			lrRingAng[0] = 0.0f;
			lrRingAng[1] = 0.0f;
			lrRingFade[0] = 0.0f;
			lrRingFade[1] = 0.0f;
			knockX = 0.0f;
			knockTimer = 0;
			reelGrace[0] = 0;
			reelGrace[1] = 0;
			for (int i = 0; i < kStakeMax; i++) {
				stakeOn[i] = false;
				stakeHp[i] = kStakeHpMax;
				stakeShakeUntil[i] = 0;
			}
			for (int i = 0; i < kStakePeelMax; i++) {
				stakePeels[i].on = false;
			}
			stakeTimer = 0;
			bossHp = kBossHpMax;
			bossHitT = 0;
			bossChip = 0.0f;
			sunBroken = false;
			stakeOn[kSunSlot] = false;
			for (int i = 0; i < kCloudPartNum; i++) {
				parts[i].alive = true;
				peels[i].on = false;
			}
			isClear = false;
			clearT = 0;
			eyeVy = 0.0f;
			eyeT = 0;
			eyeScale = 1.0f;
			endT = 0;
			eyeGoneT = 0;
			flyStake = -1;
			restTimer = kRestFrames;
			lastAttack = 0;
			attackRunning = false;
		}

		// 目玉が逃げ始めたら、プレイヤーの真上(画面の上のほう)に杭が出て、プレイヤーは自動で触手を撃って杭に引っ張られ、そのまま画面の上へ飛んでいく(その間は操作できない)
		bool isFlyOut = (isClear == true && eyeGoneT > kFlyOutDelayFrames);
		if (isFlyOut == true) {
			inX = 0.0f;
			inY = 0.0f;
			tentHold[0] = false;
			tentHold[1] = false;
			tentPress[0] = false;
			tentPress[1] = false;
			jumpHold = false;
			jumpPress = false;
			releaseHold = false;
			releasePress = false;

			// 空いている杭の番号を1つ使って、プレイヤーの真上に杭を出す(この杭は下へスクロールしない)
			if (flyStake < 0) {
				int slot = 0;
				for (int i = 0; i < kStakeMax; i++) {
					if (stakeOn[i] == false) {
						slot = i;
						break;
					}
				}
				flyStake = slot;
				stakeOn[slot] = true;
				stakeHp[slot] = kStakeHpMax;
				stakeShakeUntil[slot] = 0;
				stakeX[slot] = player.pos.x;
				stakeY[slot] = kFlyStakeY;
			}

			// ロープとして残っている触手は外す
			for (int h = 0; h < 2; h++) {
				if (sets[h].isActive == true && sets[h].gripPhase == kGripHold) {
					sets[h].gripPhase = kGripUnwind;
				}
			}

			// 杭へ触手L(0番)を撃って、押し続けて引っ張られる。ひっかけ中でなく、杭より下にいて、上への勢いが弱まっていたら撃つ。
			// (画面の下まで落ちていて、1回で杭に届かなかった時も、画面の上の外へ出るまで、何回でも繰り返す。撃つたびに、杭はプレイヤーの真上へ移す)
			tentHold[0] = true;
			bool isGripBusy = (sets[0].isActive == true && sets[0].state == kSetOut && sets[0].gripPhase != kGripUnwind);
			if (isGripBusy == false && player.pos.y > kFlyStakeY + 10.0f && player.pos.y > -kPlayerHalfH && player.Velocity.y > -kFlyOutShootVy) {
				stakeX[flyStake] = player.pos.x;
				stakeY[flyStake] = kFlyStakeY;
				tentPress[0] = true;
			}
		}

		// 登る:ゴールに近づくほどゆっくりになる
		float speed = (centerY - kGoalY) * kSlowRate;
		if (speed > kClimbSpeed) {
			speed = kClimbSpeed;
		}
		if (speed < 0.2f) {
			speed = 0.2f;
		}
		// 倒したあとは、雲の動きを止める
		if (isClear == false) {
			centerY -= speed;
			if (centerY < kGoalY) {
				centerY = kGoalY;
			}

			// 回転と波は常に進めておく
			angle += 2.0f * kPi / kPeriod;
			waveAngle += 2.0f * kPi / kWavePeriod;
		}

		// ぐるぐる(奥行きあり)の位置と大きさ
		float depth = sinf(angle); // 手前=+1、奥=-1
		float orbitX = kCenterX + cosf(angle) * kRadiusX;
		float orbitY = centerY + depth * kRadiusY;
		float orbitSize = kBaseSize * (1.0f + depth * kScaleRange);

		// 横揺れ(波)の位置
		float waveX = kCenterX + sinf(waveAngle) * kWaveWidth;
		float waveY = kGoalY + sinf(waveAngle * kWaveCount) * kWaveHeight;

		// ゴールに近づくほど 0→1 になる混ぜ具合
		float w = 1.0f - (centerY - kGoalY) / kBlendZone;
		if (w < 0.0f) {
			w = 0.0f;
		}
		if (w > 1.0f) {
			w = 1.0f;
		}
		w = w * w * (3.0f - 2.0f * w);

		// ボスの通常の位置と大きさ(攻撃中は必要に応じて上書きする)
		float idleX = orbitX + (waveX - orbitX) * w;
		float idleY = orbitY + (waveY - orbitY) * w;
		float idleSize = orbitSize + (kBaseSize - orbitSize) * w;
		bossX = idleX;
		bossY = idleY;
		bossScale = idleSize / kBaseSize;
		bossAlpha = 255.0f;
		if (isClear == true) {
			bossX = clearX;
			bossY = clearY;
			bossScale = clearScale;
		}

		// 主人公が出てきていて、攻撃中でもないときは、休憩が終わったらランダムで攻撃開始(雲の動きは止めない)
		if (attack == 0 && playerOn == true && isClear == false) {
			if (restTimer > 0) {
				restTimer--;
			}
			else {
				int pick = 1 + rand() % 5;
				// 同じ攻撃の連続と、カエルが張り付いている間のカエル攻撃は選ばない
				while (pick == lastAttack || (pick == 4 && frogsOn == true)) {
					pick = 1 + rand() % 5;
				}
				attack = pick;
				timer = 0;
				lastAttack = pick;
				attackRunning = true;
			}
		}

		// ===== 1:雷(子雲が雲から出てくる→危険線→雷→雲に戻る) =====
		if (attack == 1) {
			if (timer == 0) {
				for (int i = 0; i < 3; i++) {
					thunderX[i] = 160.0f + i * 320.0f + float(rand() % 320);
				}
				stretchT = 0; // 子雲を出す
				stretchDir = 1.0f;
			}
			if (timer >= 140) {
				attack = 0;
			}
		}

		// ===== 2:奥へ→危険マーク→手前に突撃→上へ消える→奥からフェードで戻る =====
		else if (attack == 2) {
			if (timer < 45) {
				// 奥へ(小さくなる)。位置は雲のふわふわのまま
				bossScale = Lerp(1.0f, 0.35f, EaseInOut(timer / 45.0f));
			}
			else if (timer < 135) {
				bossScale = 0.35f;
				// 狙い位置はプレイヤーをなめらかに追い、105フレーム目で固定
				if (timer == 45) {
					tx = px;
					ty = py;
				}
				if (timer < 105) {
					tx += (px - tx) * 0.15f;
					ty += (py - ty) * 0.15f;
				}
			}
			else if (timer < 155) {
				// 手前へ突撃(今いる位置から、だんだん加速しながら大きく、薄くなる)
				if (timer == 135) {
					fromX = bossX;
					fromY = bossY;
				}
				float e = EaseIn((timer - 135) / 20.0f);
				bossScale = Lerp(0.35f, 2.2f, e);
				bossX = Lerp(fromX, tx, e);
				bossY = Lerp(fromY, ty, e);
				bossAlpha = Lerp(255.0f, 110.0f, (timer - 135) / 20.0f); // 位置はEaseInだが、透明度は直線で一定に薄くする
			}
			else if (timer < 205) {
				// ぶつかった瞬間も止めず、勢いのまま手前を突き抜けて上へ消える(止まらないように最初から速度を持たせる)
				float u = (timer - 155) / 50.0f;
				float q = u * 0.6f + u * u * 0.4f; // 初速あり+加速(EaseInだと初速0で止まって見える)
				bossScale = Lerp(2.2f, 4.0f, q);
				bossX = tx;
				bossY = Lerp(ty, -300.0f, q);
				bossAlpha = Lerp(110.0f, 0.0f, u); // そのまま消えるまで薄くする
			}
			else if (timer < 225) {
				// 見えない間
				bossAlpha = 0.0f;
			}
			else if (timer < 285) {
				// 奥から、小さく薄い状態でいつもの位置に現れ、フェードしながら手前に戻ってくる
				float u = EaseInOut((timer - 225) / 60.0f);
				bossScale = Lerp(0.3f, 1.0f, u);
				bossAlpha = 255.0f * u;
			}
			else {
				attack = 0;
			}
		}

		// ===== 3:雲から雨→水が溜まる→雷を1本→水が感電→水が引く(雲のふわふわは止めない) =====
		else if (attack == 3) {
			if (timer == 0) {
				stretchT = 0; // 雨を降らし始める
				stretchDir = 1.0f;
			}
			// 雷のときだけ、雲がプレイヤーの上へ横に寄る(高さは雲の動きのまま)
			float over = 0.0f;
			if (timer >= 240 && timer < 280) {
				over = EaseInOut((timer - 240) / 40.0f);
			}
			else if (timer >= 280 && timer < 345) {
				over = 1.0f;
			}
			else if (timer >= 345 && timer < 385) {
				over = 1.0f - EaseInOut((timer - 345) / 40.0f);
			}
			if (timer == 240) {
				lightX = px;
			}
			bossX = Lerp(idleX, lightX, over);

			if (timer < 240) {
				// 動いている雲から雨を降らし、水を溜める
				int spawn = 3;
				for (int i = 0; i < 200 && spawn > 0; i++) {
					if (!drops[i].on) {
						drops[i].on = true;
						drops[i].x = bossX - 80.0f + float(rand() % 160);
						drops[i].y = bossY + 50.0f;
						spawn--;
					}
				}
				waterH = kWaterMax * EaseInOut(timer / 240.0f);
			}
			else if (timer < 345) {
				// 危険線(280〜330)→雷(330〜345)。雷が落ちた瞬間に水が感電
				waterH = kWaterMax;
				if (timer == 330) {
					shock = true;
					stretchT = 0; // 雷を落とす
					stretchDir = 1.0f;
				}
			}
			else if (timer < 350) {
				// 感電したまましばらくそのまま
			}
			else if (timer < 500) {
				// 水が引いていく
				waterH -= kWaterMax / 80.0f;
				if (waterH < 0.0f) {
					waterH = 0.0f;
				}
			}
			else {
				waterH = 0.0f;
				shock = false;
				attack = 0;
			}
		}

		// 4:雲が下へ→カエルを4連続で上に吐く→雲が戻る→カエルが手前に跳んできて張り付く
		else if (attack == 4) {
			if (timer == 0) {
				for (int i = 0; i < kFrogNum; i++) {
					flying[i] = false;
				}
			}
			// 雲が下へ行き(いつもの位置からの割合 down)、少し待ってから戻る。横の動きはそのまま
			const float kDownY = 560.0f;
			float down = 0.0f;
			if (timer < 40) {
				down = EaseInOut(timer / 40.0f);
			}
			else if (timer < 105) {
				down = 1.0f;
			}
			else if (timer < 150) {
				down = 1.0f - EaseInOut((timer - 105) / 45.0f);
			}
			// 下へ行っても、雲の上下の波(idleYの揺れ)はそのまま乗せる
			bossY = idleY + down * (kDownY - kGoalY);

			// 1回で2匹ずつ、左右に角度を分けて上へ吐き出す。それを4回(12フレームおき)。回を重ねるほど、角度が広がる
			for (int i = 0; i < kFrogNum; i++) {
				if (timer == 45 + (i / kFrogPerVolley) * kFrogVolleyInterval) {
					flying[i] = true;
					stretchT = 0; // 吐くたびに、上へにゅっと伸びる
					stretchDir = -1.0f;
					fx[i] = bossX;
					fy[i] = bossY - 30.0f;
					float spitVx = 3.0f + (i / kFrogPerVolley) * 1.5f;   // 何回目の射出かで、横への速さの最大を変える(1回目は小さく、最後は大きく)
					// 1回の射出の中での順番(0〜kFrogPerVolley-1)に応じて、一番左(-1.0)から一番右(+1.0)まで等間隔に横の向きを割り振
					float spread = 0.0f;
					if (kFrogPerVolley > 1) {
						spread = (i % kFrogPerVolley) * 2.0f / static_cast<float>(kFrogPerVolley > 1 ? kFrogPerVolley - 1 : 1) - 1.0f;   // 1匹の時でも、0で割る式にならないようにしてある
					}
					else {
						spread = ((i / kFrogPerVolley) % 2 == 0) ? -1.0f : 1.0f;   // 1匹ずつの時は、射出ごとに左・右・左…と交互に
					}
					fvx[i] = spitVx * spread;
				}
				if (flying[i]) {
					fx[i] += fvx[i];
					fy[i] -= kFrogShotRiseSpeed;
					if (fy[i] < -80.0f) {
						flying[i] = false; // 画面の上へ出ていった
					}
				}
			}

			// 雲が戻ったら、張り付くカエルの位置を決めて跳ばせ始める(以降は攻撃が終わっても続く)
			if (timer == 150) {
				// まばらに:画面を 列×行 のマスに分けて、1匹ずつ別のマスに置く(マスの中の位置はランダム)。誰がどのマスかは、毎回シャッフルする
				int cellOrder[kFrogCols * kFrogRows];
				for (int i = 0; i < kFrogCols * kFrogRows; i++) {
					cellOrder[i] = i;
				}
				for (int i = kFrogCols * kFrogRows - 1; i > 0; i--) {
					int j = rand() % (i + 1);
					int tmp = cellOrder[i];
					cellOrder[i] = cellOrder[j];
					cellOrder[j] = tmp;
				}
				int cellW = 980 / kFrogCols;   // 置ける範囲(横150〜1130・縦150〜570)を、マスに分けた大きさ
				int cellH = 420 / kFrogRows;
				for (int i = 0; i < kFrogNum; i++) {
					frogs[i].rx = kFrogStuckRadiusMin + float(rand() % static_cast<int>(kFrogStuckRadiusRand));   // 大きさ
					frogs[i].ry = frogs[i].rx * 0.85f;
					frogs[i].x = 150.0f + float((cellOrder[i] % kFrogCols) * cellW + rand() % cellW);
					frogs[i].y = 150.0f + float((cellOrder[i] / kFrogCols) * cellH + rand() % cellH);
				}
				frogsOn = true;
				frogT = 0;
			}
			// カエルが全員着地したら、この攻撃は終わり(カエルは張り付いたまま次の攻撃へ)
			if (timer >= 150 + (kFrogNum - 1) * kFrogStagger + 20) {
				attack = 0;
			}
		}

		// 5:太陽(育ってから、全方向に火の玉)
		else if (attack == 5) {
			if (timer == 0) {
				stretchT = 0; // 太陽を出す
				stretchDir = 1.0f;
				stakeHp[kSunSlot] = kSunHpMax;
				stakeShakeUntil[kSunSlot] = 0;
				sunBroken = false;
			}
			if (sunBroken == true) {
				// 掴まれて壊された・引きはがされた:縮んで消える(攻撃はキャンセル)
				sunR -= kSunBreakShrink;
				if (sunR <= 0.0f) {
					sunR = 0.0f;
					attack = 0;
				}
			}
			else {
				if (timer < 60) {
					sunR = 80.0f * timer / 60.0f;
				}
				else if (timer < 220) {
					sunR = 80.0f;
				}
				else {
					sunR = 80.0f * (1.0f - (timer - 220) / 30.0f);
				}
				if (timer >= 250) {
					sunR = 0.0f;
					attack = 0;
				}
			}
			// 太陽は、ある程度育っている間だけ、杭の1つとして掴める(壊れた・縮んだ・攻撃が終わったら掴めない)
			stakeX[kSunSlot] = kSunX;
			stakeY[kSunSlot] = kSunY;
			if (attack == 5 && sunBroken == false && sunR >= kSunGripMinR) {
				stakeOn[kSunSlot] = true;
			}
			else {
				stakeOn[kSunSlot] = false;
			}
		}

		// 攻撃が終わったら休憩(4と5のあとは休憩なしで、すぐ次の攻撃)
		if (attack == 0 && attackRunning == true) {
			attackRunning = false;
			restTimer = 0;
			if ((lastAttack != 4 && lastAttack != 5) || sunBroken == true) {
				restTimer = kRestFrames;   // 太陽をキャンセルした時は、休憩が入る
			}
			sunBroken = false;
		}

		// 掴めなくなった太陽に掛かっている触手は、もう追いかけない(解除される)
		if (stakeOn[kSunSlot] == false) {
			for (int s = 0; s < 2; s++) {
				if (sets[s].isActive == true && sets[s].gripTarget == kSunSlot) {
					sets[s].gripTarget = -2;
				}
			}
		}

		// 雲の伸びるアニメーション(にゅっと伸びて、少し縮んで、戻る)
		if (stretchT < 1000) {
			stretchT++;
		}
		stretchSX = 1.0f;
		stretchSY = 1.0f;
		if (stretchT < 20) {
			float p = stretchT / 20.0f;
			float amount = sinf(p * 2.0f * kPi) * (1.0f - p); // 伸び→縮み→0 へ減衰
			stretchSX = 1.0f - 0.3f * amount;                 // 縦に伸びると横は細くなる
			stretchSY = 1.0f + 0.5f * amount;
		}

		// 風の演出の濃さ:吹いている間はすぐ濃く、止んだらゆっくり薄れる
		if (windOn == true) {
			windFade += 0.15f;
			if (windFade > 1.0f) {
				windFade = 1.0f;
			}
		}
		else if (windFade > 0.0f) {
			windFade -= 0.03f;
			if (windFade < 0.0f) {
				windFade = 0.0f;
			}
		}

		// 張り付きカエルは攻撃が終わっても進める(最後のカエルが剥がれたら消える)
		if (frogsOn) {
			frogT++;
			if (frogT >= kFrogEndFrames) {
				frogsOn = false;
			}
		}

		// 雨と火の玉は攻撃が終わっても動かし続ける
		for (int i = 0; i < 200; i++) {
			if (drops[i].on) {
				drops[i].y += 20.0f;
				if (drops[i].y > 720.0f) {
					drops[i].on = false;
				}
			}
		}
		for (int i = 0; i < 100; i++) {
			if (bullets[i].on) {
				bullets[i].x += bullets[i].vx;
				bullets[i].y += bullets[i].vy;
				if (bullets[i].x < -50.0f || bullets[i].x > 1330.0f || bullets[i].y < -50.0f || bullets[i].y > 770.0f) {
					bullets[i].on = false;
				}
			}
		}

		// 剥がれた雲のかけらは、飛んでいって消える
		for (int i = 0; i < kCloudPartNum; i++) {
			if (peels[i].on == true) {
				peels[i].x += peels[i].vx;
				peels[i].y += peels[i].vy;
				peels[i].vx *= 0.97f;
				peels[i].vy += 0.6f;
				peels[i].t++;
				if (peels[i].t >= kPeelPieceFrames) {
					peels[i].on = false;
				}
			}
		}

		// 抜けた杭も、飛んでいって消える
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

		// ---------- 杭(上から落ちてきて、下へスクロール。雲が昇っている最中から出ている) ----------
		stakeTimer++;
		if (stakeTimer >= kStakeInterval) {
			stakeTimer = 0;
			for (int i = 0; i < kStakeMax; i++) {
				if (stakeOn[i] == false) {
					stakeOn[i] = true;
					stakeHp[i] = kStakeHpMax;
					stakeShakeUntil[i] = 0;
					stakeX[i] = 80.0f + float(rand() % 1121);
					stakeY[i] = -30.0f;
					break;
				}
			}
		}
		for (int i = 0; i < kStakeMax; i++) {
			if (stakeOn[i] == false) {
				continue;
			}
			if (i == flyStake) {
				continue;   // プレイヤーを引っ張り上げる杭は、その場から動かない
			}
			stakeY[i] += kStakeSpeed;
			if (stakeY[i] > kWindowHeight + 30.0f) {
				stakeOn[i] = false;
				// この杭にひっかけていた触手は、もう追いかけない
				for (int s = 0; s < 2; s++) {
					if (sets[s].isActive == true && sets[s].gripTarget == i) {
						sets[s].gripTarget = -2;
					}
				}
			}
		}

		// ---------- 主人公(雲が上に着いたら、下から出てくる) ----------
		if (playerOn == false && isClear == false && centerY <= kGoalY + 50.0f) {
			playerOn = true;
			player.pos.x = kCenterX;
			player.pos.y = 790.0f;
			player.Velocity.x = 0.0f;
			player.Velocity.y = -kSpawnVy;   // 下から勢いよく飛び出してくる
			player.isFacingRight = true;
			jumpReady = true;
			restTimer = kRestFrames;
		}

		if (playerOn == true) {
			if (bossHitT > 0) {
				bossHitT--;
			}

			// ひっかけ先(雲 / 杭)の動きに、触手の巻き付き先を合わせる
			for (int s = 0; s < 2; s++) {
				if (sets[s].isActive == false) {
					continue;
				}
				if (sets[s].gripTarget == -1) {
					sets[s].gripPos.x = bossX;
					sets[s].gripPos.y = bossY;
				}
				else if (sets[s].gripTarget >= 0) {
					sets[s].gripPos.x = stakeX[sets[s].gripTarget];
					sets[s].gripPos.y = stakeY[sets[s].gripTarget];
				}
			}

			// 雲にひっかけられるのは、はっきり見えていて、ふつうの大きさの時だけ(突撃で奥へ行っている間などはだめ)
			bool cloudGrippable = (bossAlpha >= 200.0f && bossScale >= 0.9f && isClear == false);

			// ---------- 触手の道具 ----------
			// 触手を解除する(引っ張っている最中なら、その向きへ飛ぶ)
			auto unwindSet = [&](int h) {
				TentacleSet& us = sets[h];
				if (us.isActive == false || us.state != kSetOut || us.gripPhase == kGripUnwind) {
					return;
				}
				if (us.gripPhase == kGripPull) {
					player.Velocity.y = us.pullDir.y * kGripLaunchSpeed;
					knockX = us.pullDir.x * kGripLaunchSpeed;
					knockTimer = kKnockHoldFrames;
					jumpReady = true;
				}
				us.gripPhase = kGripUnwind;
				};

			// 雲を1つ剥がす(プレイヤーのいる側にある、残っているかたまりを選ぶ)
			auto peelCloud = [&]() {
				float toPlayerX = player.pos.x - bossX;
				float toPlayerY = player.pos.y - bossY;
				float toPlayerLen = sqrtf(toPlayerX * toPlayerX + toPlayerY * toPlayerY);
				if (toPlayerLen < 1.0f) {
					toPlayerLen = 1.0f;
				}
				toPlayerX /= toPlayerLen;
				toPlayerY /= toPlayerLen;
				int best = -1;
				float bestDot = -1.0e9f;
				for (int i = 0; i < kCloudPartNum; i++) {
					if (parts[i].alive == false) {
						continue;
					}
					float dotValue = kPartDef[i][0] * toPlayerX + kPartDef[i][1] * toPlayerY;
					if (dotValue > bestDot) {
						bestDot = dotValue;
						best = i;
					}
				}
				if (best < 0 || bossHp <= 0) {
					return;   // 剥がすものがない(巻きで削れた分と引きはがしが同じフレームに重なっても、体力はマイナスにならない)
				}
				parts[best].alive = false;
				peels[best].on = true;
				peels[best].t = 0;
				peels[best].x = bossX + kPartDef[best][0] * bossScale;
				peels[best].y = bossY + kPartDef[best][1] * bossScale;
				peels[best].vx = toPlayerX * kPeelPieceSpeed;
				peels[best].vy = toPlayerY * kPeelPieceSpeed - 3.0f;
				peels[best].s = kPartDef[best][2] * bossScale;
				bossHp--;
				bossHitT = kBossHitFrames;
				};

			// ---------- プレイヤーの移動(左スティック / A・D) ----------
			player.Velocity.x = inX * kPlayerSpeed;
			if (inX > 0.1f) {
				player.isFacingRight = true;
			}
			else if (inX < -0.1f) {
				player.isFacingRight = false;
			}

			// 横への反動(触手を離した時などの勢い。飛んでいる間は、逆向きの入力の効きが弱くなり、反動はだんだん減る)
			if (knockX != 0.0f) {
				float inputX = player.Velocity.x;
				if (inputX * knockX < 0.0f) {
					float strength = fabsf(knockX) / kGripLaunchSpeed;   // 離した直後は1.0、反動が終わる頃には0.0
					if (strength > 1.0f) {
						strength = 1.0f;
					}
					inputX *= 1.0f - strength * (1.0f - kKnockInputRate);
				}
				player.Velocity.x = inputX + knockX;

				// 最初の kKnockHoldFrames フレームは勢いを保って、そのあとから減らしていく
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

			// この世界に地面は無い(下に落ちたら、風が吹いて画面へ戻される)
			bool isGround = false;

			// ジャンプ(Xボタン / SPACE)
			if (jumpPress == true && jumpReady == true) {
				player.Velocity.y = -kJumpPower;
				jumpReady = false;
			}
			if (jumpHold == false && preJumpHold == true) {
				if (player.Velocity.y < -kJumpCut) {
					player.Velocity.y = -kJumpCut;
				}
			}

			// 狙う向き(左スティック / WASD)。何も倒していない時は、向いている向きを狙う
			float aimLen = sqrtf(inX * inX + inY * inY);
			bool hasAimDir = (aimLen > 0.01f);
			float aimDx = 0.0f;
			float aimDy = 0.0f;
			if (hasAimDir == true) {
				aimDx = inX / aimLen;
				aimDy = inY / aimLen;
			}

			// 狙う先の探し方:雲が届く範囲にあるなら、雲が最優先。
			//  雲が狙えない時は、届く範囲の杭のうち、狙う向きの左右60度以内で、近くて向きが合っているものを狙う。
			//  excludeTarget(-1:雲 / 0以上:杭の番号)は狙わない(-3 なら除外なし)。cand には、届く範囲にあるものを全部入れる
			auto findLock = [&](int excludeTarget, int* cand, int& candCount, int& lockIdx) -> bool {
				candCount = 0;
				lockIdx = 0;
				bool isLock = false;
				float adx = aimDx;
				float ady = aimDy;
				float lockCos = kGripLockCos;
				if (hasAimDir == false) {
					// 何も倒していない時:向いている向きを基準に、どの向きの的でも狙える(近くて向きが合っているものが優先)
					adx = player.isFacingRight ? 1.0f : -1.0f;
					ady = 0.0f;
					lockCos = -1.1f;
					// もう片方の触手が何かを掴んでいるなら、それを引き剥がせる向き(掴んでいるものの反対側)を基準にする
					if (excludeTarget >= -1) {
						float heldX = bossX;
						float heldY = bossY;
						if (excludeTarget >= 0) {
							heldX = stakeX[excludeTarget];
							heldY = stakeY[excludeTarget];
						}
						float awayX = player.pos.x - heldX;
						float awayY = player.pos.y - heldY;
						float awayLen = sqrtf(awayX * awayX + awayY * awayY);
						if (awayLen > 1.0f) {
							adx = awayX / awayLen;
							ady = awayY / awayLen;
						}
					}
				}

				// 雲
				bool isCloudFarAim = false;
				if (cloudGrippable == true && excludeTarget != -1) {
					float cvx = bossX - player.pos.x;
					float cvy = bossY - player.pos.y;
					float cdist = sqrtf(cvx * cvx + cvy * cvy);
					if (cdist <= kCloudLockRange && cdist >= 40.0f) {
						cand[candCount] = -1;
						candCount++;
						float cosC = (cvx * adx + cvy * ady) / cdist;
						if (cosC >= lockCos) {
							if ((cdist <= kCloudPriorityRange && stakeOn[kSunSlot] == false) || excludeTarget >= 0) {
								// 雲にある程度近づいた(またはもう片方が杭を掛けている):雲を最優先
								isLock = true;
								lockIdx = -1;
							}
							else {
								// まだ遠い:杭が狙えなかった時だけ雲を狙う
								isCloudFarAim = true;
							}
						}
					}
				}

				// 杭
				// もう片方が雲を掛けている時は、雲から遠ざかる向きへ引っ張れる杭ほど優先(引っ張ると雲が剥がれる)
				float peelAx = 0.0f;
				float peelAy = 0.0f;
				if (excludeTarget == -1) {
					float peelVx = player.pos.x - bossX;
					float peelVy = player.pos.y - bossY;
					float peelVl = sqrtf(peelVx * peelVx + peelVy * peelVy);
					if (peelVl > 1.0f) {
						peelAx = peelVx / peelVl;
						peelAy = peelVy / peelVl;
					}
				}
				float bestScore = 1.0e9f;
				for (int i = 0; i <= kStakeMax; i++) {   // kStakeMax は太陽
					if (stakeOn[i] == false || stakeY[i] < 0.0f || stakeY[i] > kWindowHeight || i == excludeTarget) {
						continue;
					}
					float svx = stakeX[i] - player.pos.x;
					float svy = stakeY[i] - player.pos.y;
					float sdist = sqrtf(svx * svx + svy * svy);
					if (sdist > kGripLockRange || sdist < 40.0f) {
						continue;
					}
					if (candCount < kMaxGripCand) {
						cand[candCount] = i;
						candCount++;
					}
					if (isLock == false || lockIdx >= 0) {
						float cosS = (svx * adx + svy * ady) / sdist;
						if (cosS >= lockCos) {
							// 近いほど、向きが合っているほど優先
							float score = sdist + (1.0f - cosS) * 400.0f;
							if (excludeTarget == -1) {
								float cosAway = (svx * peelAx + svy * peelAy) / sdist;
								score += (1.0f - cosAway) * 300.0f;
							}
							if (score < bestScore) {
								bestScore = score;
								isLock = true;
								lockIdx = i;
							}
						}
					}
				}
				if (isLock == false && isCloudFarAim == true) {
					isLock = true;
					lockIdx = -1;
				}
				return isLock;
				};

			// 触手が出ているか(伸びている・ロープ・引っ張り中。しまっている最中は含まない)
			bool isOut[2];
			for (int h = 0; h < 2; h++) {
				isOut[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind);
			}

			// 狙いのマーク:まだ出していない触手があれば、その触手が狙う先を出す(もう片方が掴んでいるものは狙わない)
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
					int markExclude = -3;
					if (isOut[markOther] == true && sets[markOther].gripTarget >= -1) {
						markExclude = sets[markOther].gripTarget;
					}
					isGripLock = findLock(markExclude, gripCandIdx, gripCandCount, gripLockIdx);
				}
			}

			// 飛び去る時は、距離や向きに関係なく、出した杭だけを狙う
			if (isFlyOut == true && flyStake >= 0 && stakeOn[flyStake] == true) {
				gripCandCount = 0;   // ターゲットマークは出さない
				isGripLock = true;
				gripLockIdx = flyStake;
			}

			// 入力方向の矢印:入力した向きへ、プレイヤーのまわりをぐるっと回りこむ(角度の近いほうまわり)
			if (hasAimDir == true) {
				float targetAng = atan2f(inY, inX);
				if (aimArrowAlpha <= 0.01f) {
					aimArrowAng = targetAng;   // 出始めは、その向きにいきなり出る
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

			// L/R の矢印:触手がひっかけポイントに着いている間(ロープ・引っ張り中)だけ出て、回る
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

			// ---------- 触手の発射・解除 ----------
			// Aボタン(LSHIFT):出ている触手を全部解除
			if (releasePress == true) {
				for (int h = 0; h < 2; h++) {
					unwindSet(h);
				}
			}

			// L2 / R2 (Q・E):1回目で射出して掴む(押し続けていると引っ張る)。ロープ状態でもう一度押すと巻き取り(解除は LSHIFT / A)
			for (int h = 0; h < 2; h++) {
				if (tentPress[h] == false) {
					continue;
				}
				if (isOut[h] == true) {
					if (sets[h].gripPhase == kGripHold) {
						// ロープでぶら下がっている時にもう一度押したら、離さずに巻き取る(押している間だけ。離すとロープに戻る)
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
						unwindSet(h);
						isOut[h] = false;
					}
					continue;
				}

				// もう片方が掴んでいるもの(雲なら雲、杭ならその杭)は狙わない。雲と遠くの杭に1本ずつ掛けられるように
				int other = 1 - h;
				int exclude = -3;
				if (isOut[other] == true && sets[other].gripTarget >= -1) {
					exclude = sets[other].gripTarget;
				}
				int shotCand[kMaxGripCand];
				int shotCandCount = 0;
				int shotTarget = 0;
				bool isShotLock = findLock(exclude, shotCand, shotCandCount, shotTarget);
				if (isFlyOut == true && flyStake >= 0 && stakeOn[flyStake] == true) {
					isShotLock = true;
					shotTarget = flyStake;
				}
				if (isShotLock == false) {
					continue;
				}

				// ゲートは巻き付き先の反対側。真ん中の触手が巻き付き先へ伸びて巻き付き、両脇の触手は巻き付き先の向きへ少し伸びて引っ込む
				TentacleSet& ts = sets[h];
				memset(&ts, 0, sizeof(ts));
				ts.isActive = true;
				ts.state = kSetOut;
				ts.timer = 0;
				ts.gripPhase = kGripReach;
				ts.gripTarget = shotTarget;
				if (shotTarget < 0) {
					ts.gripPos.x = bossX;
					ts.gripPos.y = bossY;
				}
				else {
					ts.gripPos.x = stakeX[shotTarget];
					ts.gripPos.y = stakeY[shotTarget];
				}
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
			player.Velocity.y += kGravity;
			if (player.Velocity.y > kMaxFallSpeed) {
				player.Velocity.y = kMaxFallSpeed;
			}

			// 引っ張っている触手がある間:重力もキー入力も無視して、掴んだ先へ一直線(2本とも引いているなら、向きを平均する)
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
			// 触手ごとに、巻いている間と、そのあとの勢いで飛んでいる間を数える(もう片方の引きはがし判定用)
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
				player.Velocity.x = pullSumX * kGripPullSpeed;
				player.Velocity.y = pullSumY * kGripPullSpeed;
				knockX = 0.0f;
				knockTimer = 0;
				if (pullSumX > 0.1f) {
					player.isFacingRight = true;
				}
				else if (pullSumX < -0.1f) {
					player.isFacingRight = false;
				}
			}

			// 下に落ちて、画面の外へ出てさらにある程度(kWindDelay)下がったら、風が吹いて画面へ吹き上げる(引っ張り中は吹かない)
			if (pullCount == 0 && windOn == false && player.pos.y > kWindowHeight + kPlayerHalfH + kWindDelay) {
				windOn = true;
				knockX = 0.0f;
				knockTimer = 0;
				jumpReady = true;
				unwindSet(0);
				unwindSet(1);
			}
			if (windOn == true) {
				player.Velocity.y = -kWindPower;   // 画面へ戻るまで、ずっと吹き上げる
				player.pos.x += (kWindowWidth / 2.0f - player.pos.x) * kWindDrift;   // 風で画面の真ん中へ寄る
				if (player.pos.y <= kWindEndY) {
					windOn = false;
					player.Velocity.y = -kWindPower * 0.4f;   // 風が止む時は、すこしだけ勢いを残す
				}
			}

			// 飛び去っている間:風は吹かない。画面の上の外まで行ったら、そこで止まる
			if (isFlyOut == true) {
				windOn = false;
				if (player.pos.y <= kFlyOutStopY) {
					player.pos.y = kFlyOutStopY;
					player.Velocity.y = 0.0f;
				}
			}

			// 移動(画面の左右と上は、はみ出さない。下は落ちられる)
			player.pos.x += player.Velocity.x;
			player.pos.y += player.Velocity.y;
			if (player.pos.x < kPlayerHalfW) {
				player.pos.x = kPlayerHalfW;
			}
			if (player.pos.x > kWindowWidth - kPlayerHalfW) {
				player.pos.x = kWindowWidth - kPlayerHalfW;
			}

			// ロープ:触手を伸ばして掴んだままの間は、その長さより遠くへ離れられない(振り子のように揺れる)。
			// もう片方の触手が巻いている間(と、巻いた直後の猶予の間)にロープが限界まで伸びたら、引きはがし判定:
			//  2つの掴んでいる先が kPeelNeedLen 以上離れていれば、掴んでいたほうが引きはがされる(杭は抜ける・雲は1つ剥がれる)。足りなければ、触手が外れるだけ
			for (int h = 0; h < 2; h++) {
				TentacleSet& rs = sets[h];
				if (rs.isActive == false) {
					continue;
				}
				float ropeDx = player.pos.x - rs.gripPos.x;
				float ropeDy = player.pos.y - rs.gripPos.y;
				float ropeDist = sqrtf(ropeDx * ropeDx + ropeDy * ropeDy);
				if (rs.gripPhase == kGripPull) {
					// 引っ張られて近づく間は、ロープも縮む
					if (ropeDist < rs.ropeLen) {
						rs.ropeLen = ropeDist;
					}
					continue;
				}
				if (rs.gripPhase != kGripHold || ropeDist <= rs.ropeLen || ropeDist < 1.0f) {
					continue;
				}

				// 引きはがし判定は、もう片方が巻いている時だけ(自分で巻いて近づいただけでは判定しない)。飛び去る間はしない
				int otherIdx = 1 - h;
				bool isYank = false;
				if (rs.gripTarget >= -1 && isFlyOut == false) {
					if ((sets[otherIdx].isActive == true && sets[otherIdx].gripPhase == kGripPull) || reelGrace[otherIdx] > 0) {
						isYank = true;
					}
				}
				if (isYank == true) {
					float peelGapX = rs.gripPos.x - reelPos[otherIdx].x;
					float peelGapY = rs.gripPos.y - reelPos[otherIdx].y;
					if (sqrtf(peelGapX * peelGapX + peelGapY * peelGapY) >= kPeelNeedLen) {
						if (rs.gripTarget == -1) {
							// 雲を1つ剥がす(巻きで削れている途中でも剥がれる。削れた分はそのまま残る)
							peelCloud();
							if (bossHp <= 0) {
								bossChip = 0.0f;
							}
						}
						else if (rs.gripTarget == kSunSlot) {
							// 太陽を引きはがす:太陽は縮んで消えて、攻撃はキャンセル
							sunBroken = true;
							stakeOn[rs.gripTarget] = false;
							rs.gripTarget = -2;
						}
						else {
							// 杭を引き抜く:その場から、引っ張られた向きへ飛んでいって消える
							stakePeels[stakePeelNext].on = true;
							stakePeels[stakePeelNext].t = 0;
							stakePeels[stakePeelNext].x = rs.gripPos.x;
							stakePeels[stakePeelNext].y = rs.gripPos.y;
							stakePeels[stakePeelNext].vx = ropeDx / ropeDist * kPeelPieceSpeed;
							stakePeels[stakePeelNext].vy = ropeDy / ropeDist * kPeelPieceSpeed - 3.0f;
							stakePeels[stakePeelNext].s = 1.0f;
							stakePeelNext = (stakePeelNext + 1) % kStakePeelMax;
							stakeOn[rs.gripTarget] = false;
							rs.gripTarget = -2;
						}
					}
					else if (rs.gripTarget >= 0) {
						// 距離が足りない:杭は震えるだけ
						stakeShakeUntil[rs.gripTarget] = frame + kStakeShakeFrames;
					}
					rs.gripPhase = kGripUnwind;   // 引きはがした・外れた触手はしまわれる
					continue;
				}

				// ふつうのロープ:長さのところまで引き戻して、外へ向かう勢いを消す
				float ropeNx = ropeDx / ropeDist;
				float ropeNy = ropeDy / ropeDist;
				player.pos.x = rs.gripPos.x + ropeNx * rs.ropeLen;
				player.pos.y = rs.gripPos.y + ropeNy * rs.ropeLen;
				float outSpeed = player.Velocity.x * ropeNx + player.Velocity.y * ropeNy;
				if (outSpeed > 0.0f) {
					player.Velocity.y -= ropeNy * outSpeed;
					if (knockX * ropeNx > 0.0f) {
						knockX = 0.0f;
					}
				}
			}
			if (player.pos.x < kPlayerHalfW) {
				player.pos.x = kPlayerHalfW;
			}
			if (player.pos.x > kWindowWidth - kPlayerHalfW) {
				player.pos.x = kWindowWidth - kPlayerHalfW;
			}

			// ---------- プレイヤーのアニメーション ----------
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


			// ---------- 触手とゲートの更新 ----------
			for (int s = 0; s < 2; s++) {
				TentacleSet& ts = sets[s];
				if (ts.isActive == false) {
					continue;
				}

				ts.timer++;

				// ゲートは、プレイヤーにすこし遅れてついてくる
				{
					float followX = player.pos.x + ts.gateOffset.x;
					float followY = player.pos.y + ts.gateOffset.y;
					// マップの外に出ないように制限
					if (followX < 16.0f) { followX = 16.0f; }
					if (followX > kWindowWidth - 16.0f) { followX = kWindowWidth - 16.0f; }
					if (followY < 16.0f) { followY = 16.0f; }
					if (followY > kWindowHeight - 16.0f) { followY = kWindowHeight - 16.0f; }
					ts.gatePos.x += (followX - ts.gatePos.x) * kGateFollow;
					ts.gatePos.y += (followY - ts.gatePos.y) * kGateFollow;
				}

				if (ts.state == kSetOut) {
					// ゲートの開き具合
					ts.gateOpen = static_cast<float>(ts.timer) / kGateOpenFrames;
					if (ts.gateOpen > 1.0f) {
						ts.gateOpen = 1.0f;
					}

					// 巻き付き先が無くなった(杭が下へ消えた / 雲がひっかけられない状態になった)ら、解除
					if (ts.gripPhase != kGripUnwind && (ts.gripTarget == -2 || (ts.gripTarget == -1 && cloudGrippable == false))) {
						unwindSet(s);
					}

					// ゲートの口は、いつも巻き付き先のほうを向く(2つのゲートは、左右にずらす)
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

					Tentacle& gt = ts.t;   // 杭・雲に巻き付く触手
					if (ts.gripPhase == kGripReach) {
						// 巻き付き先の接点まで、まっすぐ伸びる
						gt.len += kGripReachSpeed;
						if (gt.len >= gp.legLen) {
							gt.len = gp.legLen;
							// 届いた:ボタンを押し続けていれば引っ張る。離していれば、ロープとしてつながる
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
						// ロープ:巻き付いたまま。ロープが伸びきっている間は、ぴんと張る
						gt.len = gp.total;
						float holdDx = player.pos.x - ts.gripPos.x;
						float holdDy = player.pos.y - ts.gripPos.y;
						float holdDist = sqrtf(holdDx * holdDx + holdDy * holdDy);
						float tenseTarget = (holdDist >= ts.ropeLen - 3.0f) ? 1.0f : 0.0f;
						ts.gripTense += (tenseTarget - ts.gripTense) * 0.3f;
					}
					else if (ts.gripPhase == kGripPull) {
						// 引っ張り中:ボタンを離したら、勢いを残してロープに戻る。掴んだ先に近づいたら離して飛ぶ(雲ならはね返る)
						ts.gripTimer++;
						float pullDx = ts.gripPos.x - player.pos.x;
						float pullDy = ts.gripPos.y - player.pos.y;
						float pullDist = sqrtf(pullDx * pullDx + pullDy * pullDy);

						// 巻きのダメージ:巻いた割合の2乗 × 距離(杭と太陽。雲は下の kCloudReelChip で少しだけ削れる。飛び去る時の杭は壊れない)
						// 杭の手前まで巻き切る長押しを kFallStakeFullReels 回やると、耐久度がちょうど0
						bool isDropped = false;
						if (ts.gripTarget >= 0 && ts.gripTarget != flyStake) {
							float reelSpan = ts.pullStartDist - kGripReleaseDist;
							if (reelSpan > 1.0f) {
								float reelProg = (ts.pullStartDist - pullDist) / reelSpan;
								if (reelProg > 1.0f) {
									reelProg = 1.0f;
								}
								if (reelProg > ts.pullProg) {
									// 落ちてくる杭は距離に関係なく、巻き切る(reelProg が1.0になる)と、ちょうど耐久度が0になる
									float reelDmg = kStakeHpMax / kFallStakeFullReels * (reelProg * reelProg - ts.pullProg * ts.pullProg);
									ts.pullProg = reelProg;
									stakeHp[ts.gripTarget] -= reelDmg;
									if (reelDmg > 0.05f) {
										stakeShakeUntil[ts.gripTarget] = frame + kStakeShakeFrames;
									}
									if (stakeHp[ts.gripTarget] <= 0.5f) {   // 計算誤差で、少し残らないように
										// 杭が外れる:少し飛びながら落ちる。勢いは残して、触手をしまう
										if (ts.gripTarget == kSunSlot) {
											// 太陽は、耐久が0になると縮んで消える(攻撃はキャンセル)
											sunBroken = true;
										}
										else {
											stakePeels[stakePeelNext].on = true;
											stakePeels[stakePeelNext].t = 0;
											stakePeels[stakePeelNext].x = ts.gripPos.x;
											stakePeels[stakePeelNext].y = ts.gripPos.y;
											stakePeels[stakePeelNext].vx = static_cast<float>(rand() % 5 - 2);
											stakePeels[stakePeelNext].vy = -5.0f;
											stakePeels[stakePeelNext].s = 1.0f;
											stakePeelNext = (stakePeelNext + 1) % kStakePeelMax;
										}
										stakeOn[ts.gripTarget] = false;
										ts.gripTarget = -2;
										player.Velocity.y = ts.pullDir.y * kGripLaunchSpeed;
										knockX = ts.pullDir.x * kGripLaunchSpeed;
										knockTimer = kKnockHoldFrames;
										jumpReady = true;
										ts.gripPhase = kGripUnwind;
										isDropped = true;
									}
								}
							}
						}

						// 雲(当たり判定なし。杭と同じく、掴んだ点の近くまで巻き切る):巻くだけでも、少しだけ体力が削れる(巻き切ると kCloudReelChip。体力の割合が下がると、周りの雲が kCloudShedAt ごとに自動で外れる)
						if (ts.gripTarget == -1 && isClear == false && bossHp > 0) {
							float cloudSpan = ts.pullStartDist - kGripReleaseDist;
							if (cloudSpan > 1.0f) {
								float cloudProg = (ts.pullStartDist - pullDist) / cloudSpan;
								if (cloudProg > 1.0f) {
									cloudProg = 1.0f;
								}
								if (cloudProg > ts.pullProg) {
									bossChip += kCloudReelChip * (cloudProg - ts.pullProg);
									ts.pullProg = cloudProg;
									if (bossHitT < kCloudChipShakeFrames) {
										bossHitT = kCloudChipShakeFrames;
									}
									// 体力の割合に合わせて、周りの雲が自動で外れる(引きはがしで剥がれるのと同じ外れ方。引きはがしもそのまま使える)
									while (bossChip >= kCloudShedAt && bossHp > 0) {
										bossChip -= 1.0f;
										peelCloud();
									}
									if (bossHp <= 0) {
										bossChip = 0.0f;
									}
								}
							}
						}

						if (isDropped == true) {
							// もう外れた
						}
						else if (tentHold[s] == false || ts.gripTimer >= kGripPullMax) {
							player.Velocity.y = ts.pullDir.y * kGripPullSpeed;
							knockX = ts.pullDir.x * kGripPullSpeed;
							knockTimer = kKnockHoldFrames;
							ts.ropeLen = pullDist;
							ts.gripPhase = kGripHold;
						}
						else if (pullDist < kGripReleaseDist) {
							// 杭も雲も(雲に当たり判定はない):掴んだ点の向きへ、そのまま飛んでいく
							player.Velocity.y = ts.pullDir.y * kGripLaunchSpeed;
							knockX = ts.pullDir.x * kGripLaunchSpeed;
							knockTimer = kKnockHoldFrames;
							jumpReady = true;
							ts.gripPhase = kGripUnwind;
						}
					}
					else if (ts.gripPhase == kGripUnwind) {
						// 巻きを解きながら、ゲートへしまう
						gt.len -= gp.total / kGripUnwindFrames;
						if (gt.len <= 0.0f) {
							gt.len = 0.0f;
							ts.state = kSetRetract;
							ts.timer = 0;
							ts.t.retractFrom = ts.t.len;
							continue;
						}
					}

					// 引っ張り中:長さは変えない。ゲートが近づいて、まっすぐな部分が短くなった分だけ、先っぽが巻き付く。ぴんと張って、ゆれない
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

			// 触手の形を計算する(ゲート → 杭・雲の接点 → 巻き付く渦、の通り道に沿って点を並べる)
			// s:触手セットの番号
			for (int s = 0; s < 2; s++) {
				TentacleSet& ts = sets[s];
				if (ts.isActive == false) {
					continue;
				}
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
					// まっすぐな部分だけゆらゆらさせる(両端は動かさない。引っ張っている間・ぴんと張っている間はゆれない)
					if (sArc < gp.legLen && gp.legLen > 1.0f) {
						float u = sArc / gp.legLen;
						float amp = 12.0f * sinf(kPi * u) * (1.0f - ts.gripTense);
						float side = sinf(frame * 0.15f + tn.phase + u * 6.0f) * amp;
						float lx = (gp.tangent.x - gp.gate.x) / gp.legLen;
						float ly = (gp.tangent.y - gp.gate.y) / gp.legLen;
						p.x += -ly * side;
						p.y += lx * side;
					}
					pts[s][i] = p;
				}
			}


			// 雲(の攻撃)が、主人公の位置を狙うために使う
			px = player.pos.x;
			py = player.pos.y;
		}

		// 雲の体力が無くなったらクリア(攻撃は止める)
		if (bossHp <= 0 && isClear == false) {
			isClear = true;
			attack = 0;
			attackRunning = false;
			waterH = 0.0f;
			shock = false;
			sunR = 0.0f;
			stretchT = 999;
			// 倒した瞬間の位置で雲を止める
			clearT = 0;
			clearX = bossX;
			clearY = bossY;
			clearScale = bossScale;
		}

		// 倒したあと:止まる → 雲が画面の中央へ動く → 四散する。残った目玉が、登場時のぐるぐるで上へ登って画面の外へ出ていく
		if (isClear == true) {
			clearT++;

			// 雲が中央へ動く(動きながら、ふつうの大きさに戻る)
			if (clearT > kClearHold) {
				float moveE = EaseInOut(static_cast<float>(clearT - kClearHold) / kClearMoveFrames);
				bossX = Lerp(clearX, kClearCenterX, moveE);
				bossY = Lerp(clearY, kClearCenterY, moveE);
				bossScale = Lerp(clearScale, 1.0f, moveE);
			}

			// 中央で四散する。目玉はその場に残る
			if (clearT == kClearScatterAt) {
				for (int i = 0; i < kScatterNum; i++) {
					float ang = (i + (rand() % 100) / 100.0f) * 2.0f * kPi / kScatterNum;
					float spd = kScatterSpeedMin + (kScatterSpeedMax - kScatterSpeedMin) * ((rand() % 100) / 100.0f);
					float off = static_cast<float>(rand() % 60);
					pieces[i].x = kClearCenterX + cosf(ang) * off;
					pieces[i].y = kClearCenterY + sinf(ang) * off * 0.6f;
					pieces[i].vx = cosf(ang) * spd;
					pieces[i].vy = sinf(ang) * spd * 0.8f;
					pieces[i].s = 0.3f + 0.3f * ((rand() % 100) / 100.0f);
				}
				eyeX = kClearCenterX;
				eyeY = kClearCenterY;
				eyeBaseY = kClearCenterY;
				eyeAngle = kPi;
				eyeScale = 1.0f;
				eyeVy = kEyeStartVy;
				eyeT = 0;
			}
			if (clearT >= kClearScatterAt) {
				for (int i = 0; i < kScatterNum; i++) {
					pieces[i].x += pieces[i].vx;
					pieces[i].y += pieces[i].vy;
					pieces[i].vx *= kScatterFriction;
					pieces[i].vy *= kScatterFriction;
				}

				// 少し間をおいてから、目玉が登場時の雲と同じぐるぐるで登っていく
				// (中央から始めて、ぐるぐるの幅と奥行きが少しずつ広がる。だんだん速くなる)
				if (clearT >= kClearScatterAt + kEyeWaitFrames) {
					eyeT++;
					eyeVy += kEyeAccel;
					if (eyeVy > kEyeMaxVy) {
						eyeVy = kEyeMaxVy;
					}
					eyeBaseY -= eyeVy;
					eyeAngle += 2.0f * kPi / kPeriod;
					float ramp = EaseInOut(static_cast<float>(eyeT) / kEyeRampFrames);
					float eyeDepth = sinf(eyeAngle);   // 手前=+1、奥=-1
					eyeX = kCenterX + cosf(eyeAngle) * kRadiusX * ramp;
					eyeY = eyeBaseY + eyeDepth * kRadiusY * ramp;
					eyeScale = 1.0f + eyeDepth * kScaleRange * ramp;

					// 目玉が画面の上へ出ていったら数え始める(そのあと、プレイヤーが杭に引っ張られて上へ飛んでいく)
					if (eyeBaseY < -(kEyeSize * (1.0f + kScaleRange) + kRadiusY)) {
						eyeGoneT++;
					}
				}
			}

			// プレイヤーも画面の上へ出ていったら、画面がフェードアウトしていく
			if (eyeGoneT > kFlyOutDelayFrames && player.pos.y < -kPlayerHalfH) {
				endT++;
			}
		}

		// 1フレーム前の入力を覚えておく
		preTentHold[0] = tentHold[0];
		preTentHold[1] = tentHold[1];
		preJumpHold = jumpHold;
		preReleaseHold = releaseHold;

		// 経過フレームを進める
		if (attack != 0) {
			timer++;
		}
		else {
			timer = 0;
		}

		///
	}
}
