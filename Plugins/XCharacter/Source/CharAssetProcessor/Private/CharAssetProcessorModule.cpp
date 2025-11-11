// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharAssetProcessorModule.h"
#include "CharAssetProcessorEditorModeCommands.h"

#define LOCTEXT_NAMESPACE "CharAssetProcessorModule"

void FCharAssetProcessorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FCharAssetProcessorEditorModeCommands::Register();
}

void FCharAssetProcessorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FCharAssetProcessorEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCharAssetProcessorModule, CharAssetProcessorEditorMode)