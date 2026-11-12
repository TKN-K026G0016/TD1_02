#include "Map.h"

#include "Vector2.h" 
#include "Texture.h"
#include "Tool.h"

#include <fstream>
#include <sstream>
#include <string>
#include <assert.h>

#include <algorithm>
#include <vector>

#pragma region データ: マップチップ

//マップチップのサイズ
static constexpr Vector2 kChipSize = { 64, 64 };
//マップのサイズ
static Vector2i mapSize = {};

std::vector<std::vector<int>> map;

//マップチップのテクスチャデータ
Texture mapChipTexture;

#pragma endregion

#pragma region データ: 読み込み関係

const char* kMapFile = "./resources/map_data/stage_sample.csv";
const bool loadFile = true;

#pragma endregion

#pragma region データ: 当たり判定関係

static constexpr float kCircleCheckRate = 0.70f;

#pragma endregion

#pragma region 宣言: 関数

void InitMapChipData(void);
bool LoadMapDataCSV(const char* fileName);

static void LoadMapTexture(void);
static void DrawMapChip(void);

#pragma endregion

void InitMap(void) {
	InitMapChipData();
	LoadMapTexture();
}

void UpdateMap(void) {

}

void DrawMap(void) {
	DrawMapChip();
}

#pragma region 関数: 読み込み関係

/// <summary>
/// マップチップデータ関係の初期化処理
/// </summary>
/// <param name=""></param>
void InitMapChipData(void) {
	if (loadFile) {
		LoadMapDataCSV(kMapFile);
	}
}

/// <summary>
/// CSVファイルからのマップデータ読み込み処理
/// </summary>
/// <param name="fileName"></param>
/// <returns></returns>
bool LoadMapDataCSV(const char* fileName) {
	//データの初期化
	map.clear();
	//マップデータの仮置き場
	std::vector<std::vector<int>> temp = {};

	//ファイルが開けるかのチェック
	std::ifstream file(fileName);
	if (!file.is_open()) return false;

	std::string line;
	bool firstLine = true;

	//CSVファイルの横一行を取得
	while (std::getline(file, line)) {

		//最初の列のみBOMのチェック
		if (firstLine) {
			firstLine = false;

			if (line.size() >= 3 &&
				(unsigned char)line[0] == 0xEF &&
				(unsigned char)line[1] == 0xBB &&
				(unsigned char)line[2] == 0xBF) {
				line = line.substr(3);
			}
		}

		std::stringstream ss(line);
		std::string cell;

		std::vector<int> row;

		//,ごとに分解
		while (std::getline(ss, cell, ',')) {
			//文字列から整数に変換
			row.push_back(std::stoi(cell));
		}

		temp.push_back(row);
	}

	if (!temp.empty()) {
		size_t width = temp[0].size();

		for (const auto& row : temp) {
			if (row.size() != width) {
				return false;
			}
		}
	}

	mapSize.y = static_cast<int>(temp.size());

	if (mapSize.y > 0) {
		mapSize.x = static_cast<int>(temp[0].size());
	}

	//[X][Y]の形に変換
	map.resize(mapSize.x);
	for (int x = 0; x < mapSize.x; x++) {
		map[x].resize(mapSize.y);

		for (int y = 0; y < mapSize.y; y++) {
			int reversedY = mapSize.y - 1 - y;

			map[x][reversedY] = temp[y][x];
		}
	}

	return true;
}

#pragma endregion

#pragma region 関数: 描画処理

/// <summary>
/// マップチップのテクスチャデータ読み込み処理
/// </summary>
static void LoadMapTexture(void) {
	mapChipTexture = GetTexture(TextureType::SampleMapChip);
}

/// <summary>
/// マップチップの描画処理
/// </summary>
static void DrawMapChip(void) {
	for (int x = 0; x < mapSize.x; x++) {
		for (int y = 0; y < mapSize.y; y++) {
			//表示の中心座標
			Vector2 centerPos = { kChipSize.x * x + kChipSize.x / 2 , kChipSize.y * y + kChipSize.y / 2 };

			TypeMapChip type = GetTypeMapChip({ x,y });
			if (type == TypeMapChip::None) continue;
			
			//描画処理
			Texture texture = mapChipTexture;
			texture.animNum = ToInt(type);
			DrawTextureAsObj(texture, centerPos, kChipSize, 0.0f, false);
		}
	}
}

#pragma endregion

#pragma region 関数: 外部参照

/// <summary>
/// マップチップのサイズ取得
/// </summary>
/// <returns>kChipSize</returns>
Vector2 GetChipSize(void) {
	return kChipSize;
}

/// <summary>
/// ワールド座標からマップ座標に変換
/// </summary>
/// <param name="pos">ワールド座標</param>
/// <returns>マップ座標</returns>
Vector2i GetNowMapPos(Vector2 pos) {
	Vector2i mapPos = { ToInt(pos.x / kChipSize.x), ToInt(pos.y / kChipSize.y) };
	return mapPos;
}

/// <summary>
/// 指定マップ座標のマップ外判定
/// </summary>
/// <param name="mapPos">マップ座標</param>
/// <returns>true:マップ外 false:マップ内</returns>
bool CheckOutMap(Vector2i mapPos) {
	return (mapPos.x < 0 || mapPos.x >= mapSize.x
		|| mapPos.y < 0 || mapPos.y >= mapSize.y);
}

/// <summary>
/// マップ座標からマップ種類に変換
/// </summary>
/// <param name="mapPos">マップチップ座標</param>
/// <returns>マップチップ種類</returns>
TypeMapChip GetTypeMapChip(Vector2i mapPos) {
	//マップ外判定
	if (CheckOutMap(mapPos)) {
		return TypeMapChip::None;
	}

	return static_cast<TypeMapChip>(map[mapPos.x][mapPos.y]);
}

/// <summary>
/// 地形との接触判定(矩形)
/// </summary>
/// <param name="prePos">移動適用前の座標</param>
/// <param name="nextPos">移動適用後の座標</param>
/// <param name="size">当たり判定のサイズ</param>
/// <param name="axis">判定する座標軸</param>
/// <returns>地形判定後の移動データ</returns>
MoveResult CheckCanMove(const Vector2& prePos, const Vector2& nextPos, const Vector2& size, MoveAxis axis) {
	switch (axis) {
	case MoveAxis::X: {
		//objの進行方向を取得
		float disX = nextPos.x - prePos.x;
		CollisionDirection direction = {};
		Vector2 vertex[2] = {};

		if (disX > 0) {
			direction = CollisionDirection::Right;
			vertex[0] = { nextPos.x + size.x / 2, nextPos.y + size.y / 2 };
			vertex[1] = { nextPos.x + size.x / 2, nextPos.y - size.y / 2 };

		} else if (disX < 0) {
			direction = CollisionDirection::Left;
			vertex[0] = { nextPos.x - size.x / 2, nextPos.y + size.y / 2 };
			vertex[1] = { nextPos.x - size.x / 2, nextPos.y - size.y / 2 };
		} else {
			return { {nextPos.x, prePos.y}, CollisionDirection::None };
		}

		for (int i = 0; i < 2; i++) {
			//マップ座標に変換
			Vector2i mapPos = GetNowMapPos(vertex[i]);
			//マップ外判定
			if (CheckOutMap(mapPos)) continue;

			//所属しているMap種類を取得
			TypeMapChip type = GetTypeMapChip(mapPos);
			switch (type) {
			case TypeMapChip::Ground:

				MoveResult result;
				result.direction = direction;

				if (direction == CollisionDirection::Right) {
					//ぶつかった箇所のチップの中心座標を取得
					float chipLeft = mapPos.x * kChipSize.x;

					result.pos = { chipLeft - size.x / 2 - 1, prePos.y };
				}
				else {
					float chipRight = mapPos.x * kChipSize.x + kChipSize.x;

					result.pos = { chipRight + size.x / 2 + 1, prePos.y };
				}

				return result;
			}
		}

		// 衝突なし
		return { {nextPos.x, prePos.y}, CollisionDirection::None };
	}
	case MoveAxis::Y: {
		//objの進行方向を取得
		float disY = nextPos.y - prePos.y;
		CollisionDirection direction = {};
		Vector2 vertex[2] = {};

		if (disY > 0) {
			direction = CollisionDirection::Up;
			vertex[0] = { nextPos.x - size.x / 2, nextPos.y + size.y / 2 };
			vertex[1] = { nextPos.x + size.x / 2, nextPos.y + size.y / 2 };
		} 
		else if (disY < 0) {
			direction = CollisionDirection::Down;
			vertex[0] = { nextPos.x - size.x / 2, nextPos.y - size.y / 2 };
			vertex[1] = { nextPos.x + size.x / 2, nextPos.y - size.y / 2 };
		} 
		else {
			return { {prePos.x, nextPos.y}, CollisionDirection::None };
		}

		for (int i = 0; i < 2; i++) {
			//マップ座標に変換
			Vector2i mapPos = GetNowMapPos(vertex[i]);

			//マップ外判定
			if (CheckOutMap(mapPos)) continue;

			//所属しているMap種類を取得
			TypeMapChip type = GetTypeMapChip(mapPos);
			switch (type) {
			case TypeMapChip::Ground: {
				MoveResult result;
				result.direction = direction;

				if (direction == CollisionDirection::Up) {
					//ぶつかった箇所のチップの中心座標を取得
					float chipBottom = mapPos.y * kChipSize.y;
					result.pos = { prePos.x , chipBottom - size.y / 2 - 1 };
				}
				else {
					float chipTop = mapPos.y * kChipSize.y + kChipSize.y;
					result.pos = { prePos.x , chipTop + size.y / 2 + 1 };
				}

				return result;
			}
			}
		}

		// 衝突なし
		return { {prePos.x, nextPos.y}, CollisionDirection::None };
	}
	}

	return {};
}

/// <summary>
/// 地形との接触判定(円)
/// </summary>
/// <param name="prePos">移動適用前の座標</param>
/// <param name="nextPos">移動適用後の座標</param>
/// <param name="radius">当たり判定の半径</param>
/// <param name="axis">判定する座標軸</param>
/// <returns>地形判定後の移動データ</returns>
MoveResult CheckCanMove(const Vector2& prePos, const Vector2& nextPos, const float& radius, MoveAxis axis) {
	switch (axis) {
	case MoveAxis::X: {
		//objの進行方向を取得
		float disX = nextPos.x - prePos.x;
		CollisionDirection direction = {};
		Vector2 vertex[3] = {};

		if (disX > 0) {
			direction = CollisionDirection::Right;
			vertex[0] = { nextPos.x + radius, nextPos.y};
			vertex[1] = { nextPos.x + radius, nextPos.y + radius * kCircleCheckRate};
			vertex[2] = { nextPos.x + radius, nextPos.y - radius * kCircleCheckRate };

		} else if (disX < 0) {
			direction = CollisionDirection::Left;
			vertex[0] = { nextPos.x - radius, nextPos.y };
			vertex[1] = { nextPos.x - radius, nextPos.y + radius * kCircleCheckRate };
			vertex[2] = { nextPos.x - radius, nextPos.y - radius * kCircleCheckRate };
		} else {
			return { {nextPos.x, prePos.y}, CollisionDirection::None };
		}

		for (int i = 0; i < 3; i++) {
			//マップ座標に変換
			Vector2i mapPos = GetNowMapPos(vertex[i]);
			//マップ外判定
			if (CheckOutMap(mapPos)) continue;

			//所属しているMap種類を取得
			TypeMapChip type = GetTypeMapChip(mapPos);
			switch (type) {
			case TypeMapChip::Ground:
				MoveResult result;
				result.direction = direction;

				if (direction == CollisionDirection::Right) {
					//ぶつかった箇所のチップの中心座標を取得
					float chipLeft = mapPos.x * kChipSize.x;

					result.pos = { chipLeft - radius - 1, prePos.y };
				} else {
					float chipRight = mapPos.x * kChipSize.x + kChipSize.x;

					result.pos = { chipRight + radius + 1, prePos.y };
				}

				return result;
			}
		}

		// 衝突なし
		return { {nextPos.x, prePos.y}, CollisionDirection::None };
	}
	case MoveAxis::Y: {
		//objの進行方向を取得
		float disY = nextPos.y - prePos.y;
		CollisionDirection direction = {};
		Vector2 vertex[3] = {};

		if (disY > 0) {
			direction = CollisionDirection::Up;
			vertex[0] = { nextPos.x, nextPos.y + radius };
			vertex[1] = { nextPos.x + radius * kCircleCheckRate, nextPos.y + radius };
			vertex[2] = { nextPos.x - radius * kCircleCheckRate, nextPos.y + radius };
		} else if (disY < 0) {
			direction = CollisionDirection::Down;
			vertex[0] = { nextPos.x, nextPos.y - radius };
			vertex[1] = { nextPos.x + radius * kCircleCheckRate, nextPos.y - radius };
			vertex[2] = { nextPos.x - radius * kCircleCheckRate, nextPos.y - radius };
		} else {
			return { {prePos.x, nextPos.y}, CollisionDirection::None };
		}

		for (int i = 0; i < 3; i++) {
			//マップ座標に変換
			Vector2i mapPos = GetNowMapPos(vertex[i]);

			//マップ外判定
			if (CheckOutMap(mapPos)) continue;

			//所属しているMap種類を取得
			TypeMapChip type = GetTypeMapChip(mapPos);
			switch (type) {
			case TypeMapChip::Ground: {
				MoveResult result;
				result.direction = direction;

				if (direction == CollisionDirection::Up) {
					//ぶつかった箇所のチップの中心座標を取得
					float chipBottom = mapPos.y * kChipSize.y;
					result.pos = { prePos.x , chipBottom - radius - 1 };
				} else {
					float chipTop = mapPos.y * kChipSize.y + kChipSize.y;
					result.pos = { prePos.x , chipTop + radius + 1 };
				}

				return result;
			}
			}
		}

		// 衝突なし
		return { {prePos.x, nextPos.y}, CollisionDirection::None };
	}
	}

	return {};
}

#pragma endregion