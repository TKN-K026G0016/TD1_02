#pragma once
#include "Vector2.h"
struct PortalCommands {
    float move = 0;
    Vector2 aim = {};
    bool aimExplicit = false; // 矢印キー/W・S/右スティックで意識して照準したか。A/D移動だけならfalse
    bool attack = false;      // パッド右スティックの倒し始め/8方向切替/自動連撃、またはキーボードJ
    bool attackHeld = false;  // 右スティックを倒している間 / キーボードJを押している間
    bool stickAction = false; // コントローラー右スティックでアクション中。短い入力は攻撃、ロック保持でグラップル
    bool jump = false;        // 左スティック上 / パッドA / キーボードSPACEの押し始め
    bool jumpHeld = false;    // ジャンプボタンを押し続けている間
    bool reset = false;
    bool connected = false;
};
class PortalInput {
public:
    PortalCommands Read();
private:
    bool previousAttack_ = false;
    bool previousRightHeld_ = false;
    int previousRightSector_ = 999;
    int rightHoldFrames_ = 0;
    bool previousJump_ = false;
    bool previousReset_ = false;
};
