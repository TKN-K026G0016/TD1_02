#include "Effect.h"

#include "Texture.h"

#include <vector>
#include <algorithm>

#pragma region データ: Common

//透明度の最大
static constexpr int kAlphaMax = 255;
//透明度の最小
static constexpr int kAlphaMin = 0;

#pragma endregion

#pragma region データ: TriangleEffect

struct DataTriangleEffect {
	//回転速度
	float rotateSpeed = ToFloat(M_PI) / 12.0f;

	//透明化が始まるタイミング
	float startFadeEasingT = 0.1f;

	//通常のイージング速度
	float basicEasingSpeed = 0.085f;
	//イージングモード
	EasingMode easingMode = EasingMode::EaseOut;

	Texture texture = {};
};
static DataTriangleEffect dataTriangleEffect;


static std::vector<TriangleEffect> triangleEffects;

#pragma endregion

#pragma region データ: CircleEffect

struct DataCircleEffect {
	float startSizeValue = 0.0f;

	//透明化が始まるタイミング
	float startFadeEasingT = 0.1f;

	//通常のイージング速度
	float basicEasingSpeed = 0.01f;
	//イージングモード
	EasingMode easingMode = EasingMode::EaseOut;

	Texture texture = {};
};
static DataCircleEffect dataCircleEffect;

static std::vector<CircleEffect> circleEffects;

#pragma endregion

#pragma region データ: PeelOffBackground

struct DataPeelOffBackground {
	Vector2 pos = { 640, 360 };

	Vector2 size = { 1280, 720 };

	float easingSpeed = 0.05f;
	float startFadeEasingT = 0.1f;
	EasingMode easingMode = EasingMode::EaseIn;

	Texture texture = {};
};
static DataPeelOffBackground dataPeelOffBackground;

static std::vector<PeelOffBackground> peelOffBackgrounds;

#pragma endregion

#pragma region データ: AnimEffect

static std::vector<AnimEffect> animEffects;

#pragma endregion

#pragma region データ: ExplosionEffect

struct DataExplosionEffect {
	Vector2 size = { 128, 128 };

	Texture texture = {};
};
static DataExplosionEffect dataExplosionEffect;

#pragma endregion

#pragma region データ: WindScrewEffect

struct DataWindScrewEffect {
	Vector2 size = { 128, 128 };

	Texture texture = {};
};
static DataWindScrewEffect dataWindScrewEffect;

#pragma endregion

#pragma region データ: PeelOffEffect

struct DataPeelOffEffect {
	Vector2 size = { 256, 256 };

	Texture texture = {};
};
static DataPeelOffEffect dataPeelOffEffect;

#pragma endregion

#pragma region データ: HitStopEffect

struct HitStopEffect {
	int time = 0;
	int count = 0;

	int basicTime = 10;

	bool isHitStop = false;
};
static HitStopEffect hitStopEffect;

#pragma endregion

#pragma region 宣言: 関数

static void InitTriangleEffect(void);
static void UpdateTriangleEffect(void);
static void DrawTriangleEffectAsObj(void);
static void DrawTriangleEffectAsUI(void);

static void InitCircleEffect(void);
static void UpdateCircleEffect(void);
static void DrawCircleEffectAsObj(void);
static void DrawCircleEffectAsUI(void);

static void InitPeelOffBackground(void);
static void UpdatePeelOffBackground(void);
static void DrawPeelOffBackground(void);

static void InitAnimEffect();
static void UpdateAnimEffect();
static void DrawAnimEffectAsObj();
static void DrawAnimEffectAsUI();

static void InitExplosionEffect(void);
static void InitWindScrewEffect(void);
static void InitPeelOffEffect(void);

static void InitHitStop(void);
static void UpdateHitStop(void);

#pragma endregion

void InitEffect(void) {
	InitTriangleEffect();
	InitCircleEffect();
	InitPeelOffBackground();

	InitAnimEffect();
	InitExplosionEffect();
	InitWindScrewEffect();
	InitPeelOffEffect();

	InitHitStop();
}

void UpdateEffect(void) {
	UpdateTriangleEffect();
	UpdateCircleEffect();
	UpdatePeelOffBackground();

	UpdateAnimEffect();

	UpdateHitStop();
}

void DrawEffectAsBG(void) {
	DrawPeelOffBackground();
}

void DrawEffectAsObj(void) {

	DrawTriangleEffectAsObj();
	DrawCircleEffectAsObj();

	DrawAnimEffectAsObj();
}

void DrawEffectAsUI(void) {
	DrawTriangleEffectAsUI();
	DrawCircleEffectAsUI();

	DrawAnimEffectAsUI();
}

#pragma region 関数: TriangleEffect

/// <summary>
/// TriangleEffectの初期化処理
/// </summary>
static void InitTriangleEffect(void) {
	//テクスチャの読み込み
	dataTriangleEffect.texture = GetTexture(TextureType::ParticleTriangle);

	//全削除処理
	triangleEffects.clear();

}

/// <summary>
/// TriangleEffectの更新処理
/// </summary>
static void UpdateTriangleEffect(void) {
	for (std::size_t i = 0; i < triangleEffects.size();) {
		triangleEffects[i].Update();

		if (!triangleEffects[i].GetIsShoot()) {
			//削除
			triangleEffects.erase(triangleEffects.begin() + i);

			continue;
		}

		i++;
	}
}

/// <summary>
/// TriangleEffectの描画処理
/// </summary>
static void DrawTriangleEffectAsObj(void) {
	for (std::size_t i = 0; i < triangleEffects.size();) {
		triangleEffects[i].Draw(TypeDrawEffect::Obj);
		i++;
	}
}

/// <summary>
/// TriangleEffectの描画処理
/// </summary>
static void DrawTriangleEffectAsUI(void) {
	for (std::size_t i = 0; i < triangleEffects.size();) {
		triangleEffects[i].Draw(TypeDrawEffect::UI);
		i++;
	}
}

/// <summary>
/// TriangleEffectの生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="sizeValue">サイズ</param>
/// <param name="moveSpeed">移動速度</param>
/// <param name="moveTheta">移動方向</param>
/// <param name="easingSpeed">フェード速度</param>
/// <param name="color">色</param>
/// <param name="typeDraw">描画タイプ</param>
void CreateTriangleEffect(Vector2 pos, float sizeValue, float moveSpeed, float moveTheta, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw) {
	triangleEffects.push_back(TriangleEffect(pos, sizeValue, moveSpeed, moveTheta, easingSpeed, color, typeDraw));
}

#pragma region 関数: TriangleEffectクラス

/// <summary>
/// 生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="moveSpeed">移動速度</param>
/// <param name="moveTheta">移動方向</param>
/// <param name="easingSpeed">イージング速度</param>
/// <param name="typeDraw">描画形式</param>
TriangleEffect::TriangleEffect(Vector2 pos, float sizeValue, float moveSpeed, float moveTheta, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw) {
	isShoot_ = true;
	alpha_ = kAlphaMax;

	if (easingSpeed <= 0.0f) {
		easingSpeed = dataTriangleEffect.basicEasingSpeed;
	}

	pos_ = pos;
	sizeValue_ = sizeValue;
	moveSpeed_ = moveSpeed;
	moveTheta_ = moveTheta;
	easingSpeed_ = easingSpeed;
	typeDraw_ = typeDraw;
	color_ = color;
}

void TriangleEffect::Update(void) {
	Move();
	Easing();
}

void TriangleEffect::Draw(TypeDrawEffect typeDraw) {
	if (typeDraw_ != typeDraw) return;

	Vector2 size = { sizeValue_, sizeValue_ };
	Texture texture = dataTriangleEffect.texture;
	texture.color = (color_ & 0xFFFFFF00) | alpha_;

	if (typeDraw_ == TypeDrawEffect::Obj) {
		DrawTextureAsObj(texture, pos_, size, rotateTheta_, false);
	} else {
		DrawTextureAsUI(texture, pos_, size, rotateTheta_);
	}
}

/// <summary>
/// 移動や回転処理
/// </summary>
void TriangleEffect::Move(void) {
	pos_.x += cosf(moveTheta_) * moveSpeed_;
	pos_.y += sinf(moveTheta_) * moveSpeed_;

	rotateTheta_ += dataTriangleEffect.rotateSpeed;

}

/// <summary>
/// イージング&フェード処理
/// </summary>
void TriangleEffect::Easing(void) {
	//easingT増加処理
	easingT_ += easingSpeed_;
	if (easingT_ >= 1.0f) {
		easingT_ = 1.0f;
		isShoot_ = false;
	}

	//透明化処理
	if (easingT_ >= dataTriangleEffect.startFadeEasingT) {
		float localT = (easingT_ - dataTriangleEffect.startFadeEasingT) / (1.0f - dataTriangleEffect.startFadeEasingT);
		float easedT = CreateEasedT(localT, dataTriangleEffect.easingMode);
		alpha_ = UpdateValueByEasing(kAlphaMax, kAlphaMin, easedT);
	}
}

bool TriangleEffect::GetIsShoot(void) const {
	return isShoot_;
}

#pragma endregion

#pragma endregion

#pragma region 関数: CircleEffect

/// <summary>
/// CircleEffectの初期化処理
/// </summary>
static void InitCircleEffect(void) {
	//テクスチャの取得
	dataCircleEffect.texture = GetTexture(TextureType::ParticleCircle);

	//全消去
	circleEffects.clear();
}

/// <summary>
/// CircleEffectの更新処理
/// </summary>
static void UpdateCircleEffect(void) {
	for (std::size_t i = 0; i < circleEffects.size();) {
		circleEffects[i].Update();

		if (!circleEffects[i].GetIsShoot()) {
			circleEffects.erase(circleEffects.begin() + i);

			continue;
		}

		i++;
	}
}

/// <summary>
/// CircleEffectの描画処理(Obj)
/// </summary>
static void DrawCircleEffectAsObj(void) {
	for (std::size_t i = 0; i < circleEffects.size();) {
		circleEffects[i].Draw(TypeDrawEffect::Obj);

		i++;
	}
}

/// <summary>
/// CircleEffectの描画処理(UI)
/// </summary>
static void DrawCircleEffectAsUI(void) {
	for (std::size_t i = 0; i < circleEffects.size();) {
		circleEffects[i].Draw(TypeDrawEffect::UI);

		i++;
	}
}

/// <summary>
/// CircleEffectの生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="endSizeValue">最終的なサイズ</param>
/// <param name="easingSpeed">イージング速度</param>
/// <param name="color">色</param>
/// <param name="typeDraw">表示タイプ</param>
void CreateCircleEffect(Vector2 pos, float endSizeValue, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw) {
	circleEffects.push_back(CircleEffect(pos, endSizeValue, easingSpeed, color, typeDraw));
}

#pragma region 関数: CircleEffectクラス

/// <summary>
/// 生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="endSizeValue">最終的なサイズ</param>
/// <param name="easingSpeed">イージング速度</param>
/// <param name="color">色</param>
/// <param name="typeDraw">表示タイプ</param>
CircleEffect::CircleEffect(Vector2 pos, float endSizeValue, float easingSpeed, unsigned int color, TypeDrawEffect typeDraw) {
	isShoot_ = true;
	alpha_ = kAlphaMax;

	if (easingSpeed <= 0.0f) {
		easingSpeed = dataCircleEffect.basicEasingSpeed;
	}

	pos_ = pos;
	endSizeValue_ = endSizeValue;
	easingSpeed_ = easingSpeed;
	color_ = color;
	typeDraw_ = typeDraw;
}

/// <summary>
/// 更新処理
/// </summary>
void CircleEffect::Update(void) {
	//easingT増加処理
	easingT_ += easingSpeed_;
	if (easingT_ >= 1.0f) {
		easingT_ = 1.0f;
		isShoot_ = false;
	}

	//拡大処理
	{
		float easedT = CreateEasedT(easingT_, dataCircleEffect.easingMode);
		sizeValue_ = UpdateValueByEasing(dataCircleEffect.startSizeValue, endSizeValue_, easedT);
	}

	if (easingT_ >= dataCircleEffect.startFadeEasingT) {

		float localT = (easingT_ - dataCircleEffect.startFadeEasingT) / (1.0f - dataCircleEffect.startFadeEasingT);
		float easedT = CreateEasedT(localT, dataCircleEffect.easingMode);
		alpha_ = UpdateValueByEasing(kAlphaMax, kAlphaMin, easedT);
	}
}

/// <summary>
/// 描画処理
/// </summary>
void CircleEffect::Draw(TypeDrawEffect typeDraw) {
	if (typeDraw_ != typeDraw) return;

	Vector2 size = { sizeValue_, sizeValue_ };

	Texture texture = dataCircleEffect.texture;
	texture.color = (color_ & 0xFFFFFF00) | alpha_;

	if (typeDraw_ == TypeDrawEffect::Obj) {
		DrawTextureAsObj(texture, pos_, size, 0.0f, false);
	} else {
		DrawTextureAsUI(texture, pos_, size, 0.0f);
	}

}

bool CircleEffect::GetIsShoot(void) const {
	return isShoot_;
}

#pragma endregion

#pragma endregion

#pragma region 関数: PeelOffBackground

static void InitPeelOffBackground(void) {
	//データの取得
	dataPeelOffBackground.texture = GetTexture(TextureType::PeelOffBackground);

	//全削除
	peelOffBackgrounds.clear();
}

static void UpdatePeelOffBackground(void) {
	for (std::size_t i = 0; i < peelOffBackgrounds.size();) {
		peelOffBackgrounds[i].Update();

		if (!peelOffBackgrounds[i].GetIsShoot()) {
			peelOffBackgrounds.erase(peelOffBackgrounds.begin() + i);

			continue;
		}

		i++;
	}
}

static void DrawPeelOffBackground(void) {
	for (std::size_t i = 0; i < peelOffBackgrounds.size();) {
		peelOffBackgrounds[i].Draw();

		i++;
	}
}

/// <summary>
/// 引き剝がし背景演出の生成処理
/// </summary>
void CreatePeelOffBackground(void) {
	peelOffBackgrounds.push_back(PeelOffBackground());
}

#pragma region 関数: PeelOffBackgroundクラス

PeelOffBackground::PeelOffBackground() {
	isShoot_ = true;
	alpha_ = kAlphaMin;
}

void PeelOffBackground::Update(void) {
	easingT_ += dataPeelOffBackground.easingSpeed;
	if (easingT_ >= 1.0f) {
		easingT_ = 1.0f;
		isShoot_ = false;
	}

	//フェードイン
	if (easingT_ < dataPeelOffBackground.startFadeEasingT) {
		float localT = easingT_ / dataPeelOffBackground.startFadeEasingT;
		float easedT = CreateEasedT(localT, dataPeelOffBackground.easingMode);
		alpha_ = UpdateValueByEasing(kAlphaMin, kAlphaMax, easedT);
	}
	//フェードアウト
	else {
		float localT = (easingT_ - dataPeelOffBackground.startFadeEasingT) / (1.0f - dataPeelOffBackground.startFadeEasingT);
		float easedT = CreateEasedT(localT, dataPeelOffBackground.easingMode);
		alpha_ = UpdateValueByEasing(kAlphaMax, kAlphaMin, easedT);
	}

}

void PeelOffBackground::Draw(void) {
	Texture texture = dataPeelOffBackground.texture;
	texture.color = (WHITE & 0xFFFFFF00) | alpha_;

	DrawTextureAsUI(texture, dataPeelOffBackground.pos, dataPeelOffBackground.size, 0.0f);
}

bool PeelOffBackground::GetIsShoot(void) const {
	return isShoot_;
}

#pragma endregion

#pragma endregion

#pragma region 関数: AnimEffect

/// <summary>
/// animEffect初期化処理
/// </summary>
static void InitAnimEffect(void) {
	animEffects.clear();
}

/// <summary>
/// animEffect更新処理
/// </summary>
static void UpdateAnimEffect(void) {
	for (std::size_t i = 0; i < animEffects.size();) {
		animEffects[i].Update();

		//消去処理
		if (!animEffects[i].GetIsShoot()) {
			animEffects.erase(animEffects.begin() + i);

			continue;
		}

		i++;
	}
}

/// <summary>
/// animEffect描画処理(obj)
/// </summary>
static void DrawAnimEffectAsObj(void) {
	for (std::size_t i = 0; i < animEffects.size();) {
		animEffects[i].Draw(TypeDrawEffect::Obj);

		i++;
	}
}

/// <summary>
/// animEffect描画処理(UI)
/// </summary>
static void DrawAnimEffectAsUI(void) {
	for (std::size_t i = 0; i < animEffects.size();) {
		animEffects[i].Draw(TypeDrawEffect::UI);

		i++;
	}
}

#pragma region 関数: AnimEffectクラス

AnimEffect::AnimEffect(Vector2 pos, Texture texture, Vector2 size, TypeDrawEffect typeDraw) {
	isShoot_ = true;

	pos_ = pos;
	texture_ = texture;
	size_ = size;
	typeDraw_ = typeDraw;

}

//更新処理
void AnimEffect::Update(void) {
	UpdateAnimation(texture_);

	//最後のアニメーションまでいったら削除
	if (texture_.animNum >= texture_.animLimit) {
		isShoot_ = false;
	}
}

/// <summary>
/// 描画処理
/// </summary>
/// <param name="typeDraw"></param>
void AnimEffect::Draw(TypeDrawEffect typeDraw) {
	if (typeDraw_ != typeDraw) return;


	if (typeDraw_ == TypeDrawEffect::Obj) {
		DrawTextureAsObj(texture_, pos_, size_, 0.0f, false);

	} else {
		DrawTextureAsUI(texture_, pos_, size_, 0.0f);
	}
}

bool AnimEffect::GetIsShoot(void) const {
	return isShoot_;
}

#pragma endregion

#pragma endregion

#pragma region 関数: ExplosionEffect

/// <summary>
/// ExplosionEffectの初期化処理
/// </summary>
static void InitExplosionEffect(void) {
	//データ読み込み
	dataExplosionEffect.texture = GetTexture(TextureType::ExplosionEffect);
}

/// <summary>
/// ExplosionEffectの生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="color">色</param>
/// <param name="typeDraw">表示タイプ</param>
void CreateExplosionEffect(Vector2 pos, TypeDrawEffect typeDraw) {
	animEffects.push_back(AnimEffect(pos, dataExplosionEffect.texture, dataExplosionEffect.size, typeDraw));
}


#pragma endregion

#pragma region 関数: WindScrewEffect

/// <summary>
/// windScrewEffectの初期化処理
/// </summary>
static void InitWindScrewEffect(void) {
	//データ読み込み
	dataWindScrewEffect.texture = GetTexture(TextureType::WindScrewEffect);
}

/// <summary>
/// windScrewEffectの生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="typeDraw">表示タイプ</param>
void CreateWindScrewEffect(Vector2 pos, TypeDrawEffect typeDraw) {
	animEffects.push_back(AnimEffect(pos, dataWindScrewEffect.texture, dataWindScrewEffect.size, typeDraw));
}

#pragma endregion

#pragma region 関数: PeelOffEffect

/// <summary>
/// PeelOffEffectのデータ読み込み処理
/// </summary>
static void InitPeelOffEffect(void) {
	dataPeelOffEffect.texture = GetTexture(TextureType::PeelOffEffect);
}

/// <summary>
/// PeelOffEffectの生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="typeDraw">表示タイプ</param>
void CreatePeelOffEffect(Vector2 pos, TypeDrawEffect typeDraw) {
	animEffects.push_back(AnimEffect(pos, dataPeelOffEffect.texture, dataPeelOffEffect.size, typeDraw));
}

#pragma endregion

#pragma region 関数: HitStopEffect

static void InitHitStop(void) {
	hitStopEffect.time = 0;
	hitStopEffect.count = 0;
}

static void UpdateHitStop(void) {
	if (!hitStopEffect.isHitStop) return;

	hitStopEffect.count++;
	if (hitStopEffect.count >= hitStopEffect.time) {
		hitStopEffect.time = 0;
		hitStopEffect.count = 0;
		hitStopEffect.isHitStop = false;
	}

}

/// <summary>
/// ヒットストップ発生処理
/// </summary>
/// <param name="time">持続時間</param>
void TriggerHitStopEffect(int time) {
	hitStopEffect.isHitStop = true;

	if (time <= 0) {
		time = hitStopEffect.basicTime;
	}

	hitStopEffect.time = time;
	hitStopEffect.count = 0;
}

/// <summary>
/// HitStop状態化の確認処理
/// </summary>
/// <returns>hitStopEffect.isHitStop</returns>
bool GetIsHitStop(void) {
	return hitStopEffect.isHitStop;
}

#pragma endregion
