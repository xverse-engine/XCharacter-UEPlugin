// Copyright Jared Therriault 2019, 2022

#pragma once

#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_CLASS(LogEasyCsv, Log, All);

class FEasyCsvModule : public IModuleInterface
{
public:
	FEasyCsvModule() {}

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterProjectSettings() const;
	void UnregisterProjectSettings() const;

	enum class ELogType
	{
		Display,
		Warning,
		Error
	};

	// A method to print to log and screen
	static void Print(FString InMessage, const ELogType InLogType = ELogType::Display);

private:

	void OnFEngineLoopInitComplete();
	
	static void PrintToLog(const FString& LogMessage);
	static void PrintWarningToLog(const FString& LogMessage);
	static void PrintErrorToLog(const FString& LogMessage);
};
