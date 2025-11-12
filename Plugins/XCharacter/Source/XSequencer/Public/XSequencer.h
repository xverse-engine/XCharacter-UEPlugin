// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/Object.h"

class FToolBarBuilder;
class FMenuBuilder;

class FXSequencerModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FString GetDateTimeSnakeCase()
	{
		FString Month = FString::FromInt(FDateTime::Now().GetMonth());
		if (Month.Len() == 1)
		{
			Month = "0" + Month;
		}

		FString Day = FString::FromInt(FDateTime::Now().GetDay());
		if (Day.Len() == 1)
		{
			Day = "0" + Day;
		}

		FString Hour = FString::FromInt(FDateTime::Now().GetHour());
		if (Hour.Len() == 1)
		{
			Hour = "0" + Hour;
		}

		FString Minute = FString::FromInt(FDateTime::Now().GetMinute());
		if (Minute.Len() == 1)
		{
			Minute = "0" + Minute;
		}

		FString Second = FString::FromInt(FDateTime::Now().GetSecond());
		if (Second.Len() == 1)
		{
			Second = "0" + Second;
		}

		return FString::FromInt(FDateTime::Now().GetYear()) + "_"
			+ Month + "_"
			+ Day + "_"
			+ Hour + "_"
			+ Minute + "_"
			+ Second;
	};
};
