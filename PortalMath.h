#pragma once
#include "Vector2.h"
#include <cmath>
namespace PortalMath {
	inline Vector2 Add(Vector2 a, Vector2 b) { return { a.x + b.x,a.y + b.y }; }
	inline Vector2 Sub(Vector2 a, Vector2 b) { return { a.x - b.x,a.y - b.y }; }
	inline Vector2 Mul(Vector2 a, float b) { return { a.x * b,a.y * b }; }
	inline float Dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
	inline float Length(Vector2 a) { return std::sqrt(Dot(a, a)); }
	inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
	inline Vector2 Unit(Vector2 a) { float n = Length(a); return n > 0.001f ? Mul(a, 1.0f / n) : Vector2{ 1,0 }; }
	inline Vector2 Lerp(Vector2 a, Vector2 b, float t) { return Add(a, Mul(Sub(b, a), t)); }
	inline Vector2 Limit(Vector2 a, float m) { float n = Length(a); return n > m ? Mul(a, m / n) : a; }
	inline Vector2 Rotate(Vector2 a, float t) { return { a.x * std::cos(t) - a.y * std::sin(t), a.x * std::sin(t) + a.y * std::cos(t) }; }
}
