#pragma once
#include "Vector2.h"
namespace ExactPlayerBattle {
	struct Snapshot { Vector2 position = { 420,128 }; Vector2 velocity = {}; bool gripping = false; bool pulling = false; int gripIndex = -1; int peeledStake = -1; };
	void Reset();
	void SetAlternate(bool alternate);
	bool IsAlternate();
	void Update(const Vector2* anchors, int count);
	void Draw();
	Snapshot GetSnapshot();
}
