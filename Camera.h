#pragma once
#include "Matrix.h"

void InitCamera(void);
void UpdateCamera(void);

float GetCameraZoom(void);

Matrix3x3 GetWorldToScreenMatrix(void);