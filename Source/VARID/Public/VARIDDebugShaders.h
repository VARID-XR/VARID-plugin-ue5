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

class FVARIDDebugDepthMapPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDDebugDepthMapPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDDebugDepthMapPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
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
IMPLEMENT_GLOBAL_SHADER(FVARIDDebugDepthMapPS, "/Plugin/VARID/Private/Debug/VARIDDebugDepthMapPS.usf", "MainPS", SF_Pixel);


class FVARIDDebugGazePositionPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDDebugGazePositionPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDDebugGazePositionPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
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
IMPLEMENT_GLOBAL_SHADER(FVARIDDebugGazePositionPS, "/Plugin/VARID/Private/Debug/VARIDDebugGazePositionPS.usf", "MainPS", SF_Pixel);


class FVARIDDebugPassthroughPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDDebugPassthroughPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDDebugPassthroughPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InSceneColorSRV)
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
IMPLEMENT_GLOBAL_SHADER(FVARIDDebugPassthroughPS, "/Plugin/VARID/Private/Debug/VARIDDebugPassthroughPS.usf", "MainPS", SF_Pixel);


class FVARIDDebugSolidColorPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDDebugSolidColorPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDDebugSolidColorPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

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
IMPLEMENT_GLOBAL_SHADER(FVARIDDebugSolidColorPS, "/Plugin/VARID/Private/Debug/VARIDDebugSolidColorPS.usf", "MainPS", SF_Pixel);


class FVARIDDebugUVMapPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FVARIDDebugUVMapPS, Global);
	SHADER_USE_PARAMETER_STRUCT(FVARIDDebugUVMapPS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
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
IMPLEMENT_GLOBAL_SHADER(FVARIDDebugUVMapPS, "/Plugin/VARID/Private/Debug/VARIDDebugUVMapPS.usf", "MainPS", SF_Pixel);
