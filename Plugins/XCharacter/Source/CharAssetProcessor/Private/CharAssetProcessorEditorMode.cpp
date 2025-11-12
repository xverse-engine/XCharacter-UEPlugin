// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharAssetProcessorEditorMode.h"
#include "CharAssetProcessorEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "CharAssetProcessorEditorModeCommands.h"


//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
// AddYourTool Step 1 - include the header file for your Tools here
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
#include "Tools/CharAssetProcessorSimpleTool.h"
#include "Tools/CharAssetProcessorInteractiveTool.h"

// step 2: register a ToolBuilder in FCharAssetProcessorEditorMode::Enter() below


#define LOCTEXT_NAMESPACE "CharAssetProcessorEditorMode"

const FEditorModeID UCharAssetProcessorEditorMode::EM_CharAssetProcessorEditorModeId = TEXT("EM_CharAssetProcessorEditorMode");

FString UCharAssetProcessorEditorMode::SimpleToolName = TEXT("CharAssetProcessor_ActorInfoTool");
FString UCharAssetProcessorEditorMode::InteractiveToolName = TEXT("CharAssetProcessor_MeasureDistanceTool");


UCharAssetProcessorEditorMode::UCharAssetProcessorEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	// appearance and icon in the editing mode ribbon can be customized here
	Info = FEditorModeInfo(UCharAssetProcessorEditorMode::EM_CharAssetProcessorEditorModeId,
		LOCTEXT("ModeName", "CharAssetProcessor"),
		FSlateIcon(),
		true);
}


UCharAssetProcessorEditorMode::~UCharAssetProcessorEditorMode()
{
}


void UCharAssetProcessorEditorMode::ActorSelectionChangeNotify()
{
}

void UCharAssetProcessorEditorMode::Enter()
{
	UEdMode::Enter();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// AddYourTool Step 2 - register the ToolBuilders for your Tools here.
	// The string name you pass to the ToolManager is used to select/activate your ToolBuilder later.
	//////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////// 
	const FCharAssetProcessorEditorModeCommands& SampleToolCommands = FCharAssetProcessorEditorModeCommands::Get();

	RegisterTool(SampleToolCommands.SimpleTool, SimpleToolName, NewObject<UCharAssetProcessorSimpleToolBuilder>(this));
	RegisterTool(SampleToolCommands.InteractiveTool, InteractiveToolName, NewObject<UCharAssetProcessorInteractiveToolBuilder>(this));

	// active tool type is not relevant here, we just set to default
	GetToolManager()->SelectActiveToolType(EToolSide::Left, SimpleToolName);
}

void UCharAssetProcessorEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FCharAssetProcessorEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UCharAssetProcessorEditorMode::GetModeCommands() const
{
	return FCharAssetProcessorEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
