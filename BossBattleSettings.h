// 【このファイルの役割】BossBattleSettings.h — エディターとゲームをつなぐ設定データ
// ボス位置、杭、ネジ、攻撃パラメータ等をテキストで保存・読み込みする。
// ============================================================================

#pragma once
// ================================================================
// エディターとゲームの両方が使う設定データ。
// 必ず同じファイルを両方のプロジェクトに入れる。
// Novice・ImGuiに依存せず、ゲームのフレーム中にはファイルを読まない。
// 座標: 左下(0,0)、右上(1280,720)。Yのプラス方向が上。
// ================================================================
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <string>

struct BattlePoint { float x = 0.0f; float y = 0.0f; };

struct BossBattleSettings {
    static constexpr int kMaxAnchors = 8;
    static constexpr int kArmorHooks = 4;
    static constexpr const char* kFilename = "bossbattle_stage1.txt";
    int anchorCount = 6;
    std::array<BattlePoint, kMaxAnchors> anchors = { {{155.0f,320.0f},{450.0f,530.0f},{780.0f,360.0f},{1090.0f,530.0f},{270.0f,590.0f},{1180.0f,300.0f}} };
    BattlePoint bossSpawn = { 860.0f,510.0f };
    // 装甲フックはボス中心からの相対位置。ボスが動いても追従する。
    std::array<BattlePoint, kArmorHooks> armorHooks = { {{-108.0f,108.0f},{108.0f,108.0f},{-108.0f,-108.0f},{108.0f,-108.0f}} };
    // 3枚剥がした後のコア判定。ボス中心からの相対座標/半径。
    BattlePoint coreOffset = { 0.0f,0.0f };
    float coreRadius = 85.0f;
    // 5つの攻撃の速度・予兆と反撃可能時間。初期値は旧BossStage1Tuningに合わせた。
    float rushSpeed = 22.0f, rushAccel = 1.9f;
    int rushWarning = 37, rushDizzy = 78;
    float shotPredict = 9.0f;
    int shotWarning = 30;
    float pressSpeed = 42.0f, pressAccel = 2.2f;
    int pressWarning = 42, pressStun = 47;
    float topInitialSpeed = 25.0f;
    int topDuration = 120; // 旧設定との互換性用。実際の終了判定は帰還到着を優先。
    float topGravity = 1.45f;        // 落下中の重力（+Y上向き）
    float topBounceHeight = 5.0f;    // 床で回転する時の上下幅
    float topReturnAccel = 0.055f;  // 反射後の速度変化率
    float topHitRadius = 80.0f;      // プレイヤーへの攻撃半径
    int topMaxLifetime = 720;       // 異常時の安全停止時間

    float yoyoRange = 490.0f;
    int yoyoDuration = 98, toyWarning = 34;
    int idleFrames = 30;
    // ボス飛行AI。速度と目的地の変更間隔をゲーム中に調整できる。
    float flightSpeed = 4.1f;
    float flightSpeedPhase2 = 5.6f;
    int flightInterval = 78;
    int flightIntervalPhase2 = 55;
    // 部位のグリップポイント。ボス中心からの相対座標（+Yが上）。
    // 0=本体、1～3=プロペラ / 追加0～3=ジェット、4=パイプ、5=目
    std::array<BattlePoint, 4> partGrips = { {{0,0},{-112,112},{0,148},{112,112}} };
    std::array<BattlePoint, 6> accessoryGrips = { {{-132,-66},{-108,18},{129,25},{130,-73},{51,-115},{0,4}} };

    // GUIで調整するHPとダメージ。V1〜V3ではこの初期値を使う。
    int bossMaxHp = 190;
    int playerMaxHp = 100;
    int phase2HpPercent = 50;
    int normalDamage = 8;
    int spinDamage = 14;
    int grappleDamage = 20;
    int mashDamage = 8;
    int incomingDamagePercent = 100;

    // 設定値が想定範囲内か確認する。壊れた設定を採用しないための判定。
    bool Valid() const {
        const auto inside = [](BattlePoint p) {
            return std::isfinite(p.x) && std::isfinite(p.y) && p.x >= 30.0f && p.x <= 1250.0f && p.y >= 70.0f && p.y <= 680.0f;
            };
        if (anchorCount < 1 || anchorCount > kMaxAnchors || !inside(bossSpawn)) return false;
        for (int i = 0;i < anchorCount;++i) if (!inside(anchors[i]))return false;
        for (auto p : armorHooks) if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
            p.x < -128 || p.x > 128 || p.y < -128 || p.y > 128) return false;
        if (!std::isfinite(coreOffset.x) || !std::isfinite(coreOffset.y) ||
            coreOffset.x < -100 || coreOffset.x > 100 ||
            coreOffset.y < -100 || coreOffset.y > 100 ||
            !std::isfinite(coreRadius) || coreRadius < 24 || coreRadius > 120)return false;
        return std::isfinite(rushSpeed) && rushSpeed >= 5 && rushSpeed <= 50 &&
            std::isfinite(rushAccel) && rushAccel >= 0.1f && rushAccel <= 10 &&
            rushWarning >= 8 && rushWarning <= 180 && rushDizzy >= 0 && rushDizzy <= 240 &&
            std::isfinite(shotPredict) && shotPredict >= 0 && shotPredict <= 40 &&
            shotWarning >= 8 && shotWarning <= 180 &&
            std::isfinite(pressSpeed) && pressSpeed >= 5 && pressSpeed <= 80 &&
            std::isfinite(pressAccel) && pressAccel >= 0.1f && pressAccel <= 10 &&
            pressWarning >= 8 && pressWarning <= 180 && pressStun >= 0 && pressStun <= 180 &&
            std::isfinite(topInitialSpeed) && topInitialSpeed >= 3 && topInitialSpeed <= 45 &&
            topDuration >= 30 && topDuration <= 300 &&
            std::isfinite(topGravity) && topGravity >= 0.1f && topGravity <= 8.0f &&
            std::isfinite(topBounceHeight) && topBounceHeight >= 0.0f && topBounceHeight <= 30.0f &&
            std::isfinite(topReturnAccel) && topReturnAccel >= 0.005f && topReturnAccel <= 0.5f &&
            std::isfinite(topHitRadius) && topHitRadius >= 20.0f && topHitRadius <= 128.0f &&
            topMaxLifetime >= 180 && topMaxLifetime <= 1800 &&
            std::isfinite(yoyoRange) && yoyoRange >= 100 && yoyoRange <= 900 &&
            yoyoDuration >= 30 && yoyoDuration <= 300 && toyWarning >= 8 && toyWarning <= 180 &&
            idleFrames >= 8 && idleFrames <= 180 &&
            std::isfinite(flightSpeed) && flightSpeed >= 0.1f && flightSpeed <= 30.0f &&
            std::isfinite(flightSpeedPhase2) && flightSpeedPhase2 >= 0.1f && flightSpeedPhase2 <= 35.0f &&
            flightInterval >= 10 && flightInterval <= 360 &&
            flightIntervalPhase2 >= 10 && flightIntervalPhase2 <= 360 &&
            bossMaxHp >= 1 && bossMaxHp <= 10000 &&
            playerMaxHp >= 1 && playerMaxHp <= 1000 &&
            phase2HpPercent >= 1 && phase2HpPercent <= 99 &&
            normalDamage >= 0 && normalDamage <= 500 &&
            spinDamage >= 0 && spinDamage <= 500 &&
            grappleDamage >= 0 && grappleDamage <= 500 &&
            mashDamage >= 0 && mashDamage <= 500 &&
            incomingDamagePercent >= 0 && incomingDamagePercent <= 500 &&
            validGripPoints();
    }
    bool validGripPoints() const {
        for (const BattlePoint& pt : partGrips)
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || pt.x < -320 || pt.x > 320 || pt.y < -320 || pt.y > 320) return false;
        for (const BattlePoint& pt : accessoryGrips)
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || pt.x < -320 || pt.x > 320 || pt.y < -320 || pt.y > 320) return false;
        return true;
    }
    // エディター設定をファイルへ保存する。
    bool Save(const char* path = kFilename) const {
        if (!Valid())return false;
        std::ofstream out(path, std::ios::trunc);
        if (!out)return false;
        out << "PORTAL_BOSS_BATTLE_V5\n" << std::setprecision(9);
        out << "anchors " << anchorCount << '\n';
        for (int i = 0;i < anchorCount;++i) out << "anchor " << anchors[i].x << ' ' << anchors[i].y << '\n';
        out << "spawn " << bossSpawn.x << ' ' << bossSpawn.y << '\n';
        for (auto p : armorHooks) out << "hook " << p.x << ' ' << p.y << '\n';
        out << "core " << coreOffset.x << ' ' << coreOffset.y << ' ' << coreRadius << '\n';
        out << "rush " << rushSpeed << ' ' << rushAccel << ' ' << rushWarning << ' ' << rushDizzy << '\n';
        out << "shot " << shotPredict << ' ' << shotWarning << '\n';
        out << "press " << pressSpeed << ' ' << pressAccel << ' ' << pressWarning << ' ' << pressStun << '\n';
        out << "top " << topInitialSpeed << ' ' << topDuration << '\n';
        out << "topPhysics " << topGravity << ' ' << topBounceHeight << ' '
            << topReturnAccel << ' ' << topHitRadius << ' ' << topMaxLifetime << '\n';
        out << "yoyo " << yoyoRange << ' ' << yoyoDuration << ' ' << toyWarning << '\n';
        out << "idle " << idleFrames << '\n';
        out << "flight " << flightSpeed << ' ' << flightSpeedPhase2 << ' ' << flightInterval << ' ' << flightIntervalPhase2 << '\n';
        for (const auto& pt : partGrips) out << "partGrip " << pt.x << ' ' << pt.y << '\n';
        for (const auto& pt : accessoryGrips) out << "extraGrip " << pt.x << ' ' << pt.y << '\n';
        out << "combat " << bossMaxHp << ' ' << playerMaxHp << ' ' << phase2HpPercent
            << ' ' << normalDamage << ' ' << spinDamage << ' ' << grappleDamage
            << ' ' << mashDamage << ' ' << incomingDamagePercent << '\n';
        return out.good();
    }

    // 失敗しても以前の設定を壊さない。欠落/古い形式/異常値は読み込まない。
// エディター設定をファイルから読み込む。失敗時は元の設定を守る。
    bool Load(const char* path = kFilename) {
        std::ifstream in(path);
        BossBattleSettings next;
        std::string word;
        const auto key = [&](const char* expected) { return bool(in >> word) && word == expected; };
        if (!(in >> word) || (word != "PORTAL_BOSS_BATTLE_V1" && word != "PORTAL_BOSS_BATTLE_V2" && word != "PORTAL_BOSS_BATTLE_V3" && word != "PORTAL_BOSS_BATTLE_V4" && word != "PORTAL_BOSS_BATTLE_V5")) return false;
        const bool legacy = word == "PORTAL_BOSS_BATTLE_V1";
        const bool v3 = word == "PORTAL_BOSS_BATTLE_V3" || word == "PORTAL_BOSS_BATTLE_V4" || word == "PORTAL_BOSS_BATTLE_V5";
        const bool v4 = word == "PORTAL_BOSS_BATTLE_V4" || word == "PORTAL_BOSS_BATTLE_V5";
        const bool v5 = word == "PORTAL_BOSS_BATTLE_V5";
        if (!key("anchors") || !(in >> next.anchorCount) ||
            next.anchorCount < 1 || next.anchorCount > kMaxAnchors)return false;
        for (int i = 0;i < next.anchorCount;++i)
            if (!key("anchor") || !(in >> next.anchors[i].x >> next.anchors[i].y))return false;
        if (!key("spawn") || !(in >> next.bossSpawn.x >> next.bossSpawn.y))return false;
        if (legacy) {
            // 旧3点データは四隅のネジ座標へ自動移行。保存はV2で行う。
            BattlePoint discard;
            for (int i = 0;i < 3;++i) if (!key("hook") || !(in >> discard.x >> discard.y)) return false;
            next.armorHooks = { {{-108,108},{108,108},{-108,-108},{108,-108}} };
        }
        else {
            for (auto& pt : next.armorHooks) if (!key("hook") || !(in >> pt.x >> pt.y))return false;
        }
        if (!key("core") || !(in >> next.coreOffset.x >> next.coreOffset.y >> next.coreRadius))return false;
        if (!key("rush") || !(in >> next.rushSpeed >> next.rushAccel >> next.rushWarning >> next.rushDizzy))return false;
        if (!key("shot") || !(in >> next.shotPredict >> next.shotWarning))return false;
        if (!key("press") || !(in >> next.pressSpeed >> next.pressAccel >> next.pressWarning >> next.pressStun))return false;
        if (!key("top") || !(in >> next.topInitialSpeed >> next.topDuration))return false;
        if (v5 && (!key("topPhysics") || !(in >> next.topGravity >> next.topBounceHeight
            >> next.topReturnAccel >> next.topHitRadius >> next.topMaxLifetime))) return false;
        if (!key("yoyo") || !(in >> next.yoyoRange >> next.yoyoDuration >> next.toyWarning))return false;
        if (!key("idle") || !(in >> next.idleFrames))return false;
        if (v3) {
            if (!key("flight") || !(in >> next.flightSpeed >> next.flightSpeedPhase2 >> next.flightInterval >> next.flightIntervalPhase2)) return false;
            for (auto& pt : next.partGrips) if (!key("partGrip") || !(in >> pt.x >> pt.y)) return false;
            for (auto& pt : next.accessoryGrips) if (!key("extraGrip") || !(in >> pt.x >> pt.y)) return false;
        }
        if (v4) {
            if (!key("combat") || !(in >> next.bossMaxHp >> next.playerMaxHp >> next.phase2HpPercent
                >> next.normalDamage >> next.spinDamage >> next.grappleDamage
                >> next.mashDamage >> next.incomingDamagePercent)) return false;
        }
        // 余分な値が入っている場合も不正として扱う。
        if (in >> word)return false;
        if (legacy) {
            // 旧V1は3杭・低いボス用。V2では新しい6杭＋上空のコプターを初期配置にする。
            const BossBattleSettings modern;
            next.anchorCount = modern.anchorCount;
            next.anchors = modern.anchors;
            next.bossSpawn = modern.bossSpawn;
            next.coreRadius = modern.coreRadius;
        }
        if (!next.Valid())return false;
        *this = next;
        return true;
    }
};
