// Fill out your copyright notice in the Description page of Project Settings.


#include "MotionGen/MoGenSetting.h"

void UMoGenSetting::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
}

UMoGenSetting* UMoGenSetting::Get()
{
	static UMoGenSetting* DefaultSettings = nullptr;
	if (!DefaultSettings)
	{
		// This is a singleton, use default object
		DefaultSettings = DuplicateObject(GetMutableDefault<UMoGenSetting>(), GetTransientPackage());
		DefaultSettings->AddToRoot();
	}

	return DefaultSettings;
}