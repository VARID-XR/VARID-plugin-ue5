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
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"

FVARIDSceneViewExtension::EVARIDCachedEye FVARIDSceneViewExtension::GetCachedEyeForStereoViewIndex(int32 StereoViewIndex)
{
	switch (StereoViewIndex)
	{
	case eSSE_LEFT_EYE:
	case eSSE_LEFT_EYE_SIDE:
		return EVARIDCachedEye::Left;
	case eSSE_RIGHT_EYE:
	case eSSE_RIGHT_EYE_SIDE:
		return EVARIDCachedEye::Right;
	case eSSE_MONOSCOPIC:
	default:
		return EVARIDCachedEye::Mono;
	}
}

FVARIDSceneViewExtension::FVARIDRenderParameters FVARIDSceneViewExtension::MakeRenderParameters_GameThread(const FVARIDEyeConditionState& EyeState)
{
	FVARIDRenderParameters RenderParams;

	RenderParams.ActiveCondition = EyeState.ActiveCondition;

	// Keep a ref-counted RHI handle so queued render work cannot outlive the texture resource.
	UTexture2D* ScotomaTexture = EyeState.ScotomaTexture.Get();
	RenderParams.ScotomaTextureRHI = ScotomaTexture && ScotomaTexture->GetResource() ? ScotomaTexture->GetResource()->TextureRHI : nullptr;

	UTexture2DArray* FloaterTexArray = EyeState.FloaterTextureArray.Get();
	RenderParams.FloaterTextureArrayRHI = FloaterTexArray && FloaterTexArray->GetResource() ? FloaterTexArray->GetResource()->TextureRHI : nullptr;

	RenderParams.NormalizedGazePosition = EyeState.NormalizedGazePosition;
	RenderParams.NumFloaters = EyeState.NumFloaters;
	RenderParams.FloaterSpeed = EyeState.FloaterSpeed;
	RenderParams.FloaterScale = EyeState.FloaterScale;
	RenderParams.BlurStrength = EyeState.BlurStrength;
	RenderParams.ContrastReduction = EyeState.ContrastReduction;
	RenderParams.GlareStrength = EyeState.GlareStrength;
	RenderParams.BrightnessThreshold = EyeState.BrightnessThreshold;
	RenderParams.CVDType = EyeState.CVDType;
	RenderParams.FocalLength_CM = EyeState.FocalLength_CM;
	RenderParams.Radius = EyeState.Radius;
	RenderParams.AmplitudeVector = EyeState.AmplitudeVector;
	RenderParams.FrequencyVector = EyeState.FrequencyVector;

	return RenderParams;
}

bool FVARIDSceneViewExtension::IsActive(const FVARIDRenderParameters& RenderParams)
{
	return RenderParams.ActiveCondition != EVARIDEyeConditionType::None;
}

FVARIDSceneViewExtension::FVARIDRenderParameters& FVARIDSceneViewExtension::GetCachedRenderParams(EVARIDCachedEye CachedEye)
{
	switch (CachedEye)
	{
	case EVARIDCachedEye::Left:
		return CachedLeftRenderParams;
	case EVARIDCachedEye::Right:
		return CachedRightRenderParams;
	case EVARIDCachedEye::Mono:
	default:
		return CachedMonoRenderParams;
	}
}

const FVARIDSceneViewExtension::FVARIDRenderParameters& FVARIDSceneViewExtension::GetCachedRenderParams(EVARIDCachedEye CachedEye) const
{
	switch (CachedEye)
	{
	case EVARIDCachedEye::Left:
		return CachedLeftRenderParams;
	case EVARIDCachedEye::Right:
		return CachedRightRenderParams;
	case EVARIDCachedEye::Mono:
	default:
		return CachedMonoRenderParams;
	}
}

const FVARIDSceneViewExtension::FVARIDRenderParameters& FVARIDSceneViewExtension::ResolveCachedRenderParams(int32 StereoViewIndex) const
{
	const EVARIDCachedEye CachedEye = GetCachedEyeForStereoViewIndex(StereoViewIndex);
	const FVARIDRenderParameters& EyeRenderParams = GetCachedRenderParams(CachedEye);

	if (CachedEye != EVARIDCachedEye::Mono && !IsActive(EyeRenderParams) && IsActive(CachedMonoRenderParams))
	{
		return CachedMonoRenderParams;
	}

	return EyeRenderParams;
}

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

	const FVARIDRenderParameters MonoRenderParams = MakeRenderParameters_GameThread(FVARIDModule::Get().GetMonoEyeConditionState());
	const FVARIDRenderParameters LeftRenderParams = MakeRenderParameters_GameThread(FVARIDModule::Get().GetLeftEyeConditionState());
	const FVARIDRenderParameters RightRenderParams = MakeRenderParameters_GameThread(FVARIDModule::Get().GetRightEyeConditionState());

	ENQUEUE_RENDER_COMMAND(VARIDParameters)(
		[
			this,
			MonoRenderParams,
			LeftRenderParams,
			RightRenderParams
		]
	(FRHICommandListImmediate& RHICmdList)
	{
		CachedMonoRenderParams = MonoRenderParams;
		CachedLeftRenderParams = LeftRenderParams;
		CachedRightRenderParams = RightRenderParams;
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
		const FVARIDRenderParameters& RenderParams = ResolveCachedRenderParams(InView.StereoViewIndex);

		if (RenderParams.ActiveCondition == EVARIDEyeConditionType::None)
		{
			return SceneColor;
		}

		// reuse or create a back buffer to render into

		FScreenPassRenderTarget BackBufferRenderTarget;

		if (InOutMaterialInputs.OverrideOutput.IsValid())
		{
			// Scenario: VR
			// override

			BackBufferRenderTarget = InOutMaterialInputs.OverrideOutput;
		}
		else
		{
			// Scenario: PIE / non VR / desktop
			// NOT overridden

			FRDGTextureDesc OutputDesc = SceneColor.Texture->Desc;
			OutputDesc.Flags |= TexCreate_RenderTargetable;

			FRDGTexture* BackBufferRenderTargetTexture = InGraphBuilder.CreateTexture(OutputDesc, TEXT("VARID BackBufferRenderTargetTexture"));
			BackBufferRenderTarget = FScreenPassRenderTarget(BackBufferRenderTargetTexture, SceneColor.ViewRect, ERenderTargetLoadAction::ENoAction);
		}

		bool Result = false;

		switch (RenderParams.ActiveCondition)
		{
		case EVARIDEyeConditionType::DebugSolidColor:
		{
			Result = Rendering.DrawDebugSolidColor_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget);
			break;
		}
		case EVARIDEyeConditionType::DebugUVMap:
		{
			Result = Rendering.DrawDebugUVMap_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget);
			break;
		}
		case EVARIDEyeConditionType::DebugDepthMap:
		{
			Result = Rendering.DrawDebugDepthMap_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget);
			break;
		}
		case EVARIDEyeConditionType::DebugPassthrough:
		{
			Result = Rendering.DrawDebugPassthrough_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget);
			break;
		}
		case EVARIDEyeConditionType::DebugGazePosition:
		{
			Result = Rendering.DrawDebugGazePosition_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.NormalizedGazePosition);
			break;
		}
		case EVARIDEyeConditionType::Cataracts:
		{
			Result = Rendering.DrawCataracts_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.BlurStrength, RenderParams.ContrastReduction, RenderParams.BrightnessThreshold, RenderParams.GlareStrength);
			break;
		}
		case EVARIDEyeConditionType::ColorVisionDeficiency:
		{
			Result = Rendering.DrawColorVisionDeficiency_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.CVDType);
			break;
		}
		case EVARIDEyeConditionType::DiabeticRetinopathy:
		{
			Result = Rendering.DrawDiabeticRetinopathy_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.FloaterTextureArrayRHI.GetReference(), RenderParams.NumFloaters, RenderParams.FloaterSpeed, RenderParams.FloaterScale, RenderParams.BlurStrength, RenderParams.ContrastReduction);
			break;
		}
		case EVARIDEyeConditionType::Glaucoma:
		{
			Result = Rendering.DrawGlaucoma_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.ScotomaTextureRHI.GetReference(), RenderParams.NormalizedGazePosition);
			break;
		}
		case EVARIDEyeConditionType::Hyperopia:
		{
			Result = Rendering.DrawHyperopia_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.FocalLength_CM, RenderParams.BlurStrength);
			break;
		}
		case EVARIDEyeConditionType::MacularDegeneration:
		{
			Result = Rendering.DrawMacularDegeneration_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.NormalizedGazePosition, RenderParams.Radius, RenderParams.BlurStrength);
			break;
		}
		case EVARIDEyeConditionType::Myopia:
		{
			Result = Rendering.DrawMyopia_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.FocalLength_CM, RenderParams.BlurStrength);
			break;
		}
		case EVARIDEyeConditionType::Nystagmus:
		{
			Result = Rendering.DrawNystagmus_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.FrequencyVector, RenderParams.AmplitudeVector);
			break;
		}
		case EVARIDEyeConditionType::RetinitisPigmentosa:
		{
			Result = Rendering.DrawRetinitisPigmentosa_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.NormalizedGazePosition, RenderParams.Radius);
			break;
		}
		default:
			Result = false;
			break;
		}

		if (!Result)
		{
			Rendering.DrawDebugGazePosition_RenderThread(InGraphBuilder, SceneColor, InView, BackBufferRenderTarget, RenderParams.NormalizedGazePosition);
		}

		return MoveTemp(BackBufferRenderTarget);
	}
}
