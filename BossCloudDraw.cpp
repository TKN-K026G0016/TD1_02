#include "BossCloud.h"
#include <cmath>
#include <cstdio>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace CloudRuntime {
	void CloudBoss::Draw() {

		auto RoundPx = [](float v) -> int {
			return static_cast<int>(floorf(v + 0.5f));
			};

		// 絵を、中心(cx, cy)・大きさ size(px)・角度 ang で描く
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

		// L/R の表示を、中心(cx, cy)に描く(setIdx 0=L 1=R)。触手の色で色分けする。絵が無い時は、色つきの丸と文字
		auto DrawLRBadge = [&](int setIdx, float cx, float cy, float size, unsigned int col) {
			if (lrTex >= 0) {
				DrawRotSprite(lrTex, cx, cy, size, 0.0f, setIdx * kLRSpriteSize, 0, kLRSpriteSize, kLRSpriteSize, col);
			}
			else {
				int rad = static_cast<int>(size * 0.5f);
				Novice::DrawEllipse(RoundPx(cx), RoundPx(cy), rad, rad, 0.0f, col, kFillModeSolid);
				Novice::ScreenPrintf(RoundPx(cx) - 4, RoundPx(cy) - 8, (setIdx == 0) ? "L" : "R");
			}
			};

		// L/R のまわりを回る、半円形の矢印2本(ang:回転角)。絵が無い時は、丸い枠と回る三角2つ
		auto DrawCycleRing = [&](float cx, float cy, float size, float ang, unsigned int col) {
			if (cycleTex >= 0) {
				DrawRotSprite(cycleTex, cx, cy, size, ang, 0, 0, kCycleSpriteSize, kCycleSpriteSize, col);
			}
			else {
				float rad = size * 0.5f;
				Novice::DrawEllipse(RoundPx(cx), RoundPx(cy), static_cast<int>(rad), static_cast<int>(rad), 0.0f, col, kFillModeWireFrame);
				for (int a = 0; a < 2; a++) {
					float th = ang + a * kPi;
					float rx = cosf(th);              // 外向き
					float ry = sinf(th);
					float tx = -ry;                   // 進む向き(時計まわり)
					float ty = rx;
					float px2 = cx + rx * rad;
					float py2 = cy + ry * rad;
					Novice::DrawTriangle(
						RoundPx(px2 + tx * 10.0f), RoundPx(py2 + ty * 10.0f),
						RoundPx(px2 - tx * 4.0f + rx * 7.0f), RoundPx(py2 - ty * 4.0f + ry * 7.0f),
						RoundPx(px2 - tx * 4.0f - rx * 7.0f), RoundPx(py2 - ty * 4.0f - ry * 7.0f),
						col, kFillModeSolid);
				}
			}
			};


		// 背景
		DrawSpriteC(sprBg, 640.0f, 360.0f, 1280.0f, 720.0f, 255.0f);

		// 杭(ひっかけポイント。上から落ちてきて下へスクロール)
		for (int i = 0; i < kStakeMax; i++) {
			if (stakeOn[i] == true) {
				// 耐久度が減っている杭は、赤黒くなって、耐久バーが出る。引き伸ばされた直後は震える
				float hpRate = stakeHp[i] / kStakeHpMax;
				if (hpRate < 0.0f) { hpRate = 0.0f; }
				if (hpRate > 1.0f) { hpRate = 1.0f; }
				unsigned int stakeTint = 0x8B5A2BFFu;
				if (hpRate < 1.0f) {
					float bright = kStakeDamagedTint + (1.0f - kStakeDamagedTint) * hpRate;
					stakeTint = 0xFF000000u | (static_cast<unsigned int>(139.0f * bright) << 16) | (static_cast<unsigned int>(90.0f * bright) << 8) | 0xFFu;
				}
				int shakeX = 0;
				if (frame < stakeShakeUntil[i]) {
					shakeX = (frame % 2 == 0) ? static_cast<int>(kStakeShakePx) : -static_cast<int>(kStakeShakePx);
				}
				int stakeDrawX = static_cast<int>(stakeX[i]) + shakeX;
				int stakeDrawY = static_cast<int>(stakeY[i]);
				Novice::DrawEllipse(stakeDrawX, stakeDrawY, static_cast<int>(kGripRadius), static_cast<int>(kGripRadius), 0.0f, stakeTint, kFillModeSolid);
				if (hpRate < 1.0f) {
					int barW = static_cast<int>(kGripRadius * 2.0f);
					int barX = stakeDrawX - barW / 2;
					int barY = stakeDrawY - static_cast<int>(kGripRadius) - 10;
					unsigned int barCol = (hpRate > 0.6f) ? 0x60E060FFu : ((hpRate > 0.3f) ? 0xF0C030FFu : 0xE04040FFu);
					Novice::DrawBox(barX, barY, barW, 5, 0.0f, 0x000000B0u, kFillModeSolid);
					Novice::DrawBox(barX, barY, static_cast<int>(barW * hpRate), 5, 0.0f, barCol, kFillModeSolid);
				}
			}
		}

		// 太陽の危険地帯(円が広がる範囲)。20〜60でふわっと出て(太陽のそばから広がる)、220〜250でふわっと消える
		if (attack == 5 && timer >= 20 && timer < 250) {
			float dangerRate = 1.0f;
			if (timer < 60) {
				dangerRate = EaseInOut((timer - 20) / 40.0f);
			}
			else if (timer >= 220) {
				dangerRate = 1.0f - EaseInOut((timer - 220) / 30.0f);
			}
			if (sunBroken == true) {
				dangerRate *= sunR / 80.0f;   // 壊された太陽と一緒に、危険地帯も消える
			}
			int dangerR = int((80.0f + 25.0f + 350.0f) * (0.5f + 0.5f * dangerRate)); // 円が一番広がったときの半径(出る・消える時は小さめから)
			unsigned int dangerFillA = static_cast<unsigned int>(40.0f * dangerRate);
			unsigned int dangerLineA = static_cast<unsigned int>(230.0f * dangerRate);
			Novice::DrawEllipse(int(kSunX), int(kSunY), dangerR, dangerR, 0.0f, 0xFF000000 | dangerFillA, kFillModeSolid);
			Novice::DrawEllipse(int(kSunX), int(kSunY), dangerR, dangerR, 0.0f, 0xFF000000 | dangerLineA, kFillModeWireFrame);
		}

		// 太陽(雲の後ろから出てくる)
		if (attack == 5 && sunR > 0.0f) {
			float d = sunR * 2.56f; // 画像の円の半径が画像の約78%なので、その分大きく描く
			float sunShakeX = 0.0f;
			if (frame < stakeShakeUntil[kSunSlot]) {
				if (frame % 2 == 0) {
					sunShakeX = kStakeShakePx;
				}
				else {
					sunShakeX = -kStakeShakePx;
				}
			}
			DrawSpriteC(sprSun, kSunX + sunShakeX, kSunY, d, d, 255.0f);
			// 耐久バー(巻かれて減った時だけ)
			float sunHpRate = stakeHp[kSunSlot] / kSunHpMax;
			if (sunHpRate < 0.0f) { sunHpRate = 0.0f; }
			if (sunHpRate < 1.0f && sunBroken == false) {
				int sunBarW = 120;
				int sunBarX = static_cast<int>(kSunX) - sunBarW / 2;
				int sunBarY = static_cast<int>(kSunY - sunR) - 18;
				unsigned int sunBarCol = 0xE04040FFu;
				if (sunHpRate > 0.6f) {
					sunBarCol = 0x60E060FFu;
				}
				else if (sunHpRate > 0.3f) {
					sunBarCol = 0xF0C030FFu;
				}
				Novice::DrawBox(sunBarX, sunBarY, sunBarW, 6, 0.0f, 0x000000B0u, kFillModeSolid);
				Novice::DrawBox(sunBarX, sunBarY, static_cast<int>(sunBarW * sunHpRate), 6, 0.0f, sunBarCol, kFillModeSolid);
			}
			// 60〜220フレームで、回りながら外へ広がって、また太陽のそばへ戻ってくる
			float spread = 0.0f;
			if (timer >= 60 && timer < 220) {
				spread = sinf((timer - 60) / 160.0f * kPi) * 350.0f;
			}
			float orbRate = sunR / 80.0f; // 太陽が育つ・縮む間は、火の玉も小さい(急に大きい玉が出ない)
			for (int k = 0; k < 8; k++) {
				float a = frame * 0.03f + k * kPi / 4.0f + spread * 0.01f; // 広がるほど余計に回る
				float r = sunR + 25.0f * orbRate + spread;
				DrawSpriteC(sprOrbYellow, kSunX + cosf(a) * r, kSunY + sinf(a) * r, 24.0f * orbRate, 24.0f * orbRate, 255.0f * orbRate);
			}
		}

		// 突撃の危険マーク(出現はフェードイン、固定後は点滅)
		if (attack == 2 && timer >= 45 && timer < 155) {
			float fa = Clamp01((timer - 45) / 15.0f);
			float a = 200.0f * fa;
			if (timer >= 105 && (timer / 4) % 2 == 0) {
				a = 255.0f * fa;
			}
			DrawSpriteC(sprDangerBox, tx, ty, 140.0f, 140.0f, a);
		}

		// 雷の子雲(ボスの後ろから出てくる)・危険線・雷
		if (attack == 1) {
			float e = 1.0f; // 0=ボスの中、1=所定の位置
			if (timer < 30) {
				e = EaseOut(timer / 30.0f);
			}
			else if (timer >= 110) {
				e = 1.0f - EaseInOut((timer - 110) / 30.0f);
			}
			for (int i = 0; i < 3; i++) {
				float cx = Lerp(bossX, thunderX[i], e);
				float cy = Lerp(bossY + 10.0f, 260.0f, e);
				float s = Lerp(0.15f, 0.5f, e);
				DrawSpriteC(sprCloudDark, cx, cy, sprCloudDark.w * s, sprCloudDark.h * s, 255.0f);

				// 危険線(最初は点滅、直前は点灯しっぱなし)
				if (timer >= 35 && timer < 95) {
					if ((timer / 6) % 2 == 0 || timer >= 80) {
						DrawSpriteC(sprDangerLine, thunderX[i], 290.0f + 430.0f / 2.0f, 8.0f, 430.0f, 230.0f);
					}
				}
				// 雷(bolt_anim.png のコマを、上下に並べて長くする。バチバチ切り替わる)
				if (timer >= 95 && timer < 110) {
					int boltFrame = (timer / kBoltFrameSpeed) % kBoltFrameNum;
					int boltTileH = kBoltSrcSize * kBoltScale;
					int boltTopY = 290;                                            // 雷の上の端
					int boltDestBottom = 720;                                      // 並べ始める下の端
					int boltDestLeft = static_cast<int>(thunderX[i]) - boltTileH / 2;
					while (boltDestBottom > boltTopY) {
						int boltDestTop = boltDestBottom - boltTileH;
						int boltSrcCrop = 0;                                       // 上の端を切り落とす絵のpx
						if (boltDestTop < boltTopY) {
							boltSrcCrop = (boltTopY - boltDestTop) / kBoltScale;
							boltDestTop = boltTopY;
						}
						Novice::DrawQuad(boltDestLeft, boltDestTop, boltDestLeft + boltTileH, boltDestTop, boltDestLeft, boltDestBottom, boltDestLeft + boltTileH, boltDestBottom,
							boltFrame * kBoltSrcSize, boltSrcCrop, kBoltSrcSize, kBoltSrcSize - boltSrcCrop, boltTex, 0xFFFFFFFF);
						boltDestBottom -= boltTileH;
					}
				}
			}
		}

		// 雨
		for (int i = 0; i < 200; i++) {
			if (drops[i].on) {
				DrawSpriteC(sprDrop, drops[i].x, drops[i].y + 8.0f, 6.0f, 24.0f, 220.0f);
			}
		}
		// 水(感電中は黄色くバチバチ点滅)。水の本体の上に、波の絵(water_top_wave.png)を横に並べて、ゆっくり流す
		if (waterH > 0.0f) {
			const Sprite* water = &sprWater;
			if (shock) {
				water = ((timer / 3) % 2 == 0) ? &sprWaterShockA : &sprWaterShockB;
			}
			// 波の絵:水が低い間は、画面の下からせり上がってくる。絵の kWaterTopCrestRow 行目が水面の高さ
			int waterTileW = kWaterTopFrameW * kWaterTopScale;
			int waterTileH = kWaterTopSrcH * kWaterTopScale;
			float waterRise = waterH / kWaterTopRiseH;
			if (waterRise > 1.0f) {
				waterRise = 1.0f;
			}
			int waterTopY = static_cast<int>(720.0f - waterH - kWaterTopCrestRow * kWaterTopScale * waterRise + sinf(frame * 0.08f) * kWaterTopBob);
			// 水の本体は、波の絵の kWaterTopBodyRow 行目より下を埋める(波が上下にゆれても隙間が出ないよう、ゆれ幅ぶん上から始める)
			int waterBodyY = waterTopY + kWaterTopBodyRow * kWaterTopScale - static_cast<int>(kWaterTopBob);
			if (waterBodyY < 720) {
				DrawSpriteC(*water, 640.0f, (waterBodyY + 720) / 2.0f, 1280.0f, static_cast<float>(720 - waterBodyY), 255.0f);
			}
			// 波を横に流す(1枚ぶん流れると、元の並びに戻る)
			int waterScroll = static_cast<int>(frame * kWaterTopScroll) % waterTileW;
			int waterFrame = (frame / kWaterTopFrameSpeed) % kWaterTopFrameNum;   // 波のアニメの、いまのコマ
			int waterTileNum = 1280 / waterTileW + 2;                              // 画面を埋めて、流れる分も足りる枚数
			// 感電中は、黄色い波の絵に切り替える。水の本体のBの時は、少し濃い黄色にして合わせる
			int waterTex = waterTopTex;
			unsigned int waterCol = 0xFFFFFFFF;
			if (shock) {
				waterTex = waterTopShockTex;
				if ((timer / 3) % 2 != 0) {
					waterCol = 0xF6D670FF;
				}
			}
			for (int wt = 0; wt < waterTileNum; wt++) {
				int wx0 = wt * waterTileW - waterScroll;
				int wx1 = wx0 + waterTileW;
				if (wx1 <= 0 || wx0 >= 1280) {
					continue;
				}
				Novice::DrawQuad(wx0, waterTopY, wx1, waterTopY, wx0, waterTopY + waterTileH, wx1, waterTopY + waterTileH,
					waterFrame * kWaterTopFrameW, 0, kWaterTopFrameW, kWaterTopSrcH, waterTex, waterCol);
			}
		}

		// 雨の攻撃の雷(1本)
		if (attack == 3) {
			float topY = bossY + 55.0f;
			float len = 720.0f - topY;
			if (timer >= 280 && timer < 330) {
				if ((timer / 6) % 2 == 0 || timer >= 315) {
					DrawSpriteC(sprDangerLine, lightX, topY + len / 2.0f, 8.0f, len, 230.0f);
				}
			}
			if (timer >= 330 && timer < 345) {
				int boltFrame = (timer / kBoltFrameSpeed) % kBoltFrameNum;
				int boltTileH = kBoltSrcSize * kBoltScale;
				int boltTopY = static_cast<int>(topY);                         // 雷の上の端(雲の下)
				int boltDestBottom = 720;                                      // 並べ始める下の端
				int boltDestLeft = static_cast<int>(lightX) - boltTileH / 2;
				while (boltDestBottom > boltTopY) {
					int boltDestTop = boltDestBottom - boltTileH;
					int boltSrcCrop = 0;                                       // 上の端を切り落とす絵のpx
					if (boltDestTop < boltTopY) {
						boltSrcCrop = (boltTopY - boltDestTop) / kBoltScale;
						boltDestTop = boltTopY;
					}
					Novice::DrawQuad(boltDestLeft, boltDestTop, boltDestLeft + boltTileH, boltDestTop, boltDestLeft, boltDestBottom, boltDestLeft + boltTileH, boltDestBottom,
						boltFrame * kBoltSrcSize, boltSrcCrop, kBoltSrcSize, kBoltSrcSize - boltSrcCrop, boltTex, 0xFFFFFFFF);
					boltDestBottom -= boltTileH;
				}
			}
		}

		// 吐き出されて上へ飛んでいくカエル(ボスの後ろから出てくる)
		if (attack == 4) {
			for (int i = 0; i < kFrogNum; i++) {
				if (flying[i]) {
					// frog_shot.png:くるくる回るコマを順に出す(カエルごとに回り始めをずらす)
					int frogShotFrame = (timer / kFrogShotFrameSpeed + i) % kFrogShotFrameNum;
					float frogShotHalf = kFrogShotSize / 2.0f;
					int frogShotLeft = static_cast<int>(fx[i] - frogShotHalf);
					int frogShotTop = static_cast<int>(fy[i] - frogShotHalf);
					int frogShotRight = static_cast<int>(fx[i] + frogShotHalf);
					int frogShotBottom = static_cast<int>(fy[i] + frogShotHalf);
					Novice::DrawQuad(frogShotLeft, frogShotTop, frogShotRight, frogShotTop, frogShotLeft, frogShotBottom, frogShotRight, frogShotBottom,
						frogShotFrame * kFrogShotSrcSize, 0, kFrogShotSrcSize, kFrogShotSrcSize, frogShotTex, 0xFFFFFFFF);
				}
			}
		}

		// ボス(雷と雨のときは暗い雲)。雲はかたまりごとに描く(剥がれたかたまりは描かない)。残りが0になると、目玉だけが残る
		if (isClear == false || clearT < kClearScatterAt) {
			const Sprite& bossSprite = (attack == 1 || attack == 3) ? sprCloudDark : sprCloud;
			float bossH = bossSprite.h * bossScale;
			// 伸びる向きの反対側の端を固定して伸ばす(下へ出すなら上の端、上へ出すなら下の端)
			float bossDrawY = bossY + stretchDir * bossH * (stretchSY - 1.0f) / 2.0f;
			float hitShake = 0.0f;
			if (bossHitT > 0) {
				if (bossHitT % 2 == 0) {
					hitShake = 8.0f;
				}
				else {
					hitShake = -8.0f;
				}
			}
			for (int i = 0; i < kCloudPartNum; i++) {
				if (parts[i].alive == false) {
					continue;
				}
				float bob = sinf(frame * 0.05f + i * 1.3f) * 3.0f * bossScale;   // かたまりごとに、すこしずつ上下にゆれる
				float partX = bossX + hitShake + kPartDef[i][0] * bossScale * stretchSX;
				float partY = bossDrawY + kPartDef[i][1] * bossScale * stretchSY + bob;
				float partScale = kPartDef[i][2] * bossScale;
				DrawSpriteC(bossSprite, partX, partY, bossSprite.w * partScale * stretchSX, bossSprite.h * partScale * stretchSY, bossAlpha);
			}
			// 雲の中央の目玉(雲と同じ位置・大きさ・濃さで描く)
			DrawEyeC(sprEye, bossX + hitShake, bossDrawY, kEyeSize * bossScale * stretchSX, bossAlpha);
		}

		// 剥がれて飛んでいく雲のかたまり
		for (int i = 0; i < kCloudPartNum; i++) {
			if (peels[i].on == true) {
				float peelFade = 1.0f - static_cast<float>(peels[i].t) / kPeelPieceFrames;
				DrawSpriteC(sprCloud, peels[i].x, peels[i].y, sprCloud.w * peels[i].s, sprCloud.h * peels[i].s, 255.0f * peelFade);
			}
		}

		// 抜けて飛んでいく杭
		for (int i = 0; i < kStakePeelMax; i++) {
			if (stakePeels[i].on == true) {
				float stakeFade = 1.0f - static_cast<float>(stakePeels[i].t) / kPeelPieceFrames;
				unsigned int stakeColor = 0x8B5A2B00u | static_cast<unsigned int>(255.0f * stakeFade);
				Novice::DrawEllipse(static_cast<int>(stakePeels[i].x), static_cast<int>(stakePeels[i].y), static_cast<int>(kGripRadius), static_cast<int>(kGripRadius), 0.0f, stakeColor, kFillModeSolid);
			}
		}

		// 倒した後:四散する雲のかけらと、上へ逃げていく目玉
		if (isClear == true && clearT >= kClearScatterAt) {
			float eyeD = kEyeSize * eyeScale;
			if (eyeY > -eyeD) {
				DrawEyeC(sprEye, eyeX, eyeY, eyeD, 255.0f);
			}
		}

		// 引っ張っている触手だけ明るくする(引っ張っていない触手は、ふつうの色のまま。薄くも暗くもしない)
		bool isPulling[2];
		bool isPeelReady[2];   // この触手を引っ張れば、もう片方が掴んでいるもの(杭・雲)を引きはがせる
		unsigned int drawCol[2];
		unsigned int glowCol[2];
		for (int h = 0; h < 2; h++) {
			isPulling[h] = (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase == kGripPull);
			isPeelReady[h] = false;
		}
		for (int h = 0; h < 2; h++) {
			int holdH = 1 - h;   // 引きはがされる側の触手
			if (sets[holdH].isActive == false || sets[holdH].state != kSetOut || sets[holdH].gripPhase != kGripHold || sets[holdH].gripTarget < -1) {
				continue;
			}
			if (sets[h].isActive == true && sets[h].state == kSetOut && sets[h].gripPhase != kGripUnwind) {
				float gapX = sets[holdH].gripPos.x - sets[h].gripPos.x;
				float gapY = sets[holdH].gripPos.y - sets[h].gripPos.y;
				float gapLen = sqrtf(gapX * gapX + gapY * gapY);
				// 2つの先が kPeelNeedLen 以上離れていて、掛かっているロープの長さより遠い(引くとロープが限界まで伸びる)
				if (gapLen >= kPeelNeedLen && gapLen > sets[holdH].ropeLen) {
					isPeelReady[h] = true;
				}
			}
		}
		float peelWave = 0.5f + 0.5f * sinf(frame * kPeelReadyPulse);
		for (int h = 0; h < 2; h++) {
			float cr = static_cast<float>((kTentColor[h] >> 24) & 0xFF);
			float cg = static_cast<float>((kTentColor[h] >> 16) & 0xFF);
			float cb = static_cast<float>((kTentColor[h] >> 8) & 0xFF);
			unsigned int ca = 0xFF;
			if (isPulling[h] == true) {
				cr += (255.0f - cr) * kPullBright;   // 白へ近づけて明るく
				cg += (255.0f - cg) * kPullBright;
				cb += (255.0f - cb) * kPullBright;
			}
			drawCol[h] = (static_cast<unsigned int>(cr) << 24) | (static_cast<unsigned int>(cg) << 16) | (static_cast<unsigned int>(cb) << 8) | ca;
			glowCol[h] = (drawCol[h] & 0xFFFFFF00u);
		}
		float glowWave = 0.5f + 0.5f * sinf(frame * kPullGlowSpeed);
		unsigned int glowAlpha = static_cast<unsigned int>(kPullGlowAlphaMin + (kPullGlowAlphaMax - kPullGlowAlphaMin) * glowWave);

		if (playerOn == true) {
			// ゲート(触手の色で出す)
			for (int s = 0; s < 2; s++) {
				TentacleSet& ts = sets[s];
				if (ts.isActive == false || ts.gateOpen <= 0.0f) {
					continue;
				}
				float cx = ts.gatePos.x;
				float cy = ts.gatePos.y;

				// 杭・雲に口を向ける細長いゲート
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

			// 触手:曲線を kTentacleStep px ごとに区切って、区切りごとに絵を貼る(太さは絵の真ん中を削って変更)
			// 区切りの点ごとに、向き(法線)と太さを1回だけ計算する。隣り合う区切りが同じ点・同じ幅を共有するので、境目に隙間ができない
			for (int s = 0; s < 2; s++) {
				TentacleSet& ts = sets[s];
				if (ts.isActive == false) {
					continue;
				}
				// 0回目:ふつうに描く  1回目:引っ張っている触手だけ、加算で重ね描きして光らせる
				int passCount = 1;
				if (isPulling[s] == true || isPeelReady[s] == true) {
					passCount = 2;
				}
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

						// 曲線を、根元から先っぽへ kTentacleStep px ごとに区切る
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

						// 最後の区切りから先っぽまでのはんぱな長さ
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

						// 区切りの点ごとの、法線(曲線に直角な向き)と太さ
						float perpX[kTentacleMaxSample];
						float perpY[kTentacleMaxSample];
						float halfSrcF[kTentacleMaxSample];   // 太さ(絵のドット数・片側)
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

							// 今出ている長さに対して、根元から先っぽへ細くなる
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

						// 区切りごとに絵を左右2枚の四角形で貼る(上の端は点 n、下の端は点 n+1 の値を使う)
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
								static_cast<int>(floorf((cx0 - perpX[n] * wTop) + 0.5f)), static_cast<int>(floorf((cy0 - perpY[n] * wTop) + 0.5f)),
								static_cast<int>(floorf((cx0)+0.5f)), static_cast<int>(floorf((cy0)+0.5f)),
								static_cast<int>(floorf((cx1 - perpX[n + 1] * wBot) + 0.5f)), static_cast<int>(floorf((cy1 - perpY[n + 1] * wBot) + 0.5f)),
								static_cast<int>(floorf((cx1)+0.5f)), static_cast<int>(floorf((cy1)+0.5f)),
								0, srcY, srcHalfW, srcRows, tentacleTex, tentCol);

							// 右半分
							Novice::DrawQuad(
								static_cast<int>(floorf((cx0)+0.5f)), static_cast<int>(floorf((cy0)+0.5f)),
								static_cast<int>(floorf((cx0 + perpX[n] * wTop) + 0.5f)), static_cast<int>(floorf((cy0 + perpY[n] * wTop) + 0.5f)),
								static_cast<int>(floorf((cx1)+0.5f)), static_cast<int>(floorf((cy1)+0.5f)),
								static_cast<int>(floorf((cx1 + perpX[n + 1] * wBot) + 0.5f)), static_cast<int>(floorf((cy1 + perpY[n + 1] * wBot) + 0.5f)),
								kTentacleSpriteW - srcHalfW, srcY, srcHalfW, srcRows, tentacleTex, tentCol);
						}
					}
					if (pass == 1) {
						Novice::SetBlendMode(kBlendModeNormal);
					}
				}
			}


			// プレイヤー(左向きの時は左右の角を入れ替えて反転)
			{
				float sideX = player.isFacingRight ? kPlayerHalfW : -kPlayerHalfW;
				float cx = player.pos.x;
				float cy = player.pos.y;
				Novice::DrawQuad(
					static_cast<int>(cx - sideX), static_cast<int>(cy - kPlayerHalfH),
					static_cast<int>(cx + sideX), static_cast<int>(cy - kPlayerHalfH),
					static_cast<int>(cx - sideX), static_cast<int>(cy + kPlayerHalfH),
					static_cast<int>(cx + sideX), static_cast<int>(cy + kPlayerHalfH),
					animationFrame* kPlayerSpriteW, 0, kPlayerSpriteW, kPlayerSpriteH,
					playerTex, 0xFFFFFFFF);
			}

			// L/R の表示:プレイヤーの左下に L、右下に R(触手の色で色分け)。
			// 触手がひっかけポイントに着いている間は、その L/R のまわりを半円形の矢印2本がぐるぐる回る
			for (int s = 0; s < 2; s++) {
				float sideSign = (s == 0) ? -1.0f : 1.0f;
				float lrX = player.pos.x + sideSign * kLRUiOffsetX;
				float lrY = player.pos.y + kLRUiOffsetY;
				if (lrRingFade[s] > 0.0f) {
					float ringScale = 0.6f + 0.4f * lrRingFade[s];   // 出る時にふわっと大きくなる
					unsigned int ringAlpha = static_cast<unsigned int>(255.0f * lrRingFade[s]);
					unsigned int ringCol = (drawCol[s] & 0xFFFFFF00u) | ringAlpha;
					DrawCycleRing(lrX, lrY, kLRRingSize * ringScale, lrRingAng[s], ringCol);
				}
				DrawLRBadge(s, lrX, lrY, kLRUiSize, drawCol[s]);
			}

			// 入力方向の矢印:プレイヤーのまわりに、入力した向きを指して出る(狙っている杭がある時は黄色)
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
						static_cast<int>(floorf((ax + (-half) * c - (-half) * sn) + 0.5f)), static_cast<int>(floorf((ay + (-half) * sn + (-half) * c) + 0.5f)),
						static_cast<int>(floorf((ax + half * c - (-half) * sn) + 0.5f)), static_cast<int>(floorf((ay + half * sn + (-half) * c) + 0.5f)),
						static_cast<int>(floorf((ax + (-half) * c - half * sn) + 0.5f)), static_cast<int>(floorf((ay + (-half) * sn + half * c) + 0.5f)),
						static_cast<int>(floorf((ax + half * c - half * sn) + 0.5f)), static_cast<int>(floorf((ay + half * sn + half * c) + 0.5f)),
						0, 0, kAimArrowSpriteSize, kAimArrowSpriteSize, aimArrowTex, arrowColor);
				}
				else {
					// 絵が無い時の仮の矢印(三角)
					Novice::DrawTriangle(
						static_cast<int>(floorf((ax + c * half) + 0.5f)), static_cast<int>(floorf((ay + sn * half) + 0.5f)),
						static_cast<int>(floorf((ax - c * half - (-sn) * half * 0.7f) + 0.5f)), static_cast<int>(floorf((ay - sn * half - c * half * 0.7f) + 0.5f)),
						static_cast<int>(floorf((ax - c * half + (-sn) * half * 0.7f) + 0.5f)), static_cast<int>(floorf((ay - sn * half + c * half * 0.7f) + 0.5f)),
						arrowColor, kFillModeSolid);
				}
			}

			// 触手が掴んでいるもの(雲・杭)に、その触手の色の輪とL/Rを出す。引っ張っている間は輪が脈打つ
			for (int s = 0; s < 2; s++) {
				TentacleSet& ts = sets[s];
				if (ts.isActive == false || ts.state != kSetOut || ts.gripPhase == kGripUnwind || ts.gripTarget < -1) {
					continue;
				}
				float ringR = kGripRadius + 10.0f;
				if (ts.gripTarget == -1) {
					ringR = 70.0f * bossScale;
				}
				else if (ts.gripTarget == kSunSlot) {
					ringR = sunR + 10.0f;
				}
				if (ts.gripPhase == kGripPull) {
					ringR += 4.0f * sinf(frame * 0.5f);
				}
				if (isPeelReady[s] == true) {
					ringR += 3.0f + 5.0f * peelWave;   // 引きはがせる:輪が膨らんで脈打つ
				}
				int ringX = RoundPx(ts.gripPos.x);
				int ringY = RoundPx(ts.gripPos.y);
				Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR), static_cast<int>(ringR), 0.0f, drawCol[s], kFillModeWireFrame);
				Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 1, static_cast<int>(ringR) + 1, 0.0f, drawCol[s], kFillModeWireFrame);
				if (ts.gripPhase == kGripPull || isPeelReady[s] == true) {
					// 引っ張っている先・引きはがせる時は、輪を太く(外側にもう2本)
					Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 2, static_cast<int>(ringR) + 2, 0.0f, drawCol[s], kFillModeWireFrame);
					Novice::DrawEllipse(ringX, ringY, static_cast<int>(ringR) + 3, static_cast<int>(ringR) + 3, 0.0f, drawCol[s], kFillModeWireFrame);
				}
				DrawLRBadge(s, static_cast<float>(ringX), ringY - ringR - kLRMarkSize * 0.5f - 2.0f, kLRMarkSize, drawCol[s]);
			}

			// ひっかけポイントの自動照準マーク(次に撃つ触手の色で出す。狙っているものにはL/R)
			for (int i = 0; i < gripCandCount; i++) {
				bool isTarget = (isGripLock == true && gripCandIdx[i] == gripLockIdx);
				float mx = bossX;
				float my = bossY;
				if (gripCandIdx[i] >= 0) {
					mx = stakeX[gripCandIdx[i]];
					my = stakeY[gripCandIdx[i]];
				}
				float half = (isTarget ? kGripMarkSize : kGripMarkSizeSub) * 0.5f;
				float ang = 0.0f;
				unsigned int markBase = 0xFFFFFFFFu;
				if (gripMarkSet >= 0) {
					markBase = kTentColor[gripMarkSet];
				}
				unsigned int markColor = (markBase & 0xFFFFFF00u) | 0x70u;
				if (isTarget == true) {
					half *= 1.0f + 0.08f * sinf(frame * 0.25f);   // ふわっと脈打つ
					ang = frame * 0.04f;                           // ゆっくり回る
					markColor = markBase;
					if (gripMarkSet >= 0) {
						DrawLRBadge(gripMarkSet, mx, my + half + kLRMarkSize * 0.5f + 2.0f, kLRMarkSize, kTentColor[gripMarkSet]);
					}
				}
				if (gripMarkTex >= 0) {
					float c = cosf(ang);
					float sn = sinf(ang);
					Novice::DrawQuad(
						static_cast<int>(floorf((mx + (-half) * c - (-half) * sn) + 0.5f)), static_cast<int>(floorf((my + (-half) * sn + (-half) * c) + 0.5f)),
						static_cast<int>(floorf((mx + half * c - (-half) * sn) + 0.5f)), static_cast<int>(floorf((my + half * sn + (-half) * c) + 0.5f)),
						static_cast<int>(floorf((mx + (-half) * c - half * sn) + 0.5f)), static_cast<int>(floorf((my + (-half) * sn + half * c) + 0.5f)),
						static_cast<int>(floorf((mx + half * c - half * sn) + 0.5f)), static_cast<int>(floorf((my + half * sn + half * c) + 0.5f)),
						0, 0, kGripMarkSpriteSize, kGripMarkSpriteSize, gripMarkTex, markColor);
				}
				else {
					// 絵が無い時の仮のマーク(丸い枠)
					Novice::DrawEllipse(static_cast<int>(floorf((mx)+0.5f)), static_cast<int>(floorf((my)+0.5f)), static_cast<int>(half), static_cast<int>(half), 0.0f, markColor, kFillModeWireFrame);
				}
			}

		}


		// 火の玉
		for (int i = 0; i < 100; i++) {
			if (bullets[i].on) {
				DrawSpriteC(sprOrbRed, bullets[i].x, bullets[i].y, 20.0f, 20.0f, 255.0f);
			}
		}

		// 画面に張り付くカエル(画面の一番手前。攻撃が終わっても残る)
		if (frogsOn) {
			for (int i = 0; i < kFrogNum; i++) {
				int jumpStart = i * kFrogStagger; // 手前に跳んでくる開始(frogT基準)
				if (frogT < jumpStart) {
					continue;
				}
				// 奥(画面中央あたり・小さい)から手前(大きい)へ、弧を描いて跳ぶ
				float u = Clamp01((frogT - jumpStart) / 20.0f);
				float e = EaseIn(u);
				float cx = Lerp(640.0f + (frogs[i].x - 640.0f) * 0.2f, frogs[i].x, e);
				float cy = Lerp(220.0f, frogs[i].y, e) - sinf(u * kPi) * 90.0f;
				float s = Lerp(0.12f, 1.0f, e);
				float sx = 1.0f;
				float sy = 1.0f;
				float a = 230.0f;

				// 着地のぺちゃっ(横に広がって戻る)
				int land = frogT - (jumpStart + 20);
				if (land >= 0 && land < 8) {
					float k = 1.0f - land / 8.0f;
					sx = 1.0f + 0.2f * k;
					sy = 1.0f - 0.25f * k;
				}

				float rx = frogs[i].rx * s * sx;
				float ry = frogs[i].ry * s * sy;

				// ペロッと剥がれる:下の端を残して上からめくれ、そのまま落ちて消える
				int peelStart = kFrogStayFrames + i * kFrogStagger;
				if (frogT >= peelStart) {
					float p = (frogT - peelStart) / 20.0f;
					if (p >= 1.0f) {
						continue;
					}
					float flap = EaseInOut(p);
					float newRy = ry * (1.0f - 0.85f * flap);
					cy += ry - newRy;       // 下の端が動かないよう、中心を下げる
					cy += p * p * 140.0f;   // 剥がれきると落ちていく
					rx *= 1.0f + 0.15f * flap;
					ry = newRy;
					a = 230.0f * (1.0f - p * p);
				}
				DrawSpriteC(sprFrog, cx, cy, rx * 2.0f, ry * 2.0f, a);
			}
		}

		// 突撃の着弾フラッシュ
		if (attack == 2 && timer >= 155 && timer < 162) {
			Novice::DrawBox(0, 0, 1280, 720, 0.0f, 0xFFFFFF60, kFillModeSolid);
		}

		// 風(下に落ちた時の、吹き上げる演出)
		if (windFade > 0.0f) {
			unsigned int windA = static_cast<unsigned int>(160.0f * windFade);
			for (int i = 0; i < 12; i++) {
				int windX = 80 + i * 100;
				int windY = 720 - (frame * 30 + i * 97) % 720;
				Novice::DrawLine(windX, windY, windX, windY + 60, 0xFFFFFF00 | windA);
			}
		}

		// 雲の体力
		if (playerOn == true) {
			Novice::DrawBox(340, 12, 600, 14, 0.0f, 0x00000080, kFillModeSolid);
			if (bossHp > 0) {
				float bossBarRate = (bossHp - bossChip) / kBossHpMax;
				if (bossBarRate < 0.0f) {
					bossBarRate = 0.0f;
				}
				Novice::DrawBox(340, 12, static_cast<int>(600.0f * bossBarRate), 14, 0.0f, 0xFF4040FF, kFillModeSolid);
			}
		}

		// 触手の状態(色つきの四角がL/Rの触手の色)
		if (playerOn == true) {
			for (int h = 0; h < 2; h++) {
				Novice::DrawBox(10, 662 + h * 26, 18, 18, 0.0f, kTentColor[h], kFillModeSolid);
				const char* tentName = "L";
				if (h == 1) {
					tentName = "R";
				}
				const char* stateText = "READY";
				if (sets[h].isActive == true) {
					if (sets[h].gripPhase == kGripReach) {
						stateText = "SHOOT";
					}
					else if (sets[h].gripPhase == kGripHold) {
						stateText = "HOOKED (rope)";
					}
					else if (sets[h].gripPhase == kGripPull) {
						stateText = "PULLING";
					}
					else {
						stateText = "RETURN";
					}
				}
				Novice::ScreenPrintf(36, 662 + h * 26, "%s : %s", tentName, stateText);
			}
		}

		// 説明
		Novice::ScreenPrintf(8, 40, "A/D:MOVE  W/S:AIM  SPACE:JUMP  Q/E:TENTACLE L/R(HOLD:PULL)");
		Novice::ScreenPrintf(8, 60, "LSHIFT:RELEASE  F:RESET  TAB:PAD DEBUG  F7:STAGE 1");

		if (isPadDebug == true) {
			Novice::ScreenPrintf(10, 80, "stick: %.2f %.2f", padX, padY);
			int debugY = 100;
			for (int i = 0; i < 32; i++) {
				if (Novice::IsPressButton(0, static_cast<PadButton>(i))) {
					Novice::ScreenPrintf(10, debugY, "pad button: %d", i);
					debugY += 20;
				}
			}
		}

		// フェードアウト(目玉が画面の外へ出たら、だんだん暗くなる)
		if (endT > 0) {
			float fadeRate = Clamp01(static_cast<float>(endT) / kEndFadeFrames);
			Novice::DrawBox(0, 0, kWindowWidth, kWindowHeight, 0.0f, 0x00000000 | static_cast<unsigned int>(ClampAlpha(255.0f * fadeRate)), kFillModeSolid);
			if (endT >= kEndFadeFrames) {
				//クリア処理
			}
		}

		///

		if (endT >= kEndFadeFrames) Novice::ScreenPrintf(510, 340, "STAGE 3 CLEAR!  F7: STAGE 1");
#ifdef USE_IMGUI
		if (showCloudEditor) {
			if (ImGui::Begin("ステージ3：雲ボス", &showCloudEditor)) {
				ImGui::Text("HP: %d / %d", bossHp, kBossHpMax);
				ImGui::Text("攻撃番号: %d", attack);
				ImGui::SliderFloat("剥がす距離", &kPeelNeedLen, 100.0f, 800.0f);
				ImGui::SliderFloat("巻き取りダメージ", &kCloudReelChip, 0.05f, 1.0f);
				ImGui::SliderInt("攻撃間隔（フレーム）", &kRestFrames, 30, 360);
				ImGui::TextUnformatted("F: リトライ / F7: ステージ1");
			}
			ImGui::End();
		}
#endif
	}
}
