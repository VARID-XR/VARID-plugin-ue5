// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "VARIDSceneViewExtension.h"
#include "VARIDRendering.h"
#include "VARIDModule.h"

#include "CoreMinimal.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneViewExtension.h"
#include "SceneView.h"
#include "SceneTextures.h"
#include "PostProcess/PostProcessInputs.h"
#include "Engine/Texture2DArray.h"

FVARIDSceneViewExtension::FVARIDSceneViewExtension(const FAutoRegister& AutoRegister, const EVARIDSamplerType InSamplerType)
	: FSceneViewExtensionBase(AutoRegister),
	Rendering(InSamplerType)
{
	UE_LOG(LogTemp, Display, TEXT("FVARIDSceneViewExtension::FVARIDSceneViewExtension"));
}

void FVARIDSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	// this method runs on the GAME THREAD before the VARID rendering is performed
	// It is here that we marshall the data from the game thread to the render thread.
	// We can use this method to set up any view specific parameters if needed.

	FVARIDEyeConditionState EyeState;

	switch (InView.StereoViewIndex)
	{
	case eSSE_MONOSCOPIC:
		EyeState = FVARIDModule::Get().GetMonoEyeConditionState();
		break;
	case eSSE_LEFT_EYE:
		EyeState = FVARIDModule::Get().GetLeftEyeConditionState();
		break;
	case eSSE_RIGHT_EYE:
		EyeState = FVARIDModule::Get().GetRightEyeConditionState();
		break;
	default:
		break;
	}

	// start marshalling ALL of the parameters to the render thread. 

	EVARIDEyeConditionType ActiveCondition = EyeState.ActiveCondition;

	// Very important to convert UTextures to FRHITextures

	UTexture2D* ScotomaTexture = EyeState.ScotomaTexture;
	FRHITexture* ScotomaRHI = ScotomaTexture && ScotomaTexture->GetResource() ? ScotomaTexture->GetResource()->TextureRHI : nullptr;

	UTexture2DArray* FloaterTexArray = EyeState.FloaterTextureArray;
	FRHITexture* FloatersRHI = FloaterTexArray && FloaterTexArray->GetResource() ? FloaterTexArray->GetResource()->TextureRHI : nullptr;

	FVector2f NormalizedGazePosition = EyeState.NormalizedGazePosition;

	uint8 NumFloaters = EyeState.NumFloaters;
	float FloaterSpeed = EyeState.FloaterSpeed;
	float FloaterScale = EyeState.FloaterScale;
	float BlurStrength = EyeState.BlurStrength;
	float ContrastReduction = EyeState.ContrastReduction;
	float GlareStrength = EyeState.GlareStrength;
	float BrightnessThreshold = EyeState.BrightnessThreshold;
	float FocalLength_CM = EyeState.FocalLength_CM;
	float Radius = EyeState.Radius;

	EVARIDColorVisionDeficiencyType CVDType = EyeState.CVDType;

	FVector2f AmplitudeVector = EyeState.AmplitudeVector;
	FVector2f FrequencyVector = EyeState.FrequencyVector;

	ENQUEUE_RENDER_COMMAND(VARIDParameters)(
		[
			this,
			ActiveCondition,
			FloatersRHI,
			NormalizedGazePosition,
			NumFloaters,
			FloaterSpeed,
			FloaterScale,
			BlurStrength,
			ContrastReduction,
			GlareStrength,
			BrightnessThreshold,
			CVDType,
			ScotomaRHI,
			FocalLength_CM,
			Radius,
			AmplitudeVector,
			FrequencyVector
		]
	(FRHICommandListImmediate& RHICmdList)
	{
		CachedRenderParams.ActiveCondition = ActiveCondition;
		CachedRenderParams.FloaterTextureArrayRHI = FloatersRHI;
		CachedRenderParams.NormalizedGazePosition = NormalizedGazePosition;
		CachedRenderParams.NumFloaters = NumFloaters;
		CachedRenderParams.FloaterSpeed = FloaterSpeed;
		CachedRenderParams.FloaterScale = FloaterScale;
		CachedRenderParams.BlurStrength = BlurStrength;
		CachedRenderParams.ContrastReduction = ContrastReduction;
		CachedRenderParams.GlareStrength = GlareStrength;
		CachedRenderParams.BrightnessThreshold = BrightnessThreshold;
		CachedRenderParams.CVDType = CVDType;
		CachedRenderParams.ScotomaTextureRHI = ScotomaRHI;
		CachedRenderParams.FocalLength_CM = FocalLength_CM;
		CachedRenderParams.Radius = Radius;
		CachedRenderParams.AmplitudeVector = AmplitudeVector;
		CachedRenderParams.FrequencyVector = FrequencyVector;
	});
}

void FVARIDSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass InPass, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (InPass == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FVARIDSceneViewExtension::PostProcessPassAfterTonemap_RenderThread));
	}
}

FScreenPassTexture FVARIDSceneViewExtension::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& InGraphBuilder, const FSceneView& InView, const FPostProcessMaterialInputs& InOutMaterialInputs)
{
	check(IsInRenderingThread());

	const FScreenPassTexture& SceneColor = FScreenPassTexture::CopyFromSlice(InGraphBuilder, InOutMaterialInputs.GetInput(EPostProcessMaterialInput::SceneColor));

	check(SceneColor.IsValid());

	if (!(SceneColor.Texture->Desc.Flags & ETextureCreateFlags::ShaderResource))
	{
		UE_LOG(LogTemp, Error, TEXT("FVARIDSceneViewExtension::PostProcessPassAfterTonemap_RenderThread: SceneColor is missing TexCreate_ShaderResource flag"));
		return SceneColor;
	}

	if (!SceneColor.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("FVARIDSceneViewExtension::PostProcessPassAfterTonemap_RenderThread: Invalid SceneColor"));
		return SceneColor;
	}

	//UE_LOG(LogTemp, Warning, TEXT("CameraConstrainedViewRect: %s"), *InView.CameraConstrainedViewRect.ToString());
	//UE_LOG(LogTemp, Warning, TEXT("UnscaledViewRect: %s"), *InView.UnscaledViewRect.ToString());
	//UE_LOG(LogTemp, Warning, TEXT("UnconstrainedViewRect: %s"), *InView.UnconstrainedViewRect.ToString());
	//UE_LOG(LogTemp, Warning, TEXT("SceneColor.ViewRect: %s"), *SceneColor.ViewRect.ToString());

	//if (MasterViewRect.Size() == FIntPoint::ZeroValue)
	//{
	//	UE_LOG(LogTemp, Error, TEXT("FVARIDSceneViewExtension::PostProcessPassAfterTonemap_RenderThread: ViewRect has zero size"));
	//	// This can happen in some cases like when the view is not valid or not rendered
	//	return SceneColor;
	//}

	RDG_EVENT_SCOPE(InGraphBuilder, "VARID Rendering");
	{
		// reuse or create a back buffer to render into

		FScreenPassRenderTarget BackBufferRenderTarget;
		FRenderTargetBinding BackBufferRenderTargetBinding;

		if (InOutMaterialInputs.OverrideOutput.IsValid())
		{
			// Scenario: VR
			// override

			BackBufferRenderTarget = InOutMaterialInputs.OverrideOutput;
			BackBufferRenderTargetBinding = BackBufferRenderTarget.GetRenderTargetBinding();
		}
		else
		{
			// Scenario: PIE / non VR / desktop
			// NOT overridden

			FRDGTextureDesc OutputDesc = SceneColor.Texture->Desc; 
			OutputDesc.Flags |= TexCreate_RenderTargetable;

			FRDGTexture* BackBufferRenderTargetTexture = InGraphBuilder.CreateTexture(OutputDesc, TEXT("VARID BackBufferRenderTargetTexture"));
			BackBufferRenderTarget = FScreenPassRenderTarget(BackBufferRenderTargetTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
			BackBufferRenderTargetBinding = BackBufferRenderTarget.GetRenderTargetBinding();
		}

		bool Result = false;

		switch (CachedRenderParams.ActiveCondition)
		{
		case EVARIDEyeConditionType::None:
		{
			Result = Rendering.DrawDebugGazePosition_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.NormalizedGazePosition);
			break;
		}
		case EVARIDEyeConditionType::DebugSolidColor:
		{
			Result = Rendering.DrawDebugSolidColor_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding);
			break;
		}
		case EVARIDEyeConditionType::DebugUVMap:
		{
			Result = Rendering.DrawDebugUVMap_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding);
			break;
		}
		case EVARIDEyeConditionType::DebugDepthMap:
		{
			Result = Rendering.DrawDebugDepthMap_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding);
			break;
		}
		case EVARIDEyeConditionType::DebugPassthrough:
		{
			Result = Rendering.DrawDebugPassthrough_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding);
			break;
		}
		case EVARIDEyeConditionType::DebugGazePosition:
		{
			Result = Rendering.DrawDebugGazePosition_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.NormalizedGazePosition);
			break;
		}
		case EVARIDEyeConditionType::Cataracts:
		{
			Result = Rendering.DrawCataracts_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.BlurStrength, CachedRenderParams.ContrastReduction, CachedRenderParams.BrightnessThreshold, CachedRenderParams.GlareStrength);
			break;
		}
		case EVARIDEyeConditionType::ColorVisionDeficiency:
		{
			Result = Rendering.DrawColorVisionDeficiency_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.CVDType);
			break;
		}
		case EVARIDEyeConditionType::DiabeticRetinopathy:
		{
			Result = Rendering.DrawDiabeticRetinopathy_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.FloaterTextureArrayRHI, CachedRenderParams.NormalizedGazePosition, CachedRenderParams.NumFloaters, CachedRenderParams.FloaterSpeed, CachedRenderParams.FloaterScale, CachedRenderParams.BlurStrength, CachedRenderParams.ContrastReduction);
			break;
		}
		case EVARIDEyeConditionType::Glaucoma:
		{
			Result = Rendering.DrawGlaucoma_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.ScotomaTextureRHI, CachedRenderParams.NormalizedGazePosition);
			break;
		}
		case EVARIDEyeConditionType::Hyperopia:
		{
			Result = Rendering.DrawHyperopia_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.FocalLength_CM, CachedRenderParams.BlurStrength);
			break;
		}
		case EVARIDEyeConditionType::MacularDegeneration:
		{
			Result = Rendering.DrawMacularDegeneration_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.NormalizedGazePosition, CachedRenderParams.Radius, CachedRenderParams.BlurStrength);
			break;
		}
		case EVARIDEyeConditionType::Myopia:
		{
			Result = Rendering.DrawMyopia_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.FocalLength_CM, CachedRenderParams.BlurStrength);
			break;
		}
		case EVARIDEyeConditionType::Nystagmus:
		{
			Result = Rendering.DrawNystagmus_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.FrequencyVector, CachedRenderParams.AmplitudeVector);
			break;
		}
		case EVARIDEyeConditionType::RetinitisPigmentosa:
		{
			Result = Rendering.DrawRetinitisPigmentosa_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.NormalizedGazePosition, CachedRenderParams.Radius);
			break;
		}
		default:
			Result = false;
			break;
		}

		if (!Result)
		{
			Rendering.DrawDebugGazePosition_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTargetBinding, CachedRenderParams.NormalizedGazePosition);
		}

		return MoveTemp(BackBufferRenderTarget);
	}
}
