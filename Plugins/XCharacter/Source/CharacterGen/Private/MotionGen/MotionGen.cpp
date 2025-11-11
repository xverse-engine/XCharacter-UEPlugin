// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionGen/MotionGen.h"
#include "ISettingsModule.h"
#include "XVCPluginSettings.h" 

#define LOCTEXT_NAMESPACE "MotionGenModule"

void FMotionGenModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	if (ISettingsModule* SettingModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingModule->RegisterSettings("Project", "Plugins", "XCharacterSettings",
			LOCTEXT("RuntimeSettingsName", "XCharacterSettings"),
			LOCTEXT("RuntimeSettingsDescription", "Config your sceret key and set motion animation download path"),
			GetMutableDefault<UXVCPluginSettings>()
		);
	}

}

void FMotionGenModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "XCharacterSettings");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMotionGenModule, MotionGen)