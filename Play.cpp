#include "play.h"
#include "input.h"
#include "scene_manager.h"
#include "Camera.h"


#include "Map.h"
#include "Stage.h"

#include <Novice.h>

static void Pause(void) {
	if (CheckInputAction(InputAction::Pause)) {
		ChangeScene(Scene::Pause);
	}
}


void InitPlay(void) {
	InitCamera();

	InitMap();
	InitStage();
}

void UpdatePlay(void) {
	Pause();

	UpdateCamera();

	UpdateMap();
	UpdateStage();
}

void DrawPlay(void) {
	DrawStage();
	DrawMap();

#ifdef _DEBUG

	//Novice::ScreenPrintf(20, 20, "PlayScene");

#endif // _DEBUG
}