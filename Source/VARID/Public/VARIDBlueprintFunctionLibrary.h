// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "VARIDEnums.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "VARIDBlueprintFunctionLibrary.generated.h"

UCLASS(BlueprintType)
class UVARIDBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, category = "VARID")
	static void BeginRendering(EVARIDSamplerType InSamplerType = EVARIDSamplerType::GaussianSampling);

	UFUNCTION(BlueprintCallable, category = "VARID")
	static void EndRendering();

public:
	UFUNCTION(BlueprintCallable, Category = "VARID")
    static void SetNormalizedGazePosition(EVARIDEyeType Eye = EVARIDEyeType::Mono, FVector2f NormalizedGazePosition = FVector2f());

	// BlueprintPure because it's a read-only query of the system state
	UFUNCTION(BlueprintPure, Category = "VARID")
	static FVector2f GetNormalizedMousePosition(APlayerController* PlayerController);

public:
	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetDebugSolidColor(EVARIDEyeType Eye = EVARIDEyeType::Mono);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetDebugUVMap(EVARIDEyeType Eye = EVARIDEyeType::Mono);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetDebugDepthMap(EVARIDEyeType Eye = EVARIDEyeType::Mono);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetDebugPassthrough(EVARIDEyeType Eye = EVARIDEyeType::Mono);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetDebugGazePosition(EVARIDEyeType Eye = EVARIDEyeType::Mono);

public:
	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void ClearAllEyeConditions();

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetCataractsParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0")) float ContrastReduction = 0.5f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "10.0")) float BlurStrength = 3.0f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "10.0")) float GlareStrength = 3.0f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0")) float BrightnessThreshold = 0.5f
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetColorVisionDeficiencyParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		EVARIDColorVisionDeficiencyType CVDType = EVARIDColorVisionDeficiencyType::Protanopia
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetDiabeticRetinopathyParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0")) float ContrastReduction = 0.5f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "10.0")) float BlurStrength = 2.0f,
		UTexture2DArray* FloaterTextureArray = nullptr,
		UPARAM(meta = (ClampMin = "0", ClampMax = "32")) uint8 NumFloaters = 0,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0")) float FloaterSpeed = 0.01f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0")) float FloaterScale = 0.01f
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetGlaucomaParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UTexture2D* ScotomaTexture = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetHyperopiaParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "10.0")) float BlurStrength = 3.0f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1000.0")) float FocalLength = 200.0f
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetMacularDegenerationParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "10.0")) float BlurStrength = 3.0f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0")) float Radius = 0.5f
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetMyopiaParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "10.0")) float BlurStrength = 3.0f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1000.0")) float FocalLength = 200.0f
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetNystagmusParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "0.1")) float AmplitudeX = 0.01f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "0.1")) float AmplitudeY = 0.02f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "50.0")) float FrequencyX = 10.0f,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "50.0")) float FrequencyY = 20.0f
	);

	UFUNCTION(BlueprintCallable, Category = "VARID")
	static void SetRetinitisPigmentosaParams(
		EVARIDEyeType Eye = EVARIDEyeType::Mono,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "2.0")) float Radius = 0.5f
	);

};