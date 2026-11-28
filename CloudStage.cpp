// ステージ3の入口。状態を固定領域に構築して再利用する。
// std::functionの大きなキャプチャと動的確保を廃止。
#include "CloudStage.h"
#include "BossCloud.h"
#include <optional>

namespace {
    std::optional<CloudRuntime::CloudBoss> cloudBoss;
}
void InitCloudStage() { cloudBoss.emplace(); }
void UpdateCloudStage() { if (cloudBoss) cloudBoss->Update(); }
void DrawCloudStage() { if (cloudBoss) cloudBoss->Draw(); }
