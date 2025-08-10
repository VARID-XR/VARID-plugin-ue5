// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "SceneViewExtension.h"
#include "VARIDRendering.h"

class FTextureResource;

class FVARIDSceneViewExtension : public FSceneViewExtensionBase
{
public:

	FVARIDSceneViewExtension(const FAutoRegister& AutoRegister, const EVARIDSamplerType InSamplerType);

	// pure virtual function overrides
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override; // implemented. Responsible for copying params from from game thread to render thread
	virtual void SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InViewInfo) override {};
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void PostCreateSceneRenderer(const FSceneViewFamily& InViewFamily, ISceneRenderer* Renderer) override {};

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {};
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {};
	virtual void PreInitViews_RenderThread(FRDGBuilder& GraphBuilder) override {};
	virtual void PreRenderBasePass_RenderThread(FRDGBuilder& GraphBuilder, bool bDepthBufferIsPopulated) override {};
	virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override {};
	virtual void PostRenderBasePassMobile_RenderThread(FRHICommandList& RHICmdList, FSceneView& InView) override {};
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {};
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {};
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs) override {};
	virtual int32 GetPriority() const { return 0; }
	
	/**
	* This will be called at the beginning of post processing to make sure that each view extension gets a chance to subscribe to an after pass event.
	*  - The pass MUST write to the override output texture if it is active (this occurs when the pass is the last in the post processing chain writing to the back buffer).
	*    For performance reasons it is recommended to only subscribe to a pass when the pass will produce a GPU resource. Calling
	*/
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

	// VARID main render method
	FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs);

private:

	struct FVARIDRenderParameters
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

		FRHITexture* ScotomaTextureRHI = nullptr;

		FRHITexture* FloaterTextureArrayRHI = nullptr;
		uint8 NumFloaters = 0.0f;
		float FloaterSpeed = 0.0f;
		float FloaterScale = 0.0f;
	};

	// Local cached copy of the data. Purely used by scene view extension render threads
	FVARIDRenderParameters CachedRenderParams;


	FVARIDRendering Rendering;
};