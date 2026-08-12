// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "VARIDModule.h"
#include "VARIDSceneViewExtension.h"

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ShaderCore.h"
#include "UObject/GCObject.h"

#define LOCTEXT_NAMESPACE "FVARIDModule"

class FVARIDTextureReferencer : public FGCObject
{
public:
	explicit FVARIDTextureReferencer(FVARIDModule& InModule)
		: Module(InModule)
	{
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Module.GetLeftEyeConditionState().ScotomaTexture);
		Collector.AddReferencedObject(Module.GetRightEyeConditionState().ScotomaTexture);
		Collector.AddReferencedObject(Module.GetMonoEyeConditionState().ScotomaTexture);

		Collector.AddReferencedObject(Module.GetLeftEyeConditionState().FloaterTextureArray);
		Collector.AddReferencedObject(Module.GetRightEyeConditionState().FloaterTextureArray);
		Collector.AddReferencedObject(Module.GetMonoEyeConditionState().FloaterTextureArray);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FVARIDTextureReferencer");
	}

private:
	FVARIDModule& Module;
};

void FVARIDModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("VARID: FVARIDModule_StartupModule"));

	check(ENGINE_MAJOR_VERSION == 5);
	check(ENGINE_MINOR_VERSION == 5);

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// map shader dir
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("VARID"))->GetBaseDir(), TEXT("Shaders"));
	UE_LOG(LogTemp, Display, TEXT("VARID: PluginShaderDir: %s"), *PluginShaderDir);
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/VARID"), PluginShaderDir);

	// can't instantiate scene extension here. too early to be added.
}

void FVARIDModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("VARID: FVARIDModule_ShutdownModule"));

	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	EndRendering();	// Module could be shutdown before we explicitly end rendering. Ensure cleanup.
	delete TextureReferencer;
	TextureReferencer = nullptr;

	// UE 5.5 does not expose a per-mapping remove API. Do not call
	// ResetAllShaderSourceDirectoryMappings(), because that also clears mappings
	// owned by the engine and other plugins.
}

void FVARIDModule::BeginRendering(EVARIDSamplerType InSamplerType)
{
	UE_LOG(LogTemp, Display, TEXT("VARID: FVARIDModule_BeginRendering"));

	EnsureTextureReferencer();

	if (!SceneViewExtension)
	{
		SceneViewExtension = FSceneViewExtensions::NewExtension<FVARIDSceneViewExtension>(InSamplerType);
	}
}

void FVARIDModule::EnsureTextureReferencer()
{
	if (!TextureReferencer)
	{
		TextureReferencer = new FVARIDTextureReferencer(*this);
	}
}

void FVARIDModule::EndRendering()
{
	UE_LOG(LogTemp, Display, TEXT("VARID: FVARIDModule_EndRendering"));

	if (SceneViewExtension)
	{
		FlushRenderingCommands();
		SceneViewExtension.Reset();
		SceneViewExtension = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVARIDModule, VARID)
