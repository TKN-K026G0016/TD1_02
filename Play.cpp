#include "play.h"
#include "input.h"
#include "scene_manager.h"
#include "Camera.h"
#include "Stage.h"
#include <Novice.h>

static void Pause(void) {
    if (CheckInputAction(InputAction::Pause)) {
        ChangeScene(Scene::Pause);
    }
}

void InitPlay(void) {
    InitCamera();
    InitStage();
    UpdateCamera(); 
}

void UpdatePlay(void) {
    Pause();
    UpdateStage();
    UpdateCamera(); 
}

void DrawPlay(void) {
    
    DrawStage();
}
