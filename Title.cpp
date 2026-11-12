#include "title.h"
#include "input.h"
#include "scene_manager.h"
#include "se.h"

#include <Novice.h>


void InitTitle(void) {

}

void UpdateTitle(void) {
	if (CheckInputAction(InputAction::Confirm)) {
		if (GetNowModeFade() != ModeFade::Standby) {
			return;
		}

		ChangeModeFade(ModeFade::FadeOut);
		ChangeFadeTarget(FadeTarget::Play);
		PlaySe(SeType::Sample);
	}
}

void DrawTitle(void) {

#ifdef _DEBUG

	Novice::ScreenPrintf(20, 20, "TitleScene");

#endif // _DEBUG


}