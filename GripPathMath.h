#pragma once
#include <cmath>

// 触手の巻き付き経路。全ステージ・旧サンプル共通。
namespace GripPathMath {
	template<class Vec>
	struct Path {
		Vec gate;      // 根元
		Vec center;    // 杭の中心
		Vec tangent;   // 渦の始点(接点)
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
	template<class Vec>
	inline void Build(const Vec& gate, const Vec& center, float spin, Path<Vec>& gp,
		float radius, float wireHalf, float turns, float coilGap, float pi) {
		gp.gate = gate;
		gp.center = center;
		gp.spin = spin;
		gp.r0 = radius + wireHalf;
		gp.tauMax = turns * 2.0f * pi;
		// 外側ほど1周ごとに coilGap ずつ半径が増える
		gp.rEntry = gp.r0 + turns * coilGap;

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
	template<class Vec>
	inline Vec Point(const Path<Vec>& gp, float s) {
		Vec p;
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


}
