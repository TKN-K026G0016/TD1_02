#include "AssetEditor.h"
#include "CameraBackgroundTuning.h"
// ============================================================================
// 【このファイルの役割】PortalArena.cpp — ボス戦の床・杭・プレイヤー管理
// 移動杭と外装ネジの座標をまとめ、プレイヤー更新へ渡す。背景/杭とプレイヤーは別々に描く。
// 【読むコツ】①公開関数/定数 → ②初期化 → ③Update → ④Draw の順に追う。
// 【座標の基本】このボス戦のワールド座標は右が+X、上が+Y。画面描画時に変換。
// 【編集の約束】今回は元の計算・条件式は変えず、説明コメントを追加。
// ============================================================================

#include "PortalArena.h"
#include "ExactPlayerBattle.h"
#include "MiniUnityEditor.h"
#include "PortalDraw.h"
#include "PortalMath.h"
#include "Texture.h"
#include <Novice.h>
#include <cmath>



using namespace PortalMath;
namespace {
    constexpr int kEditorGripCapacity = MiniUnityEditor::kMaxEditorGripPoints;
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::Initialize()
// 何をする？ オブジェクトを使い始めるときに値を初期状態へ戻す。リトライ時にも重要。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::Initialize() {
    player_.Initialize({ 420.0f,110.0f });
    ExactPlayerBattle::Reset();
    coasterStage_ = false;
    frame_ = 0;lastFailedYankFrame_ = -1000;
    for (int i = 0; i < kMaxAnchors; ++i) movingGpRespawnAt_[i] = 0;
    stage2_.Reset();armorTension_.Reset();connected_ = false;mashPulse_ = false;
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) { armorPointEnabled_[i] = false;armorPoints_[i] = {}; }
    for (int i = 0;i < kExtraBossPoints;++i) { extraPointEnabled_[i] = false;extraPoints_[i] = {}; }

#pragma region テクスチャ読み込み処理
    bgForm1Texture = GetTexture(TextureType::BackgroundForm1);
    bg1Form2Texture = GetTexture(TextureType::Background1Form2);
    bg2Form2Texture = GetTexture(TextureType::Background2Form2);
    bg3Form2Texture = GetTexture(TextureType::Background3Form2);

#pragma endregion

}
// ------------------------------------------------------------------------
// 【関数】PortalArena::SetBattleSettings()
// 何をする？ エディターで保存した戦闘設定を、このオブジェクトへ反映する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::SetBattleSettings(const BossBattleSettings& settings) {
    if (!settings.Valid())return;
    anchorCount_ = settings.anchorCount;
    for (int i = 0;i < anchorCount_;++i)anchors_[i] = { settings.anchors[i].x,settings.anchors[i].y };
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::SetBossArmorTargets()
// 何をする？ 現在残っているボス外装ネジの座標をプレイヤーへ渡すために保存する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
void PortalArena::SetBossArmorTargets(const Vector2 points[BossBattleSettings::kArmorHooks], const bool enabled[BossBattleSettings::kArmorHooks]) {
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) { armorPointEnabled_[i] = enabled[i];armorPoints_[i] = points[i]; }
}
void PortalArena::SetBossExtraTargets(const Vector2 points[kExtraBossPoints], const bool enabled[kExtraBossPoints]) {
    for (int i = 0; i < kExtraBossPoints; ++i) { extraPointEnabled_[i] = enabled[i];extraPoints_[i] = points[i]; }
}
// ------------------------------------------------------------------------
// 【関数】PortalArena::Update()
// 何をする？ 1フレーム分の入力・物理・状態遷移を進める。描画ではなく内部状態を更新する。
// 読み方：引数を確認 → ifで早期終了する条件 → 最後に更新/戻り値を確認。
// ------------------------------------------------------------------------
int PortalArena::ActiveAnchorCount() const {
    return coasterStage_ ? Stage2Tuning::values.gpCount : anchorCount_;
}
Vector2 PortalArena::AnchorPosition(int index) const {
    return coasterStage_ ? stage2_.Anchor(index) : anchors_[index];
}
int PortalArena::CollectSceneGrips(Vector2* grips, int capacity) const {
    return MiniUnityEditor::CollectGripPoints(coasterStage_ ? 2 : 1, grips, capacity);
}

bool PortalArena::Update() {
    const int anchorCount = ActiveAnchorCount();
    // エディターのGPは目標配列に直接書き込む。中間配列とコピーを省く。
    Vector2 points[kMaxAnchors + kEditorGripCapacity + BossBattleSettings::kArmorHooks + kExtraBossPoints] = {};
    const int additionalCount = CollectSceneGrips(points + anchorCount, kEditorGripCapacity);
    const int fixedCount = anchorCount + additionalCount;
    const int totalCount = fixedCount + BossBattleSettings::kArmorHooks + kExtraBossPoints;
    // 【注目】【入力取得】キーボード／ゲームパッドを共通形式へ変換。
    const PortalCommands commands = input_.Read();
    connected_ = commands.connected;
    mashPulse_ = commands.mashTap;
    if (commands.reset) { Initialize();return true; }
    // 【ターゲット一覧】設定された移動用杭＋ボス外装の四隅ネジ4点。
    // 取り外した外装は巨大な距離へ追い出して選択できなくする。
    // 【注目】【目標配列】前半＝移動杭、後半＝ボスのネジ。固定杭の個数を境界としてPlayerに渡す。
    for (int i = 0; i < anchorCount; ++i) {
        // 外れた流れるGPはサンプルの杭と同じ600F、選択対象に含めない。
        points[i] = MovingGpAvailable(i)
            ? AnchorPosition(i)
            : Vector2{ 100000.0f, 100000.0f };
    }
    for (int i = 0;i < BossBattleSettings::kArmorHooks;++i) points[fixedCount + i] = armorPointEnabled_[i]
        ? armorPoints_[i] : Vector2{ 100000.0f,100000.0f };
    // ジェットとパイプも部位剥がしの対象として数える。
    // 目と本体はBoss側で破壊不可にしている。
    // 【プレイヤー更新】敵に倒されていなければ物理・攻撃・触手を進める。
    for (int i = 0; i < kExtraBossPoints; ++i)
        points[fixedCount + BossBattleSettings::kArmorHooks + i] = extraPointEnabled_[i]
        ? extraPoints_[i] : Vector2{ 100000.0f,100000.0f };
    // Run the exact source physical simulation once, not PortalPlayer's old physics.
    // 第2操作パターンは廃止。既存サンプルの非交互パターンのみ使用。
    if (ExactPlayerBattle::IsAlternate()) ExactPlayerBattle::SetAlternate(false);
    if (!player_.IsDefeated()) {
        ExactPlayerBattle::SetMovingAnchorCount(coasterStage_ ? anchorCount : 0);
        ExactPlayerBattle::SetBossAnchorStart(fixedCount);
        // 第2ステージで機体が退場したら、ボスに繋がる触手だけ解除。
        // 背景GPへの接続や、プレイヤーの移動物理は維持する。
        if (coasterStage_ && bossOutsideScreen_)
            ExactPlayerBattle::RequestReleaseBossGrips();
        ExactPlayerBattle::Update(points, totalCount);
        const auto snap = ExactPlayerBattle::GetSnapshot();
        // 第2ステージの背景GPが触手で引き抜かれたら、
        // PortalArena側の描画・照準も消す。ボスのネジは対象外。
        if (coasterStage_ && snap.peeledStake >= 0 && snap.peeledStake < anchorCount &&
            snap.peeledStake < kMaxAnchors) {
            movingGpRespawnAt_[snap.peeledStake] = frame_ + 600;
        }
        player_.SyncSamplePose(snap.position, snap.velocity, snap.gripping, snap.pulling);
        // ネジは「命中時に距離で緩ませ100%」または
        // 「ボスGP＋背景GPを引っ張る」いずれかで外せる。
        // プレイヤーの2本触手の物理・巻き取り計算は変更しない。
        const int part = armorTension_.Update(snap, fixedCount,
            BossBattleSettings::kArmorHooks + kExtraBossPoints,
            points, totalCount);
        // 本体(0)と目(最終番号)は移動用GP。張力が足りても破壊要求しない。
        if (part > 0 && part != BossBattleSettings::kArmorHooks + kExtraBossPoints - 1)
            player_.QueueSampleArmorPeel(part);
        // 実際に引っ張るイベントが発生したときだけ失敗判定する。
        // 緩ませだけで外れた場合に失敗扱いしない。
        if (BossCombatEditor::values.twoGripPeelEnabled &&
            snap.yankTarget >= fixedCount && part < 0 &&
            BossCombatEditor::values.failedYankDamage>0 && frame_ - lastFailedYankFrame_ >= 40) {
            player_.TakeDamage(BossCombatEditor::values.failedYankDamage, player_.GetPosition());
            lastFailedYankFrame_ = frame_;
        }
    }
    ++frame_;
    return false;
}
// ボス巻き直し用インターフェイス。操作と触手シミュレーションは変更しない。
int PortalArena::MostLooseScrew(float* loosenValue) const {
    const int count = BossBattleSettings::kArmorHooks + kExtraBossPoints;
    float best = 0.0f;int chosen = -1;
    for (int i = 0;i < count;++i) {
        if (!ScrewExists(i))continue;
        const float value = armorTension_.Loosen(i);
        if (value > best) { best = value;chosen = i; }
    }
    if (loosenValue)*loosenValue = best;
    return chosen;
}
void PortalArena::TightenScrew(int index, float perFrame) {
    if (ScrewExists(index))armorTension_.Tighten(index, perFrame);
}
bool PortalArena::ScrewExists(int index) const {
    if (index >= 0 && index < BossBattleSettings::kArmorHooks)return armorPointEnabled_[index];
    const int extra = index - BossBattleSettings::kArmorHooks;
    return extra >= 0 && extra < kExtraBossPoints && extraPointEnabled_[extra];
}
Vector2 PortalArena::GetScrewWorldPosition(int index) const {
    if (index >= 0 && index < BossBattleSettings::kArmorHooks)return armorPoints_[index];
    const int extra = index - BossBattleSettings::kArmorHooks;
    return extra >= 0 && extra < kExtraBossPoints ? extraPoints_[extra] : Vector2{};
}
// Stage 2 only. No changes to sample player movement or tentacle rendering.
void PortalArena::UpdateStage2(Vector2 bossPosition, int bossHp, int bossMaxHp, int specialMode, int specialFrame) {
    if (!coasterStage_) return;
    stage2_.Update(player_.GetPosition(), bossPosition, bossHp, bossMaxHp,
        player_.IsGripping() || player_.HasWireMomentum(), specialMode, specialFrame);
}
int PortalArena::Stage2HitPlayer(Vector2 player, Vector2* source) {
    return coasterStage_ ? stage2_.CheckHit(player, 24.0f, source) : 0;
}
