// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "CharacterGenEditorManager.h"

class FCharacterGenEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override { GetMutableDefault<UCharacterGenEditorManager>(); }
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FCharacterGenEditorModule, CharacterGenEditor)

