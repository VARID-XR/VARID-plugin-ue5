// This source code is provided "as is" without warranty of any kind, either express or implied. Use at your own risk.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Modules/ModuleManager.h"
#include "VARIDEnums.h"
#include "VARIDEyeConditionState.h"

class FVARIDSceneViewExtension;

// This class is the hub of the VARID plugin. 
// The IModuleInterface gives us singleton behaviour which is perfect because we only want one instance

class FVARIDModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FVARIDModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FVARIDModule>("VARID");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		bool result = FModuleManager::Get().IsModuleLoaded("VARID");

		if (result)
		{
			UE_LOG(LogTemp, Warning, TEXT("FVARIDModule::IsAvailable TRUE"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("FVARIDModule::IsAvailable FALSE"));
		}

		return result;
	}

public:
	void BeginRendering(EVARIDSamplerType InSamplerType);
	void EndRendering();

public:
	TSharedPtr<FVARIDSceneViewExtension> GetSceneViewExtension() const { return SceneViewExtension; }

	FVARIDEyeConditionState& GetEyeConditionState(const EVARIDEyeType Eye) 
	{ 		
		switch (Eye)
		{
		case EVARIDEyeType::Mono:
			return MonoEyeConditionState;
			break;
		case EVARIDEyeType::Left:
			return LeftEyeConditionState;
			break;
		case EVARIDEyeType::Right:
			return RightEyeConditionState;
			break;
		default:
			return MonoEyeConditionState; // Default to Mono if an invalid eye type is provided
			break;
		}
	}

	FVARIDEyeConditionState& GetLeftEyeConditionState() { return LeftEyeConditionState; }
	FVARIDEyeConditionState& GetRightEyeConditionState() { return RightEyeConditionState; }
	FVARIDEyeConditionState& GetMonoEyeConditionState() { return MonoEyeConditionState; }

private:
	TSharedPtr<FVARIDSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
	FVARIDEyeConditionState LeftEyeConditionState;
	FVARIDEyeConditionState RightEyeConditionState;
	FVARIDEyeConditionState MonoEyeConditionState;
};

