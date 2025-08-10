// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VARIDEnums.generated.h"


UENUM(BlueprintType)
enum class EVARIDEyeType : uint8
{
	Mono	UMETA(DisplayName = "Mono", ToolTip = "Both eyes/single display"),
	Left	UMETA(DisplayName = "Stereo Left", ToolTip = "Stereo Left"),
	Right	UMETA(DisplayName = "Stereo Right", ToolTip = "Stereo Right")
};

UENUM(BlueprintType)
enum class EVARIDEyeConditionType : uint8
{
	None = 0				UMETA(DisplayName = "None", ToolTip = "No eye condition applied"),

	Cataracts				UMETA(DisplayName = "Cataracts", ToolTip = "Clouding of the lens causing blurred vision and glare"),
	ColorVisionDeficiency	UMETA(DisplayName = "Color Vision Deficiency", ToolTip = "Reduced ability to distinguish certain colors, typically red-green or blue-yellow"),
	DiabeticRetinopathy     UMETA(DisplayName = "Diabetic Retinopathy", ToolTip = "Damage to retinal blood vessels causing floaters, blurred vision, and scotomas"),
	Glaucoma				UMETA(DisplayName = "Glaucoma", ToolTip = "Progressive loss of peripheral vision due to optic nerve damage"),
	Hyperopia				UMETA(DisplayName = "Hyperopia", ToolTip = "Farsightedness; difficulty focusing on nearby objects"),
	MacularDegeneration     UMETA(DisplayName = "Macular Degeneration", ToolTip = "Loss of central vision due to retinal damage in the macula"),
	Myopia					UMETA(DisplayName = "Myopia", ToolTip = "Nearsightedness; distant objects appear blurry"),
	Nystagmus				UMETA(DisplayName = "Nystagmus", ToolTip = "Involuntary eye movements causing visual instability and motion blur"),
	RetinitisPigmentosa		UMETA(DisplayName = "Retinitis Pigmentosa", ToolTip = "Progressive tunnel vision and night blindness due to retinal degeneration"),

	DebugSolidColor			UMETA(DisplayName = "Debug Solid Color", ToolTip = "Debug mode for testing purposes"),
	DebugUVMap				UMETA(DisplayName = "Debug UV Map", ToolTip = "Debug mode for testing purposes"),
	DebugDepthMap			UMETA(DisplayName = "Debug Depth Map", ToolTip = "Debug mode for testing purposes"),
	DebugPassthrough		UMETA(DisplayName = "Debug Passthrough", ToolTip = "Debug mode for testing purposes"),
	DebugGazePosition		UMETA(DisplayName = "Debug Gaze Position", ToolTip = "Debug mode for testing purposes")
};

UENUM(BlueprintType)
enum class EVARIDColorVisionDeficiencyType : uint8
{
	None		   UMETA(DisplayName = "None", ToolTip = "No color deficiency"),
	Protanopia     UMETA(DisplayName = "Protanopia", ToolTip = "Protanope (red weak/blind) (2% of males, 0.01% of females)"),
	Deuteranopia   UMETA(DisplayName = "Deuteranopia", ToolTip = "Deuteranope (green weak/blind) (7% of males, 0.4% of females)"),
	Tritanopia     UMETA(DisplayName = "Tritanopia", ToolTip = "Tritanope (blue weak/blind) (0.0003% of males)")
};

UENUM(BlueprintType)
enum class EVARIDSamplerType : uint8
{
	PointSampling		UMETA(DisplayName = "PointSampling", ToolTip = "PointSampling"),
	BilinearSampling	UMETA(DisplayName = "BilinearSampling", ToolTip = "BilinearSampling"),
	GaussianSampling	UMETA(DisplayName = "GaussianSampling", ToolTip = "GaussianSampling")
};