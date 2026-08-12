// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "ScreenPass.h"
#include "SceneRenderTargetParameters.h"
#include "VARIDEnums.h"

class FVARIDRendering
{
public:
	FVARIDRendering(EVARIDSamplerType InSamplerType);

private:
	const uint8 MAX_NUM_MIP_LEVELS = 10;
	const FGlobalShaderMap* GlobalShaderMap = nullptr;
	EVARIDSamplerType SamplerType = EVARIDSamplerType::PointSampling;

private: // HELPERS
	uint8 CalculateNumMips1D(int32 InValue);
	uint8 CalculateNumMips2D(FIntPoint InSize);
	uint8 CalculateNumMipsForBlur(FIntPoint InSize, float InMaxBlurStrength);

private: // COMPUTE
	FRDGTextureRef CreateBlurredTexture(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const EVARIDSamplerType SamplerType, const uint8 InNumMipsToGenerate);
	bool BuildSamplerPyramid_RenderThread(FRDGBuilder& InGraphBuilder, const FIntRect InViewRect, const FRDGTextureRef InTexture, FRDGTextureRef OutSamplerMipTexture, const FSceneView& InView, const uint8 InNumMips, FRHISamplerState* InSampler);
	bool BuildGaussianPyramid_RenderThread(FRDGBuilder& InGraphBuilder, const FIntRect InViewRect, const FRDGTextureRef InTexture, FRDGTextureRef OutGaussianMipTexture, const uint8 InNumMips);

public: // DEBUG
	bool DrawDebugPassthrough_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget);
	bool DrawDebugGazePosition_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InNormalizedGazePosition);
	bool DrawDebugSolidColor_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget);
	bool DrawDebugUVMap_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget);
	bool DrawDebugDepthMap_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget);

public:	// EYE CONDITIONS
	bool DrawCataracts_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const float InBlurStrength, const float InContrastReduction, const float InBrightThreshold, const float InGlareStrength);
	bool DrawColorVisionDeficiency_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const EVARIDColorVisionDeficiencyType InCVDType);
	bool DrawDiabeticRetinopathy_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, FRHITexture* InFloaterTextureArrayRHI, const uint8 InNumFloaters, const float InFloaterSpeed, const float InFloaterScale, const float InBlurStrength, const float InContrastReduction);
	bool DrawGlaucoma_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, FRHITexture* InScotomaTexture, const FVector2f InNormalizedGazePosition);
	bool DrawHyperopia_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const float InFocalLength_CM, const float InBlurStrength);
	bool DrawMacularDegeneration_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InNormalizedGazePosition, const float InRadius, const float InBlurStrength);
	bool DrawMyopia_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const float InFocalLength_CM, const float InBlurStrength);
	bool DrawNystagmus_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InFrequency, const FVector2f InAmplitude);
	bool DrawRetinitisPigmentosa_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InNormalizedGazePosition, const float InRadius);
};
