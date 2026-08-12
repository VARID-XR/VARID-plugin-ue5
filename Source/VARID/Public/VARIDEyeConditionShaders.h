// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterStruct.h"

#include "ShaderParameterUtils.h"
#include "ScreenPass.h"
#include "SceneTexturesConfig.h"


class FVARIDCataractsPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDCataractsPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDCataractsPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER(float, InBlurStrength)
		SHADER_PARAMETER(float, InContrastReduction)
		SHADER_PARAMETER(float, InBrightThreshold)
		SHADER_PARAMETER(float, InGlareStrength)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDCataractsPS, "/Plugin/VARID/Private/EyeConditions/VARIDCataractsPS.usf", "MainPS", SF_Pixel);


class FVARIDColorVisionDeficiencyPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDColorVisionDeficiencyPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDColorVisionDeficiencyPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER(FVector3f, InCvdMatrixRow0)
		SHADER_PARAMETER(FVector3f, InCvdMatrixRow1)
		SHADER_PARAMETER(FVector3f, InCvdMatrixRow2)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDColorVisionDeficiencyPS, "/Plugin/VARID/Private/EyeConditions/VARIDColorVisionDeficiencyPS.usf", "MainPS", SF_Pixel);


class FVARIDDiabeticRetinopathyPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDDiabeticRetinopathyPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDDiabeticRetinopathyPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, InFloaterSRVArray)
		SHADER_PARAMETER(int, InNumFloaters)
		SHADER_PARAMETER(float, InContrastReduction)
		SHADER_PARAMETER(float, InAspectRatio)
		SHADER_PARAMETER(float, InMaxMipLevel)
		SHADER_PARAMETER(float, InBlurStrength)
		SHADER_PARAMETER_ARRAY(FVector4f, InFloaterCenterScale, [32])
		SHADER_PARAMETER_ARRAY(FVector4f, InFloaterRotation, [32])
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDDiabeticRetinopathyPS, "/Plugin/VARID/Private/EyeConditions/VARIDDiabeticRetinopathyPS.usf", "MainPS", SF_Pixel);


class FVARIDGlaucomaPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDGlaucomaPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDGlaucomaPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InScotomaSRV)
		SHADER_PARAMETER(float, InMaxMipLevel)
		SHADER_PARAMETER(FVector2f, InNormalizedGazePosition)
		SHADER_PARAMETER(float, InAspectRatio)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDGlaucomaPS, "/Plugin/VARID/Private/EyeConditions/VARIDGlaucomaPS.usf", "MainPS", SF_Pixel);


class FVARIDHyperopiaPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDHyperopiaPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDHyperopiaPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)	// required for depth
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER(float, InFocalLength_CM)
		SHADER_PARAMETER(float, InBlurStrength)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDHyperopiaPS, "/Plugin/VARID/Private/EyeConditions/VARIDHyperopiaPS.usf", "MainPS", SF_Pixel);


class FVARIDMacularDegenerationPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDMacularDegenerationPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDMacularDegenerationPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER(float, InMaxMipLevel)
		SHADER_PARAMETER(FVector2f, InNormalizedGazePosition)
		SHADER_PARAMETER(float, InAspectRatio)
		SHADER_PARAMETER(float, InRadius)
		SHADER_PARAMETER(float, InBlurStrength)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDMacularDegenerationPS, "/Plugin/VARID/Private/EyeConditions/VARIDMacularDegenerationPS.usf", "MainPS", SF_Pixel);


class FVARIDMyopiaPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDMyopiaPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDMyopiaPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)	// required for depth
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER(float, InFocalLength_CM)
		SHADER_PARAMETER(float, InBlurStrength)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDMyopiaPS, "/Plugin/VARID/Private/EyeConditions/VARIDMyopiaPS.usf", "MainPS", SF_Pixel);


class FVARIDNystagmusPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDNystagmusPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDNystagmusPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER(FVector2f, InOffset)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDNystagmusPS, "/Plugin/VARID/Private/EyeConditions/VARIDNystagmusPS.usf", "MainPS", SF_Pixel);


class FVARIDRetinitisPigmentosaPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDRetinitisPigmentosaPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDRetinitisPigmentosaPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
		SHADER_PARAMETER(FVector2f, InNormalizedGazePosition)
		SHADER_PARAMETER(float, InAspectRatio)
		SHADER_PARAMETER(float, InRadius)
		RENDER_TARGET_BINDING_SLOTS()

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
IMPLEMENT_GLOBAL_SHADER(FVARIDRetinitisPigmentosaPS, "/Plugin/VARID/Private/EyeConditions/VARIDRetinitisPigmentosaPS.usf", "MainPS", SF_Pixel);
