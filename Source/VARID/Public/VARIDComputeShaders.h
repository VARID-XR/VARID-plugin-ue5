// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"

class FVARIDBasicResampleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVARIDBasicResampleCS)
	SHADER_USE_PARAMETER_STRUCT(FVARIDBasicResampleCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FVector2f, InTexelSize)
		SHADER_PARAMETER(FVector2f, InOutputSize)
		SHADER_PARAMETER_SAMPLER(SamplerState, InSampler)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutUAV)

	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVARIDBasicResampleCS, "/Plugin/VARID/Private/Compute/VARIDBasicResampleCS.usf", "MainCS", SF_Compute);


class FVARIDGaussianBlurCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVARIDGaussianBlurCS)
	SHADER_USE_PARAMETER_STRUCT(FVARIDGaussianBlurCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FVector2f, InOutputSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutUAV)

	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVARIDGaussianBlurCS, "/Plugin/VARID/Private/Compute/VARIDGaussianBlurCS.usf", "MainCS", SF_Compute);


class FVARIDGaussianDownsampleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVARIDGaussianDownsampleCS)
	SHADER_USE_PARAMETER_STRUCT(FVARIDGaussianDownsampleCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(FVector2f, InInputSize)
		SHADER_PARAMETER(FVector2f, InOutputSize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutUAV)

	END_SHADER_PARAMETER_STRUCT();

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVARIDGaussianDownsampleCS, "/Plugin/VARID/Private/Compute/VARIDGaussianDownsampleCS.usf", "MainCS", SF_Compute);
