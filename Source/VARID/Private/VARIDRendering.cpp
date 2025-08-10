// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "VARIDRendering.h"
#include "VARIDModule.h"
#include "VARIDDebugShaders.h"
#include "VARIDEyeConditionShaders.h"
#include "VARIDComputeShaders.h"
#include "VARIDEnums.h"

#include "ScreenPass.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include <SystemTextures.h>

FVARIDRendering::FVARIDRendering(EVARIDSamplerType InSamplerType)
{
	GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	check(GlobalShaderMap); // Safe if this is constructed after shader init
	SamplerType = InSamplerType;
}

uint8 FVARIDRendering::CalculateNumMips1D(int32 InValue)
{
	uint8 numTimesHalved = 0;
	while (InValue > 1)
	{
		// could also be done by logarithm to base 2 rounded up/down to the next integer
		InValue = InValue >> 1;
		numTimesHalved++;
	}

	return numTimesHalved;
}

uint8 FVARIDRendering::CalculateNumMips2D(FIntPoint InSize)
{
	uint8 mipsWidth = CalculateNumMips1D(InSize.X);
	uint8 mipsHeight = CalculateNumMips1D(InSize.Y);
	uint8 NumberOfMipsToGenerate = mipsWidth > mipsHeight ? mipsWidth : mipsHeight;
	return FMath::Min(NumberOfMipsToGenerate, FVARIDRendering::MAX_NUM_MIP_LEVELS);
}

FRDGTextureRef FVARIDRendering::CreateBlurredTexture(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const EVARIDSamplerType InSamplerType)
{
	const FIntPoint ViewSize = InSceneColor.ViewRect.Size();
	const uint8 NumberOfMipsToGenerate = CalculateNumMips2D(ViewSize);

	ensureAlwaysMsgf(InSceneColor.Texture->Desc.ArraySize == 1, TEXT("InSceneColor is still arrayed (ArraySize=%d). Ensure CopyFromSlice is used in the scene view extension or bind FirstArraySlice."), InSceneColor.Texture->Desc.ArraySize);

	FRDGTextureDesc BlurredTextureDesc = FRDGTextureDesc::Create2D
	(
		ViewSize,
		InSceneColor.Texture->Desc.Format,
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV,
		NumberOfMipsToGenerate
	);

	FRDGTextureRef BlurredTexture = InGraphBuilder.CreateTexture(BlurredTextureDesc, TEXT("VARID BlurredTexture"));

	bool Result = false;

	switch (InSamplerType)
	{
	case EVARIDSamplerType::PointSampling:
		Result = BuildSamplerPyramid_RenderThread(InGraphBuilder, InSceneColor.ViewRect, InSceneColor.Texture, BlurredTexture, InView, NumberOfMipsToGenerate, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		break;
	case EVARIDSamplerType::BilinearSampling:
		Result = BuildSamplerPyramid_RenderThread(InGraphBuilder, InSceneColor.ViewRect, InSceneColor.Texture, BlurredTexture, InView, NumberOfMipsToGenerate, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		break;
	case EVARIDSamplerType::GaussianSampling:
		Result = BuildGaussianPyramid_RenderThread(InGraphBuilder, InSceneColor.ViewRect, InSceneColor.Texture, BlurredTexture, InView, NumberOfMipsToGenerate);
		break;
	default:
		break;
	}

	if (!Result)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to build blurred texture"), static_cast<uint8>(InSamplerType));
		return nullptr;
	}

	return BlurredTexture;
}

/************************************************************************/
// IMAGE PROCESSING COMPUTE SHADERS

bool FVARIDRendering::BuildSamplerPyramid_RenderThread(FRDGBuilder& InGraphBuilder, const FIntRect InViewRect, const FRDGTextureRef InTexture, FRDGTextureRef OutTexture, const FSceneView& InView, const uint8 InNumMips, FRHISamplerState* InSampler)
{
	check(InTexture);
	check(OutTexture);

	const FRDGTextureDesc& OutTextureDesc = OutTexture->Desc;
	TShaderMapRef<FVARIDBasicResampleCS> ResampleComputeShader(GlobalShaderMap);

	if (InNumMips > OutTextureDesc.NumMips)
	{
		UE_LOG(LogTemp, Error, TEXT("InNumMips (%d) is greater than OutTextureDesc.NumMips (%d)."), InNumMips, OutTextureDesc.NumMips);
		return false;
	}

	ensureAlways(OutTextureDesc.Extent == InViewRect.Size());

	// --- Mip 0: Direct Copy ---
	{
		const FIntPoint CopySize(FMath::Max(InViewRect.Width(), 1), FMath::Max(InViewRect.Height(), 1));

		FRHICopyTextureInfo CopyInfo;
		CopyInfo.NumMips = 1;
		CopyInfo.Size = FIntVector(CopySize.X, CopySize.Y, 1);
		CopyInfo.SourcePosition = FIntVector(InViewRect.Min.X, InViewRect.Min.Y, 0);
		CopyInfo.DestPosition = FIntVector(0, 0, 0);

		AddCopyTexturePass(InGraphBuilder, InTexture, OutTexture, CopyInfo);
	}

	// --- Remaining Mips: Downsample ---
	for (uint32 MipLevel = 1; MipLevel < InNumMips; ++MipLevel)
	{
		const FIntPoint DispatchSize(FMath::Max(InViewRect.Width() >> MipLevel, 1), FMath::Max(InViewRect.Height() >> MipLevel, 1));
		const FIntPoint DestTextureSize(FMath::Max(InViewRect.Width() >> MipLevel, 1), FMath::Max(InViewRect.Height() >> MipLevel, 1));
		const FVector2f TexelSize(1.0f / DestTextureSize.X, 1.0f / DestTextureSize.Y);

		// NOTE: The SRV mip level is different to UAV mip level - therefore the texture can be used as both an input and an output

		FVARIDBasicResampleCS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDBasicResampleCS::FParameters>();
		PassParameters->View = InView.ViewUniformBuffer;
		PassParameters->InTexelSize = TexelSize;
		PassParameters->InSampler = InSampler;
		PassParameters->InSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(OutTexture, MipLevel - 1));
		PassParameters->OutUAV = InGraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutTexture, MipLevel));

		FComputeShaderUtils::AddPass(
			InGraphBuilder,
			RDG_EVENT_NAME("VARID - Build Pyramid Copy - Downsample - MipLevel=%d", MipLevel),
			ResampleComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DispatchSize, FComputeShaderUtils::kGolden2DGroupSize));
	}

	return true;
}

bool FVARIDRendering::BuildGaussianPyramid_RenderThread(FRDGBuilder& InGraphBuilder, const FIntRect InViewRect, const FRDGTextureRef InTexture, const FRDGTextureRef OutTexture, const FSceneView& InView, const uint8 InNumMips)
{
	check(InTexture);
	check(OutTexture);

	const FRDGTextureDesc& OutTextureDesc = OutTexture->Desc;

	FRDGTextureRef BlurredMipTexture = InGraphBuilder.CreateTexture(OutTextureDesc, TEXT("BlurredMipTexture"));

	TShaderMapRef<FVARIDGaussianBlurCS> GaussianBlurComputeShader(GlobalShaderMap);
	TShaderMapRef<FVARIDBasicResampleCS> ResampleComputeShader(GlobalShaderMap);

	if (InNumMips > OutTextureDesc.NumMips)
	{
		UE_LOG(LogTemp, Error, TEXT("InNumMips (%d) is greater than OutTextureDesc.NumMips (%d)."), InNumMips, OutTextureDesc.NumMips);
		return false;
	}

	ensureAlways(OutTextureDesc.Extent == InViewRect.Size());

	// --- Mip 0: Direct Copy ---
	{
		const FIntPoint CopySize = InViewRect.Size();

		FRHICopyTextureInfo CopyInfo;
		CopyInfo.NumMips = 1;
		CopyInfo.Size = FIntVector(CopySize.X, CopySize.Y, 1);
		CopyInfo.SourcePosition = FIntVector(InViewRect.Min.X, InViewRect.Min.Y, 0);
		CopyInfo.DestPosition = FIntVector(0, 0, 0);

		AddCopyTexturePass(InGraphBuilder, InTexture, OutTexture, CopyInfo);
	}

	// --- Remaining Mips ---
	for (uint32 MipLevel = 1; MipLevel < OutTextureDesc.NumMips; ++MipLevel)
	{
		int32 LoResMipLevel = MipLevel;
		int32 HiResMipLevel = MipLevel - 1;

		const FIntPoint LoResTextureSize(FMath::Max(InViewRect.Width() >> LoResMipLevel, 1), FMath::Max(InViewRect.Height() >> LoResMipLevel, 1));
		const FVector2f LoResTexelSize(1.0f / LoResTextureSize.X, 1.0f / LoResTextureSize.Y);
		const FIntPoint LoResDispatchSize(FMath::Max(InViewRect.Width() >> LoResMipLevel, 1), FMath::Max(InViewRect.Height() >> LoResMipLevel, 1));
		const FIntPoint HiResDispatchSize(FMath::Max(InViewRect.Width() >> HiResMipLevel, 1), FMath::Max(InViewRect.Height() >> HiResMipLevel, 1));

		// DONT: downsample THEN Filter. Noise will alias back in.
		// DO: filter THEN downsample

		// blur
		{
			FVARIDGaussianBlurCS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDGaussianBlurCS::FParameters>();
			PassParameters->View = InView.ViewUniformBuffer;
			PassParameters->InSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(OutTexture, HiResMipLevel));
			PassParameters->OutUAV = InGraphBuilder.CreateUAV(FRDGTextureUAVDesc(BlurredMipTexture, HiResMipLevel));

			FComputeShaderUtils::AddPass(
				InGraphBuilder,
				RDG_EVENT_NAME("VARID - Build Gaussian Pyramid - Gaussian Blur - MipLevel=%d", HiResMipLevel),
				GaussianBlurComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(HiResDispatchSize, FComputeShaderUtils::kGolden2DGroupSize));
		}

		// downsample
		{
			FVARIDBasicResampleCS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDBasicResampleCS::FParameters>();
			PassParameters->View = InView.ViewUniformBuffer;
			PassParameters->InTexelSize = LoResTexelSize;
			PassParameters->InSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->InSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(BlurredMipTexture, HiResMipLevel));
			PassParameters->OutUAV = InGraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutTexture, LoResMipLevel));

			FComputeShaderUtils::AddPass(
				InGraphBuilder,
				RDG_EVENT_NAME("VARID - Build Gaussian Pyramid - Downsample - MipLevel=%d", LoResMipLevel),
				ResampleComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(LoResDispatchSize, FComputeShaderUtils::kGolden2DGroupSize));
		}
	}

	return true;
}

/************************************************************************/
// DEBUG

bool FVARIDRendering::DrawDebugSolidColor_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugSolidColorPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FVARIDDebugSolidColorPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugSolidColorPS::FParameters>();
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Debug Solid Color"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawDebugUVMap_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugUVMapPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FVARIDDebugUVMapPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugUVMapPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Debug UV Map"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawDebugDepthMap_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugDepthMapPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(InGraphBuilder, InView, ESceneTextureSetupMode::All);	// must be manually registered with the graph builder

	FVARIDDebugDepthMapPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugDepthMapPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->SceneTextures = SceneTextures;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Debug Depth Map"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawDebugPassthrough_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugPassthroughPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FVARIDDebugPassthroughPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugPassthroughPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Debug Passthrough"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawDebugGazePosition_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const FVector2f InNormalizedGazePosition)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugGazePositionPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FVARIDDebugGazePositionPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugGazePositionPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Debug Gaze Position"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

/************************************************************************/
// EYE CONDITIONS

bool FVARIDRendering::DrawCataracts_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const float InBlurStrength, const float InContrastReduction, const float InBrightThreshold, const float InGlareStrength)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDCataractsPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType);

	FVARIDCataractsPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDCataractsPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InBlurStrength = InBlurStrength;
	PassParameters->InContrastReduction = InContrastReduction;
	PassParameters->InBrightThreshold = InBrightThreshold;
	PassParameters->InGlareStrength = InGlareStrength;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Cataracts"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawColorVisionDeficiency_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const EVARIDColorVisionDeficiencyType InCVDType)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDColorVisionDeficiencyPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FVector3f MatrixRow0, MatrixRow1, MatrixRow2;

	switch (InCVDType)
	{
	case EVARIDColorVisionDeficiencyType::None:
		MatrixRow0 = FVector3f(1.0, 0.0, 0.0);
		MatrixRow1 = FVector3f(0.0, 1.0, 0.0);
		MatrixRow2 = FVector3f(0.0, 0.0, 1.0);
		break;
	case EVARIDColorVisionDeficiencyType::Protanopia:
		MatrixRow0 = FVector3f(0.56667, 0.43333, 0.0);
		MatrixRow1 = FVector3f(0.55833, 0.44167, 0.0);
		MatrixRow2 = FVector3f(0.0, 0.24167, 0.75833);
		break;
	case EVARIDColorVisionDeficiencyType::Deuteranopia:
		MatrixRow0 = FVector3f(0.625, 0.375, 0.0);
		MatrixRow1 = FVector3f(0.7, 0.3, 0.0);
		MatrixRow2 = FVector3f(0.0, 0.3, 0.7);
		break;
	case EVARIDColorVisionDeficiencyType::Tritanopia:
		MatrixRow0 = FVector3f(0.95, 0.05, 0.0);
		MatrixRow1 = FVector3f(0.0, 0.43333, 0.56667);
		MatrixRow2 = FVector3f(0.0, 0.475, 0.525);
		break;
	default:
		MatrixRow0 = FVector3f(1.0, 0.0, 0.0);
		MatrixRow1 = FVector3f(0.0, 1.0, 0.0);
		MatrixRow2 = FVector3f(0.0, 0.0, 1.0);
		break;
	}

	FVARIDColorVisionDeficiencyPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDColorVisionDeficiencyPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->InCvdMatrixRow0 = MatrixRow0;
	PassParameters->InCvdMatrixRow1 = MatrixRow1;
	PassParameters->InCvdMatrixRow2 = MatrixRow2;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Color Vision Deficiency"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters);

	return true;
}

bool FVARIDRendering::DrawDiabeticRetinopathy_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, FRHITexture* InFloaterTextureArrayRHI, const FVector2f InNormalizedGazePosition, const uint8 InNumFloaters, const float InFloaterSpeed, const float InFloaterScale, const float InBlurStrength, const float InContrastReduction)
{
	if (!InFloaterTextureArrayRHI)
	{
		UE_LOG(LogTemp, Warning, TEXT("VARID DrawDiabeticRetinopathy: InFloaterTextureArrayRHI is null! Cannot draw Diabetic Retinopathy effect."));
		// shader EXPECTS a texture array
		return false;
	}

	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDiabeticRetinopathyPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	uint16 FloaterArraySize = InFloaterTextureArrayRHI->GetDesc().ArraySize;

	uint8 NumFloaters = InNumFloaters;
	NumFloaters = FMath::Clamp(NumFloaters, 0, FloaterArraySize);
	NumFloaters = FMath::Clamp(NumFloaters, 0, 32);

	FRDGTextureRef FloaterTextureArray = RegisterExternalTexture(InGraphBuilder, InFloaterTextureArrayRHI, TEXT("VARID Floater Texture Array"));

	const FIntPoint ViewportSize = InSceneColor.ViewRect.Size();
	const uint8 NumberOfMipsToGenerate = CalculateNumMips2D(ViewportSize);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType);

	FVARIDDiabeticRetinopathyPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDiabeticRetinopathyPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InFloaterSRVArray = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FloaterTextureArray));
	PassParameters->InNumFloaters = NumFloaters;
	PassParameters->InFloaterSpeed = InFloaterSpeed;
	PassParameters->InContrastReduction = InContrastReduction;
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->InFloaterScale = InFloaterScale;
	PassParameters->InMaxMipLevel = NumberOfMipsToGenerate;
	PassParameters->InBlurStrength = InBlurStrength;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Diabetic Retinopathy"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawGlaucoma_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, FRHITexture* InScotomaTextureRHI, const FVector2f InNormalizedGazePosition)
{
	if (!InScotomaTextureRHI)
	{
		//FRDGTextureRef ScotomaTexture = GSystemTextures.GetBlackDummy(InGraphBuilder);	// Could use a dummy texture, but it would not be correct

		UE_LOG(LogTemp, Warning, TEXT("VARID DrawGlaucoma: InScotomaTextureRHI is null! Cannot draw Glaucoma effect."));
		return false;
	}

	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDGlaucomaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FRDGTextureRef ScotomaTexture = RegisterExternalTexture(InGraphBuilder, InScotomaTextureRHI, TEXT("VARID Scotoma Texture"));

	const FIntPoint ViewportSize = InSceneColor.ViewRect.Size();
	const uint8 NumberOfMipsToGenerate = CalculateNumMips2D(ViewportSize);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType);

	FVARIDGlaucomaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDGlaucomaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InScotomaSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(ScotomaTexture));
	PassParameters->InMaxMipLevel = NumberOfMipsToGenerate;
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Glaucoma"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawHyperopia_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const float InFocalLength_CM, const float InBlurStrength)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDHyperopiaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType);

	FSceneTextureShaderParameters SceneTextureShaderParameters = CreateSceneTextureShaderParameters(InGraphBuilder, InView, ESceneTextureSetupMode::All);	// must be manually registered with the graph builder

	FVARIDHyperopiaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDHyperopiaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->SceneTextures = SceneTextureShaderParameters;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InFocalLength_CM = InFocalLength_CM;
	PassParameters->InBlurStrength = InBlurStrength;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Hyperopia"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawMacularDegeneration_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const FVector2f InNormalizedGazePosition, const float InRadius, const float InBlurStrength)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDMacularDegenerationPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	const FIntPoint ViewportSize = InSceneColor.ViewRect.Size();
	const uint8 NumberOfMipsToGenerate = CalculateNumMips2D(ViewportSize);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType);

	FVARIDMacularDegenerationPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDMacularDegenerationPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InMaxMipLevel = NumberOfMipsToGenerate;
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->InRadius = InRadius;
	PassParameters->InBlurStrength = InBlurStrength;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Macular Degeneration"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawMyopia_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const float InFocalLength_CM, const float InBlurStrength)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDMyopiaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType);

	FSceneTextureShaderParameters SceneTextureShaderParameters = CreateSceneTextureShaderParameters(InGraphBuilder, InView, ESceneTextureSetupMode::All);	// must be manually registered with the graph builder

	FVARIDMyopiaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDMyopiaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->SceneTextures = SceneTextureShaderParameters;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InFocalLength_CM = InFocalLength_CM;
	PassParameters->InBlurStrength = InBlurStrength;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Myopia"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawNystagmus_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const FVector2f InFrequency, const FVector2f InAmplitude)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDNystagmusPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FVARIDNystagmusPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDNystagmusPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->InFrequency = InFrequency;
	PassParameters->InAmplitude = InAmplitude;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Nystagmus"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}

bool FVARIDRendering::DrawRetinitisPigmentosa_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FRenderTargetBinding& InRenderTargetBinding, const FVector2f InNormalizedGazePosition, const float InRadius)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDRetinitisPigmentosaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InSceneColor.ViewRect);

	FVARIDRetinitisPigmentosaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDRetinitisPigmentosaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->InRadius = InRadius;
	PassParameters->RenderTargets[0] = InRenderTargetBinding;

	AddDrawScreenPass(
		InGraphBuilder,
		RDG_EVENT_NAME("VARID Draw Retinitis Pigmentosa"),
		InView,
		OutputViewport,
		InputViewport,
		VertexShader,
		PixelShader,
		PassParameters
	);

	return true;
}