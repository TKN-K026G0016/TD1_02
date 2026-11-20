#pragma once
// 統合エディターから書き出したゲーム用データ（C++17）
namespace CreatorRuntime {
	struct AnimationKey { int frame; float x, y, scale; int ease; };
	inline constexpr int kAnimationLength = 120;
	inline constexpr int kAnimationKeyCount = 2;
	inline constexpr AnimationKey kAnimationKeys[] = {
	  {0,280.0f,250.0f,1.0f,0},
	  {60,510.0f,250.0f,1.0f,3},
	};
	struct EffectSettings { int count, life; float speed, gravity, size, spread, direction, x, y; float color[4]; };
	inline constexpr EffectSettings kEffect = { 40,70,4.0f,0.0799999982f,5.0f,6.28318548f,0.0f,430.0f,320.0f,{1.0f,0.49000001f,0.25f,1.0f} };
	struct HudBar { float x, y; int width, height, value, maxValue; float color[4]; };
	inline constexpr HudBar kHud = { 400.0f,100.0f,230,22,75,100,{0.239999995f,0.860000014f,0.560000002f,1.0f} };
	struct Gimmick { int type, id; float x, y, distance, speed; int group; bool locked, enabled; };
	inline constexpr int kGimmickCount = 1;
	inline constexpr Gimmick kGimmicks[] = {
	  {0,0,420.0f,360.0f,96.0f,0.0350000001f,0,false,true},
	};
	struct UIWidget { int type; float x, y; int width, height, value, maxValue; float color[4]; const char* text; int group; bool locked, visible; };
	inline constexpr int kUIWidgetCount = 0;
	inline constexpr UIWidget kUIWidgets[] = {
	 {0,0.0f,0.0f,16,8,0,1,{1.0f,1.0f,1.0f,1.0f},"",0,false,true},
	};
	inline constexpr float kCameraStart[3] = { -16.0f,-88.0f,1.0f };
	inline constexpr int kSpriteFrameCount = 4;
	inline constexpr int kSpriteFrameHold = 6;
	inline constexpr int kSpriteCellWidth = 32;
	inline constexpr int kSpriteCellHeight = 32;
	inline constexpr int kSpriteColumns = 4;
	inline constexpr int kSpriteRows = 1;
	inline constexpr const char* kSpritePath = "";
	struct AnimationClip { const char* name; int firstKey, keyCount, length; float speed; bool loop; const char* spritePath; int cellW, cellH, columns, rows, frames, hold; };
	inline constexpr int kClipCount = 1;
	inline constexpr int kSelectedClip = 0;
	inline constexpr AnimationKey kClipKeys[] = {
	 {0,280.0f,250.0f,1.0f,0},
	 {60,510.0f,250.0f,1.0f,3},
	};
	inline constexpr AnimationClip kClips[] = {
	 {"基本動作",0,2,120,1.0f,true,"",32,32,4,1,4,6},
	};
	struct Emitter { const char* name; int count, life; float x, y, speed, gravity, size, spread, direction; float color[4]; bool loop, enabled; };
	inline constexpr int kEmitterCount = 1;
	inline constexpr Emitter kEmitters[] = {
	 {"エミッター",40,70,430.0f,320.0f,4.0f,0.0799999982f,5.0f,6.28318548f,0.0f,{1.0f,0.49000001f,0.25f,1.0f},false,true},
	};
	inline constexpr bool kUiSnap = false;
	struct AnimationEvent { int clip, frame, type, target, value; };
	inline constexpr int kAnimationEventCount = 0;
	inline constexpr AnimationEvent kAnimationEvents[] = {
	 {0,0,0,0,0},
	};
	struct EmitterFlow { bool continuous; float rate; };
	inline constexpr EmitterFlow kEmitterFlows[] = {
	 {false,2.0f},
	};
	struct UIAction { int anchor, action, target; };
	inline constexpr UIAction kUIActions[] = {
	 {0,0,0},
	};
	struct CameraKey { int frame; float x, y, zoom; int ease; };
	inline constexpr int kCameraLength = 120;
	inline constexpr bool kCameraLoop = false;
	inline constexpr float kCameraShake = 8.0f;
	inline constexpr int kCameraKeyCount = 2;
	inline constexpr CameraKey kCameraKeys[] = {
	 {0,-16.0f,-88.0f,1.0f,0},
	 {120,100.0f,-88.0f,1.0f,3},
	};
}
