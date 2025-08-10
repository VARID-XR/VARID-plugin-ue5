// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "VARIDEnums.h"

// Used by the game thread to store the current eye condition parameters
// All parameters combined for all eye conditions. 
// Many parameters overlap between conditions, so this is a single struct to hold them all.

struct FVARIDEyeConditionState
{
	EVARIDEyeConditionType ActiveCondition = EVARIDEyeConditionType::None;

	FVector2f NormalizedGazePosition = FVector2f::ZeroVector;

	float Radius = 0.0f;
	float FocalLength_CM = 0.0f;
	float BlurStrength = 0.0f;
	float ContrastReduction = 0.0f;
	float BrightnessThreshold = 0.0f;
	float GlareStrength = 0.0f;

	FVector2f FrequencyVector = FVector2f::ZeroVector;
	FVector2f AmplitudeVector = FVector2f::ZeroVector;

	EVARIDColorVisionDeficiencyType CVDType = EVARIDColorVisionDeficiencyType::None;

	UTexture2D* ScotomaTexture = nullptr;

	UTexture2DArray* FloaterTextureArray = nullptr;
	uint8 NumFloaters = 0.0f;
	float FloaterSpeed = 0.0f;
	float FloaterScale = 0.0f;
};