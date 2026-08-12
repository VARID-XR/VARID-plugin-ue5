// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "VARIDBlueprintFunctionLibrary.h"
#include "VARIDModule.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"

namespace
{
	void ClearTextureRefs(FVARIDEyeConditionState& State)
	{
		State.ScotomaTexture = nullptr;
		State.FloaterTextureArray = nullptr;
	}
}

void UVARIDBlueprintFunctionLibrary::BeginRendering(EVARIDSamplerType InSamplerType)
{
	FVARIDModule::Get().BeginRendering(InSamplerType);
}

void UVARIDBlueprintFunctionLibrary::EndRendering()
{
	FVARIDModule::Get().EndRendering();
}

void UVARIDBlueprintFunctionLibrary::SetNormalizedGazePosition(EVARIDEyeType Eye, FVector2f NormalizedGazePosition)
{
	FVARIDModule::Get().GetEyeConditionState(Eye).NormalizedGazePosition = NormalizedGazePosition;
}

FVector2f UVARIDBlueprintFunctionLibrary::GetNormalizedMousePosition(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerController is null"));
		return FVector2f::ZeroVector;
	}

	float MouseX, MouseY;
	if (PlayerController->GetMousePosition(MouseX, MouseY))
	{
		//UE_LOG(LogTemp, Display, TEXT("Mouse Position: X = %.1f, Y = %.1f"), MouseX, MouseY);

		if (!GEngine || !GEngine->GameViewport)
		{
			UE_LOG(LogTemp, Warning, TEXT("GameViewport is null"));
			return FVector2f::ZeroVector;
		}

		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);

		//UE_LOG(LogTemp, Display, TEXT("Viewport Size: X = %.1f, Y = %.1f"), ViewportSize.X, ViewportSize.Y);

		if (ViewportSize.X > 0 && ViewportSize.Y > 0)
		{
			float NormalizedX = -1.0f + 2.0f * (MouseX / ViewportSize.X);
			float NormalizedY = -1.0f + 2.0f * (MouseY / ViewportSize.Y);

			//UE_LOG(LogTemp, Display, TEXT("Normalized Mouse (NDC): X = %.3f, Y = %.3f"), NormalizedX, NormalizedY);

			return FVector2f(NormalizedX, NormalizedY);
		}
	}

	return FVector2f::ZeroVector;
}

void UVARIDBlueprintFunctionLibrary::SetDebugSolidColor(EVARIDEyeType Eye)
{
	FVARIDEyeConditionState& Params = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(Params);
	Params.ActiveCondition = EVARIDEyeConditionType::DebugSolidColor;
}

void UVARIDBlueprintFunctionLibrary::SetDebugUVMap(EVARIDEyeType Eye)
{
	FVARIDEyeConditionState& Params = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(Params);
	Params.ActiveCondition = EVARIDEyeConditionType::DebugUVMap;
}

void UVARIDBlueprintFunctionLibrary::SetDebugDepthMap(EVARIDEyeType Eye)
{
	FVARIDEyeConditionState& Params = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(Params);
	Params.ActiveCondition = EVARIDEyeConditionType::DebugDepthMap;
}

void UVARIDBlueprintFunctionLibrary::SetDebugPassthrough(EVARIDEyeType Eye)
{
	FVARIDEyeConditionState& Params = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(Params);
	Params.ActiveCondition = EVARIDEyeConditionType::DebugPassthrough;
}

void UVARIDBlueprintFunctionLibrary::SetDebugGazePosition(EVARIDEyeType Eye)
{
	FVARIDEyeConditionState& Params = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(Params);
	Params.ActiveCondition = EVARIDEyeConditionType::DebugGazePosition;
}

void UVARIDBlueprintFunctionLibrary::ClearAllEyeConditions()
{
	FVARIDModule& Module = FVARIDModule::Get();
	FVARIDEyeConditionState& LeftEyeState = Module.GetLeftEyeConditionState();
	FVARIDEyeConditionState& RightEyeState = Module.GetRightEyeConditionState();
	FVARIDEyeConditionState& MonoEyeState = Module.GetMonoEyeConditionState();

	LeftEyeState.ActiveCondition = EVARIDEyeConditionType::None;
	RightEyeState.ActiveCondition = EVARIDEyeConditionType::None;
	MonoEyeState.ActiveCondition = EVARIDEyeConditionType::None;

	ClearTextureRefs(LeftEyeState);
	ClearTextureRefs(RightEyeState);
	ClearTextureRefs(MonoEyeState);
}

void UVARIDBlueprintFunctionLibrary::SetCataractsParams(EVARIDEyeType Eye, float ContrastReduction, float BlurStrength, float GlareStrength, float BrightnessThreshold)
{
	ContrastReduction = FMath::Clamp(ContrastReduction, 0.0f, 1.0f);
	BlurStrength = FMath::Clamp(BlurStrength, 0.0f, 10.0f);
	GlareStrength = FMath::Clamp(GlareStrength, 0.0f, 10.0f);
	BrightnessThreshold = FMath::Clamp(BrightnessThreshold, 0.0f, 1.0f);

	FVARIDEyeConditionState& State = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(State);
	State.ActiveCondition = EVARIDEyeConditionType::Cataracts;
	State.ContrastReduction = ContrastReduction;
	State.BlurStrength = BlurStrength;
	State.GlareStrength = GlareStrength;
	State.BrightnessThreshold = BrightnessThreshold;
}

void UVARIDBlueprintFunctionLibrary::SetColorVisionDeficiencyParams(EVARIDEyeType Eye, EVARIDColorVisionDeficiencyType CVDType)
{
	FVARIDEyeConditionState& State = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(State);
	State.ActiveCondition = EVARIDEyeConditionType::ColorVisionDeficiency;
	State.CVDType = CVDType;
}

void UVARIDBlueprintFunctionLibrary::SetDiabeticRetinopathyParams(EVARIDEyeType Eye, float ContrastReduction, float BlurStrength, UTexture2DArray* FloaterTextureArray, uint8 NumFloaters, float FloaterSpeed, float FloaterScale)
{
	ContrastReduction = FMath::Clamp(ContrastReduction, 0.0f, 1.0f);
	BlurStrength = FMath::Clamp(BlurStrength, 0.0f, 10.0f);
	NumFloaters = FMath::Clamp<uint8>(NumFloaters, 0, 32);
	FloaterSpeed = FMath::Clamp(FloaterSpeed, 0.0f, 1.0f);
	FloaterScale = FMath::Clamp(FloaterScale, 0.1f, 1.0f);

	FVARIDModule& Module = FVARIDModule::Get();
	Module.EnsureTextureReferencer();

	FVARIDEyeConditionState& State = Module.GetEyeConditionState(Eye);
	State.ScotomaTexture = nullptr;
	State.ActiveCondition = EVARIDEyeConditionType::DiabeticRetinopathy;
	State.ContrastReduction = ContrastReduction;
	State.BlurStrength = BlurStrength;
	State.FloaterTextureArray = FloaterTextureArray;
	State.NumFloaters = NumFloaters;
	State.FloaterSpeed = FloaterSpeed;
	State.FloaterScale = FloaterScale;
}

void UVARIDBlueprintFunctionLibrary::SetGlaucomaParams(EVARIDEyeType Eye, UTexture2D* ScotomaTexture)
{
	FVARIDModule& Module = FVARIDModule::Get();
	Module.EnsureTextureReferencer();

	FVARIDEyeConditionState& State = Module.GetEyeConditionState(Eye);
	State.FloaterTextureArray = nullptr;
	State.ActiveCondition = EVARIDEyeConditionType::Glaucoma;
	State.ScotomaTexture = ScotomaTexture;
}

void UVARIDBlueprintFunctionLibrary::SetHyperopiaParams(EVARIDEyeType Eye, float BlurStrength, float FocalLength)
{
	BlurStrength = FMath::Clamp(BlurStrength, 0.0f, 10.0f);
	FocalLength = FMath::Clamp(FocalLength, 0.0f, 1000.0f);

	FVARIDEyeConditionState& State = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(State);
	State.ActiveCondition = EVARIDEyeConditionType::Hyperopia;
	State.BlurStrength = BlurStrength;
	State.FocalLength_CM = FocalLength;
}

void UVARIDBlueprintFunctionLibrary::SetMacularDegenerationParams(EVARIDEyeType Eye, float BlurStrength, float Radius)
{
	BlurStrength = FMath::Clamp(BlurStrength, 0.0f, 10.0f);
	Radius = FMath::Clamp(Radius, 0.0f, 1.0f);

	FVARIDEyeConditionState& State = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(State);
	State.ActiveCondition = EVARIDEyeConditionType::MacularDegeneration;
	State.BlurStrength = BlurStrength;
	State.Radius = Radius;
}

void UVARIDBlueprintFunctionLibrary::SetMyopiaParams(EVARIDEyeType Eye, float BlurStrength, float FocalLength)
{
	BlurStrength = FMath::Clamp(BlurStrength, 0.0f, 10.0f);
	FocalLength = FMath::Clamp(FocalLength, 0.0f, 1000.0f);

	FVARIDEyeConditionState& State = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(State);
	State.ActiveCondition = EVARIDEyeConditionType::Myopia;
	State.BlurStrength = BlurStrength;
	State.FocalLength_CM = FocalLength;
}

void UVARIDBlueprintFunctionLibrary::SetNystagmusParams(EVARIDEyeType Eye, float AmplitudeX, float AmplitudeY, float FrequencyX, float FrequencyY)
{
	AmplitudeX = FMath::Clamp(AmplitudeX, 0.0f, 0.1f);
	AmplitudeY = FMath::Clamp(AmplitudeY, 0.0f, 0.1f);
	FrequencyX = FMath::Clamp(FrequencyX, 0.0f, 50.0f);
	FrequencyY = FMath::Clamp(FrequencyY, 0.0f, 50.0f);

	FVARIDEyeConditionState& State = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(State);
	State.ActiveCondition = EVARIDEyeConditionType::Nystagmus;
	State.AmplitudeVector = FVector2f(AmplitudeX, AmplitudeY);
	State.FrequencyVector = FVector2f(FrequencyX, FrequencyY);
}

void UVARIDBlueprintFunctionLibrary::SetRetinitisPigmentosaParams(EVARIDEyeType Eye, float Radius)
{
	Radius = FMath::Clamp(Radius, 0.0f, 2.0f);

	FVARIDEyeConditionState& State = FVARIDModule::Get().GetEyeConditionState(Eye);
	ClearTextureRefs(State);
	State.ActiveCondition = EVARIDEyeConditionType::RetinitisPigmentosa;
	State.Radius = Radius;
}
