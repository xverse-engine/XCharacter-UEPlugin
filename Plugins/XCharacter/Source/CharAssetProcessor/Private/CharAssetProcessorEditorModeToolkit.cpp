// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharAssetProcessorEditorModeToolkit.h"
#include "CharAssetProcessorEditorMode.h"
#include "Engine/Selection.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "CharAssetProcessorEditorModeToolkit"

FCharAssetProcessorEditorModeToolkit::FCharAssetProcessorEditorModeToolkit()
{
}

void FCharAssetProcessorEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FCharAssetProcessorEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FCharAssetProcessorEditorModeToolkit::GetToolkitFName() const
{
	return FName("CharAssetProcessorEditorMode");
}

FText FCharAssetProcessorEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "CharAssetProcessorEditorMode Toolkit");
}

#undef LOCTEXT_NAMESPACE
