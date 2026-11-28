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
	float basicEasingSpeed = 0.01f;
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

#pragma region データ: ExplosionEffect

struct DataExplosionEffect {
	Vector2 size = { 128, 128 };

	Texture texture = {};
};
static DataExplosionEffect dataExplosionEffect;

static std::vector<ExplosionEffect> explosionEffects;

#pragma endregion

#pragma region データ: ExplosionEffect

struct DataWindScrewEffect {
	Vector2 size = { 128, 128 };

	Texture texture = {};
};
static DataWindScrewEffect dataWindScrewEffect;

static std::vector<WindScrewEffect> windScrewEffects;

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

static void InitExplosionEffect(void);
static void UpdateExplosionEffect(void);
static void DrawExplosionEffectAsObj(void);
static void DrawExplosionEffectAsUI(void);

static void InitWindScrewEffect(void);
static void UpdateWindScrewEffect(void);
static void DrawWindScrewEffectAsObj(void);
static void DrawWindScrewEffectAsUI(void);

#pragma endregion

void InitEffect(void) {
	InitTriangleEffect();
	InitCircleEffect();
	InitExplosionEffect();
	InitWindScrewEffect();
}

void UpdateEffect(void) {
	UpdateTriangleEffect();
	UpdateCircleEffect();
	UpdateExplosionEffect();
	UpdateWindScrewEffect();
}

void DrawEffectAsObj(void) {
	DrawTriangleEffectAsObj();
	DrawCircleEffectAsObj();
	DrawExplosionEffectAsObj();
	DrawWindScrewEffectAsObj();
}

void DrawEffectAsUI(void) {
	DrawTriangleEffectAsUI();
	DrawCircleEffectAsUI();
	DrawExplosionEffectAsUI();
	DrawWindScrewEffectAsUI();
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

#pragma region 関数: ExplosionEffect

/// <summary>
/// ExplosionEffectの初期化処理
/// </summary>
static void InitExplosionEffect(void) {
	//データ読み込み
	dataExplosionEffect.texture = GetTexture(TextureType::ExplosionEffect);

	//全消去
	explosionEffects.clear();
}

/// <summary>
/// ExplosionEffectの更新処理
/// </summary>
static void UpdateExplosionEffect(void) {
	for (std::size_t i = 0; i < explosionEffects.size();) {
		explosionEffects[i].Update();

		//消去処理
		if (!explosionEffects[i].GetIsShoot()) {
			explosionEffects.erase(explosionEffects.begin() + i);

			continue;
		}

		i++;
	}
}

/// <summary>
/// ExplosionEffectの描画処理(Obj)
/// </summary>
static void DrawExplosionEffectAsObj(void) {
	for (std::size_t i = 0; i < explosionEffects.size();) {
		explosionEffects[i].Draw(TypeDrawEffect::Obj);

		i++;
	}
}

/// <summary>
/// ExplosionEffectの描画処理(UI)
/// </summary>
static void DrawExplosionEffectAsUI(void) {
	for (std::size_t i = 0; i < explosionEffects.size();) {
		explosionEffects[i].Draw(TypeDrawEffect::UI);

		i++;
	}
}

/// <summary>
/// ExplosionEffectの生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="color">色</param>
/// <param name="typeDraw">表示タイプ</param>
void CreateExplosionEffect(Vector2 pos, unsigned int color, TypeDrawEffect typeDraw) {
	explosionEffects.push_back(ExplosionEffect(pos, color, typeDraw));
}

#pragma region 関数: ExplosionEffectクラス

/// <summary>
/// 生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="color">色</param>
/// <param name="typeDraw">表示タイプ</param>
ExplosionEffect::ExplosionEffect(Vector2 pos, unsigned int color, TypeDrawEffect typeDraw) {
	isShoot_ = true;
	texture_ = dataExplosionEffect.texture;

	pos_ = pos;
	color_ = color;
	typeDraw_ = typeDraw;
}

/// <summary>
/// 更新処理
/// </summary>
void ExplosionEffect::Update(void) {
	UpdateAnimation(texture_);

	//最後のアニメーションまでいったら削除
	if (texture_.animNum >= texture_.animLimit) {
		isShoot_ = false;
	}
}

/// <summary>
/// 描画処理
/// </summary>
/// <param name="typeDraw">表示タイプ</param>
void ExplosionEffect::Draw(TypeDrawEffect typeDraw) {
	if (typeDraw_ != typeDraw) return;

	Texture texture = texture_;
	texture.color = color_;

	if (typeDraw_ == TypeDrawEffect::Obj) {
		DrawTextureAsObj(texture, pos_, dataExplosionEffect.size, 0.0f, false);
	} else {
		DrawTextureAsUI(texture, pos_, dataExplosionEffect.size, 0.0f);
	}
}

bool ExplosionEffect::GetIsShoot(void) const {
	return isShoot_;
}

#pragma endregion

#pragma endregion

#pragma region 関数: WindScrewEffect

/// <summary>
/// windScrewEffectの初期化処理
/// </summary>
static void InitWindScrewEffect(void) {
	//データ読み込み
	dataWindScrewEffect.texture = GetTexture(TextureType::WindScrewEffect);

	//全消去
	windScrewEffects.clear();
}

/// <summary>
/// windScrewEffectの更新処理
/// </summary>
static void UpdateWindScrewEffect(void) {
	for (std::size_t i = 0; i < windScrewEffects.size();) {
		windScrewEffects[i].Update();

		//消去処理
		if (!windScrewEffects[i].GetIsShoot()) {
			windScrewEffects.erase(windScrewEffects.begin() + i);

			continue;
		}

		i++;
	}
}

/// <summary>
/// windScrewEffectの描画処理(Obj)
/// </summary>
static void DrawWindScrewEffectAsObj(void) {
	for (std::size_t i = 0; i < windScrewEffects.size();) {
		windScrewEffects[i].Draw(TypeDrawEffect::Obj);

		i++;
	}
}

/// <summary>
/// WindScrewEffectの描画処理(UI)
/// </summary>
static void DrawWindScrewEffectAsUI(void) {
	for (std::size_t i = 0; i < windScrewEffects.size();) {
		windScrewEffects[i].Draw(TypeDrawEffect::UI);

		i++;
	}
}

/// <summary>
/// windScrewEffectの生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="typeDraw">表示タイプ</param>
void CreateWindScrewEffect(Vector2 pos, TypeDrawEffect typeDraw) {
	windScrewEffects.push_back(WindScrewEffect(pos, typeDraw));
}

#pragma region 関数: windScrewEffectクラス

/// <summary>
/// 生成処理
/// </summary>
/// <param name="pos">生成座標</param>
/// <param name="typeDraw">表示タイプ</param>
WindScrewEffect::WindScrewEffect(Vector2 pos, TypeDrawEffect typeDraw) {
	isShoot_ = true;
	texture_ = dataWindScrewEffect.texture;

	pos_ = pos;
	typeDraw_ = typeDraw;
}

/// <summary>
/// 更新処理
/// </summary>
void WindScrewEffect::Update(void) {
	UpdateAnimation(texture_);

	//最後のアニメーションまでいったら削除
	if (texture_.animNum >= texture_.animLimit) {
		isShoot_ = false;
	}
}

/// <summary>
/// 描画処理
/// </summary>
/// <param name="typeDraw">表示タイプ</param>
void WindScrewEffect::Draw(TypeDrawEffect typeDraw) {
	if (typeDraw_ != typeDraw) return;

	Texture texture = texture_;

	if (typeDraw_ == TypeDrawEffect::Obj) {
		DrawTextureAsObj(texture, pos_, dataWindScrewEffect.size, 0.0f, false);
	} else {
		DrawTextureAsUI(texture, pos_, dataWindScrewEffect.size, 0.0f);
	}
}

bool WindScrewEffect::GetIsShoot(void) const {
	return isShoot_;
}

#pragma endregion

#pragma endregion
