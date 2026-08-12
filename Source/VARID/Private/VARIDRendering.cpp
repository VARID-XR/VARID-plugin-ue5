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

namespace
{
	constexpr int32 VARIDMaxFloaters = 32;
	constexpr float VARIDTau = 6.2831853f;

	float VARIDFrac(float InValue)
	{
		return InValue - FMath::FloorToFloat(InValue);
	}

	FVector2f VARIDGetFloaterDirection(int32 InIndex)
	{
		const float Angle = VARIDFrac(FMath::Sin(InIndex * 91.928f) * 43758.5453f) * VARIDTau;
		return FVector2f(FMath::Cos(Angle), FMath::Sin(Angle));
	}

	FVector2f VARIDGetFloaterBaseOffset(int32 InIndex)
	{
		return FVector2f
		(
			VARIDFrac(FMath::Sin(InIndex * 13.37f) * 12345.6789f),
			VARIDFrac(FMath::Cos(InIndex * 7.42f) * 98765.4321f)
		);
	}

	FVector2f VARIDGetFloaterJitter(float InRealTime, int32 InIndex)
	{
		const FVector2f JitterFrequency(1.5f + 0.2f * InIndex, 1.0f + 0.15f * InIndex);
		const FVector2f JitterAmplitude(0.001f, 0.001f);

		return FVector2f
		(
			FMath::Sin(InRealTime * JitterFrequency.X * VARIDTau) >= 0.97f ? JitterAmplitude.X : 0.0f,
			FMath::Sin(InRealTime * JitterFrequency.Y * VARIDTau) >= 0.97f ? JitterAmplitude.Y : 0.0f
		);
	}

	float VARIDHash11(float InValue)
	{
		return VARIDFrac(FMath::Sin(InValue) * 43758.5453f);
	}

	float VARIDTriangleAsymmetric(float InPhase, float InSkew)
	{
		return InPhase < InSkew
			? InPhase / InSkew
			: 1.0f - (InPhase - InSkew) / (1.0f - InSkew);
	}

	FVector2f VARIDWaveAsymmetricTriangleRandomized(float InRealTime, FVector2f InFrequency, FVector2f InAmplitude, FVector2f InSkew, float InRandomStrength)
	{
		const FVector2f Phase(VARIDFrac(InRealTime * InFrequency.X), VARIDFrac(InRealTime * InFrequency.Y));
		const float CycleIndexX = FMath::FloorToFloat(InRealTime * InFrequency.X);

		const float RandomSkewOffset = (VARIDHash11(CycleIndexX + 17.0f) - 0.5f) * InRandomStrength;
		const FVector2f RandomSkew
		(
			FMath::Clamp(InSkew.X + RandomSkewOffset, 0.05f, 0.95f),
			FMath::Clamp(InSkew.Y + RandomSkewOffset, 0.05f, 0.95f)
		);

		const float RandomAmpFactor = (VARIDHash11(CycleIndexX + 53.0f) - 0.5f) * InRandomStrength;
		const FVector2f RandomAmplitude = InAmplitude + (InAmplitude * RandomAmpFactor);
		const FVector2f Triangle
		(
			VARIDTriangleAsymmetric(Phase.X, RandomSkew.X),
			VARIDTriangleAsymmetric(Phase.Y, RandomSkew.Y)
		);

		return FVector2f
		(
			(Triangle.X - 0.5f) * 2.0f * RandomAmplitude.X,
			(Triangle.Y - 0.5f) * 2.0f * RandomAmplitude.Y
		);
	}
}

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
	return FMath::Max<uint8>(FMath::Min(NumberOfMipsToGenerate, FVARIDRendering::MAX_NUM_MIP_LEVELS), 1);
}

uint8 FVARIDRendering::CalculateNumMipsForBlur(FIntPoint InSize, float InMaxBlurStrength)
{
	const uint8 AvailableMips = CalculateNumMips2D(InSize);
	const int32 RequestedMips = FMath::CeilToInt(FMath::Max(InMaxBlurStrength, 0.0f)) + 1;
	return static_cast<uint8>(FMath::Clamp(RequestedMips, 1, static_cast<int32>(AvailableMips)));
}

FRDGTextureRef FVARIDRendering::CreateBlurredTexture(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const EVARIDSamplerType InSamplerType, const uint8 InNumMipsToGenerate)
{
	ensureAlwaysMsgf(InSceneColor.Texture->Desc.ArraySize == 1, TEXT("InSceneColor is still arrayed (ArraySize=%d). Ensure CopyFromSlice is used in the scene view extension or bind FirstArraySlice."), InSceneColor.Texture->Desc.ArraySize);

	if (InNumMipsToGenerate <= 1)
	{
		return InSceneColor.Texture;
	}

	const FIntPoint ViewSize = InSceneColor.ViewRect.Size();

	FRDGTextureDesc BlurredTextureDesc = FRDGTextureDesc::Create2D
	(
		ViewSize,
		InSceneColor.Texture->Desc.Format,
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV,
		InNumMipsToGenerate
	);

	FRDGTextureRef BlurredTexture = InGraphBuilder.CreateTexture(BlurredTextureDesc, TEXT("VARID BlurredTexture"));

	bool Result = false;

	switch (InSamplerType)
	{
	case EVARIDSamplerType::PointSampling:
		Result = BuildSamplerPyramid_RenderThread(InGraphBuilder, InSceneColor.ViewRect, InSceneColor.Texture, BlurredTexture, InView, InNumMipsToGenerate, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		break;
	case EVARIDSamplerType::BilinearSampling:
		Result = BuildSamplerPyramid_RenderThread(InGraphBuilder, InSceneColor.ViewRect, InSceneColor.Texture, BlurredTexture, InView, InNumMipsToGenerate, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		break;
	case EVARIDSamplerType::GaussianSampling:
		Result = BuildGaussianPyramid_RenderThread(InGraphBuilder, InSceneColor.ViewRect, InSceneColor.Texture, BlurredTexture, InNumMipsToGenerate);
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
		PassParameters->InOutputSize = FVector2f(static_cast<float>(DispatchSize.X), static_cast<float>(DispatchSize.Y));
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

bool FVARIDRendering::BuildGaussianPyramid_RenderThread(FRDGBuilder& InGraphBuilder, const FIntRect InViewRect, const FRDGTextureRef InTexture, const FRDGTextureRef OutTexture, const uint8 InNumMips)
{
	check(InTexture);
	check(OutTexture);

	const FRDGTextureDesc& OutTextureDesc = OutTexture->Desc;

	TShaderMapRef<FVARIDGaussianDownsampleCS> GaussianDownsampleComputeShader(GlobalShaderMap);

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

		const FIntPoint LoResDispatchSize(FMath::Max(InViewRect.Width() >> LoResMipLevel, 1), FMath::Max(InViewRect.Height() >> LoResMipLevel, 1));
		const FIntPoint HiResDispatchSize(FMath::Max(InViewRect.Width() >> HiResMipLevel, 1), FMath::Max(InViewRect.Height() >> HiResMipLevel, 1));

		// Filter while downsampling so each mip costs one compute pass.
		{
			FVARIDGaussianDownsampleCS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDGaussianDownsampleCS::FParameters>();
			PassParameters->InInputSize = FVector2f(static_cast<float>(HiResDispatchSize.X), static_cast<float>(HiResDispatchSize.Y));
			PassParameters->InOutputSize = FVector2f(static_cast<float>(LoResDispatchSize.X), static_cast<float>(LoResDispatchSize.Y));
			PassParameters->InSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(OutTexture, HiResMipLevel));
			PassParameters->OutUAV = InGraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutTexture, LoResMipLevel));

			FComputeShaderUtils::AddPass(
				InGraphBuilder,
				RDG_EVENT_NAME("VARID - Build Gaussian Pyramid - Gaussian Downsample - MipLevel=%d", LoResMipLevel),
				GaussianDownsampleComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(LoResDispatchSize, FComputeShaderUtils::kGolden2DGroupSize));
		}
	}

	return true;
}

/************************************************************************/
// DEBUG

bool FVARIDRendering::DrawDebugSolidColor_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugSolidColorPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FVARIDDebugSolidColorPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugSolidColorPS::FParameters>();
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawDebugUVMap_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugUVMapPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FVARIDDebugUVMapPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugUVMapPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawDebugDepthMap_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugDepthMapPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(InGraphBuilder, InView, ESceneTextureSetupMode::All);	// must be manually registered with the graph builder

	FVARIDDebugDepthMapPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugDepthMapPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->SceneTextures = SceneTextures;
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawDebugPassthrough_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugPassthroughPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FVARIDDebugPassthroughPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugPassthroughPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawDebugGazePosition_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InNormalizedGazePosition)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDDebugGazePositionPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FVARIDDebugGazePositionPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDebugGazePositionPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawCataracts_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const float InBlurStrength, const float InContrastReduction, const float InBrightThreshold, const float InGlareStrength)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDCataractsPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	const uint8 NumberOfMipsToGenerate = CalculateNumMipsForBlur(InSceneColor.ViewRect.Size(), InBlurStrength);
	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType, NumberOfMipsToGenerate);

	FVARIDCataractsPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDCataractsPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InBlurStrength = FMath::Min(InBlurStrength, static_cast<float>(NumberOfMipsToGenerate - 1));
	PassParameters->InContrastReduction = InContrastReduction;
	PassParameters->InBrightThreshold = InBrightThreshold;
	PassParameters->InGlareStrength = InGlareStrength;
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawColorVisionDeficiency_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const EVARIDColorVisionDeficiencyType InCVDType)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDColorVisionDeficiencyPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

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
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawDiabeticRetinopathy_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, FRHITexture* InFloaterTextureArrayRHI, const uint8 InNumFloaters, const float InFloaterSpeed, const float InFloaterScale, const float InBlurStrength, const float InContrastReduction)
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
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	uint16 FloaterArraySize = InFloaterTextureArrayRHI->GetDesc().ArraySize;

	const uint8 NumFloaters = static_cast<uint8>(FMath::Clamp<int32>(InNumFloaters, 0, FMath::Min<int32>(FloaterArraySize, VARIDMaxFloaters)));

	FRDGTextureRef FloaterTextureArray = RegisterExternalTexture(InGraphBuilder, InFloaterTextureArrayRHI, TEXT("VARID Floater Texture Array"));

	const FIntPoint ViewportSize = InSceneColor.ViewRect.Size();
	const uint8 NumberOfMipsToGenerate = CalculateNumMips2D(ViewportSize);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType, NumberOfMipsToGenerate);

	FVARIDDiabeticRetinopathyPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDDiabeticRetinopathyPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InFloaterSRVArray = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FloaterTextureArray));
	PassParameters->InNumFloaters = NumFloaters;
	PassParameters->InContrastReduction = InContrastReduction;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->InMaxMipLevel = NumberOfMipsToGenerate - 1;
	PassParameters->InBlurStrength = FMath::Min(InBlurStrength, static_cast<float>(NumberOfMipsToGenerate - 1));
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

	const float RealTime = InView.Family ? static_cast<float>(InView.Family->Time.GetRealTimeSeconds()) : 0.0f;
	for (int32 FloaterIndex = 0; FloaterIndex < VARIDMaxFloaters; ++FloaterIndex)
	{
		const float ScaleVariation = 0.8f + 0.4f * VARIDFrac(FMath::Sin(FloaterIndex * 19.7f) * 523.1f);
		const float Scale = InFloaterScale * ScaleVariation;

		const FVector2f Center = VARIDGetFloaterDirection(FloaterIndex) * InFloaterSpeed * RealTime
			+ VARIDGetFloaterBaseOffset(FloaterIndex)
			+ VARIDGetFloaterJitter(RealTime, FloaterIndex);

		const float RawRotation = VARIDFrac(FMath::Sin(FloaterIndex * 33.33f) * 6543.21f);
		const float RotationSpeed = 0.5f * FMath::Lerp(-1.0f, 1.0f, RawRotation);
		const float RotationAngle = RealTime * RotationSpeed;

		PassParameters->InFloaterCenterScale[FloaterIndex] = FVector4f(VARIDFrac(Center.X), VARIDFrac(Center.Y), Scale, 0.0f);
		PassParameters->InFloaterRotation[FloaterIndex] = FVector4f(FMath::Cos(RotationAngle), FMath::Sin(RotationAngle), 0.0f, 0.0f);
	}

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

bool FVARIDRendering::DrawGlaucoma_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, FRHITexture* InScotomaTextureRHI, const FVector2f InNormalizedGazePosition)
{
	if (!InScotomaTextureRHI)
	{
		UE_LOG(LogTemp, Warning, TEXT("VARID DrawGlaucoma: InScotomaTextureRHI is null! Cannot draw Glaucoma effect."));
		return false;
	}

	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDGlaucomaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FRDGTextureRef ScotomaTexture = RegisterExternalTexture(InGraphBuilder, InScotomaTextureRHI, TEXT("VARID Scotoma Texture"));

	const FIntPoint ViewportSize = InSceneColor.ViewRect.Size();
	const uint8 NumberOfMipsToGenerate = CalculateNumMips2D(ViewportSize);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType, NumberOfMipsToGenerate);

	FVARIDGlaucomaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDGlaucomaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InScotomaSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(ScotomaTexture));
	PassParameters->InMaxMipLevel = NumberOfMipsToGenerate - 1;
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawHyperopia_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const float InFocalLength_CM, const float InBlurStrength)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDHyperopiaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	const uint8 NumberOfMipsToGenerate = CalculateNumMipsForBlur(InSceneColor.ViewRect.Size(), InBlurStrength);
	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType, NumberOfMipsToGenerate);

	FSceneTextureShaderParameters SceneTextureShaderParameters = CreateSceneTextureShaderParameters(InGraphBuilder, InView, ESceneTextureSetupMode::All);	// must be manually registered with the graph builder

	FVARIDHyperopiaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDHyperopiaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->SceneTextures = SceneTextureShaderParameters;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InFocalLength_CM = InFocalLength_CM;
	PassParameters->InBlurStrength = FMath::Min(InBlurStrength, static_cast<float>(NumberOfMipsToGenerate - 1));
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawMacularDegeneration_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InNormalizedGazePosition, const float InRadius, const float InBlurStrength)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDMacularDegenerationPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	const FIntPoint ViewportSize = InSceneColor.ViewRect.Size();
	const uint8 NumberOfMipsToGenerate = CalculateNumMipsForBlur(ViewportSize, InBlurStrength);

	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType, NumberOfMipsToGenerate);

	FVARIDMacularDegenerationPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDMacularDegenerationPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InMaxMipLevel = NumberOfMipsToGenerate - 1;
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->InRadius = InRadius;
	PassParameters->InBlurStrength = InBlurStrength;
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawMyopia_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const float InFocalLength_CM, const float InBlurStrength)
{
	const TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	const TShaderMapRef<FVARIDMyopiaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor.ViewRect);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	const uint8 NumberOfMipsToGenerate = CalculateNumMipsForBlur(InSceneColor.ViewRect.Size(), InBlurStrength);
	FRDGTextureRef BlurredTexture = CreateBlurredTexture(InGraphBuilder, InSceneColor, InView, SamplerType, NumberOfMipsToGenerate);

	FSceneTextureShaderParameters SceneTextureShaderParameters = CreateSceneTextureShaderParameters(InGraphBuilder, InView, ESceneTextureSetupMode::All);	// must be manually registered with the graph builder

	FVARIDMyopiaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDMyopiaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->SceneTextures = SceneTextureShaderParameters;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlurredTexture));
	PassParameters->InFocalLength_CM = InFocalLength_CM;
	PassParameters->InBlurStrength = FMath::Min(InBlurStrength, static_cast<float>(NumberOfMipsToGenerate - 1));
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawNystagmus_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InFrequency, const FVector2f InAmplitude)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDNystagmusPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FVARIDNystagmusPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDNystagmusPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	const float RealTime = InView.Family ? static_cast<float>(InView.Family->Time.GetRealTimeSeconds()) : 0.0f;
	PassParameters->InOffset = VARIDWaveAsymmetricTriangleRandomized(RealTime, InFrequency, InAmplitude, FVector2f(0.99f, 0.99f), 0.9f);
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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

bool FVARIDRendering::DrawRetinitisPigmentosa_RenderThread(FRDGBuilder& InGraphBuilder, const FScreenPassTexture& InSceneColor, const FSceneView& InView, const FScreenPassRenderTarget& InRenderTarget, const FVector2f InNormalizedGazePosition, const float InRadius)
{
	TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FVARIDRetinitisPigmentosaPS> PixelShader(GlobalShaderMap);

	const FScreenPassTextureViewport InputViewport(InSceneColor);
	const FScreenPassTextureViewport OutputViewport(InRenderTarget);

	FVARIDRetinitisPigmentosaPS::FParameters* PassParameters = InGraphBuilder.AllocParameters<FVARIDRetinitisPigmentosaPS::FParameters>();
	PassParameters->View = InView.ViewUniformBuffer;
	PassParameters->InSceneColorSRV = InGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InSceneColor.Texture));
	PassParameters->InNormalizedGazePosition = InNormalizedGazePosition;
	PassParameters->InAspectRatio = InSceneColor.ViewRect.Width() / (float)InSceneColor.ViewRect.Height();
	PassParameters->InRadius = InRadius;
	PassParameters->RenderTargets[0] = InRenderTarget.GetRenderTargetBinding();

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
