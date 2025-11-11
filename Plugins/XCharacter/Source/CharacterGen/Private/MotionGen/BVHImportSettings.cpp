// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionGen/BVHImportSettings.h"
#include "Serialization/Archive.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/ReleaseObjectVersion.h"

UBVHImportSettings::UBVHImportSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bReimport = false;
}

UBVHImportSettings* UBVHImportSettings::Get()
{	
	static UBVHImportSettings* DefaultSettings = nullptr;
	if (!DefaultSettings)
	{
		// This is a singleton, use default object
		DefaultSettings = DuplicateObject(GetMutableDefault<UBVHImportSettings>(), GetTransientPackage());
		DefaultSettings->AddToRoot();
	}
	
	return DefaultSettings;
}

void UBVHImportSettings::Serialize(FArchive& Archive)
{
	Super::Serialize( Archive );
}
