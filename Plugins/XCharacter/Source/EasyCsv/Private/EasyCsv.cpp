// Copyright Jared Therriault 2019, 2022

#include "EasyCsv.h"

#include "EasyCsvModule.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "HAL/PlatformFileManager.h"
#else
#include "HAL/PlatformFilemanager.h"
#endif

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

TArray<TArray<FString>> UEasyCsv::ReadCsv(const FString& CsvContent)
{
	TArray<TArray<FString>> Lines;
	
	const FCsvParser Parser(CsvContent);

	for (const auto& Row : Parser.GetRows())
	{
		TArray<FString> Line;

		for (const auto& Cell : Row)
		{
			if (Cell)
			{
				Line.Add(Cell);
			}
			else
			{
				Line.Add("");
			}
		}
		
		Lines.Add(Line);
	}

	return Lines;
}

bool UEasyCsv::SaveStringToFile(
	const FString& InString, const FString InDirectory, const FString Filename, const FString Extension)
{
	//FEasyCsvModule::Print(FString::Printf(TEXT("%hs: Start: Filename: %s, Extension: %s"), __FUNCTION__, *Filename, *Extension));
	FString Directory = InDirectory;
	
	FPaths::NormalizeDirectoryName(Directory);
	FPaths::RemoveDuplicateSlashes(Directory);
	
	const FString FullPath =
		FPaths::Combine(Directory, FPaths::SetExtension(FPaths::MakeValidFileName(Filename), Extension));
	
		FText Error = FText::FromString(
			FString::Printf(TEXT("%hs: No error"), __FUNCTION__));
			
	if (FFileHelper::IsFilenameValidForSaving(FullPath, Error))
	{
		FEasyCsvModule::Print("Saving file to " + FullPath);
		return FFileHelper::SaveStringToFile(InString, *FullPath);
	}
	
	FEasyCsvModule::Print(
		FString::Printf(
			TEXT("%hs: The provided save path is not valid ('%s'). Original error: %s"),
			__FUNCTION__, *FullPath, *Error.ToString()),
		FEasyCsvModule::ELogType::Error);
	return false;
}

bool UEasyCsv::SaveStringToFileWithFullPath(const FString& InString, const FString InFullPath)
{
	const FString Directory = FPaths::GetPath(InFullPath);
	const FString Filename = FPaths::GetBaseFilename(InFullPath, true);
	const FString Extension = FPaths::GetExtension(InFullPath);

	return SaveStringToFile(InString, Directory, Filename, Extension);
}

bool UEasyCsv::LoadStringFromLocalFile(const FString& InPath, FString& OutString)
{
	FString Directory = FPaths::GetPath(InPath);
	const FString Filename = FPaths::GetBaseFilename(InPath, true);
	const FString Extension = FPaths::GetExtension(InPath);
	
	FPaths::NormalizeDirectoryName(Directory);
	FPaths::RemoveDuplicateSlashes(Directory);
	
	const FString FullPath =
		FPaths::Combine(Directory, FPaths::SetExtension(FPaths::MakeValidFileName(Filename), Extension));
	
	FPlatformFileManager::Get().GetPlatformFile().BypassSecurity(true);
	const bool bSuccess = FFileHelper::LoadFileToString(OutString, *InPath) && OutString.Len() > 10;
	FPlatformFileManager::Get().GetPlatformFile().BypassSecurity(false);

	return bSuccess;
}

TArray<FString> UEasyCsv::GetColumnAsStringArray(const FEasyCsvInfo& CSV_Info, const FString& ColumnName, bool& Success)
{
	TArray<FString> ReturnValue;

	const int32 HeaderIndex = CSV_Info.CSV_Headers.Find(ColumnName);

	if (HeaderIndex >= 0)
	{
		Success = true;
		TArray<FName> KeysLocal = CSV_Info.CSV_Keys;

		for (int32 KeyIndex = 0; KeyIndex < KeysLocal.Num(); KeyIndex++)
		{
			ReturnValue.Add(CSV_Info.CSV_Map[KeysLocal[KeyIndex]].StringValues[HeaderIndex]);
		}
	}

	return ReturnValue;
}

TArray<FString> UEasyCsv::GetRowAsStringArray(const FEasyCsvInfo& CSV_Info, const FName RowKey, bool& Success)
{
	TArray<FString> Row;
	if (CSV_Info.CSV_Keys.Contains(RowKey))
	{
		Success = true;
		Row = CSV_Info.CSV_Map[RowKey].StringValues;
	}
	return Row;
}

FString UEasyCsv::GetRowValueAsString(const FEasyCsvInfo& CSV_Info, const FString& ColumnName, const FName RowKey, bool& Success)
{
	FString ReturnValue = "";

	const int32 HeaderIndex = CSV_Info.CSV_Headers.Find(ColumnName);

	if (HeaderIndex >= 0 && CSV_Info.CSV_Map.Contains(RowKey))
	{
		Success = true;
		const TMap<FName, FEasyCsvStringValueArray> LocalMap = CSV_Info.CSV_Map;
		TArray<FName> KeysLocal;
		LocalMap.GetKeys(KeysLocal);
		FEasyCsvStringValueArray ValueArray = LocalMap.FindRef(RowKey);
		ReturnValue = ValueArray.StringValues[HeaderIndex];
	}

	return ReturnValue;
}

TArray<FName> UEasyCsv::GetMapKeys(const FEasyCsvInfo& CSV_Info)
{
	return CSV_Info.CSV_Keys;
}

TArray<FString> UEasyCsv::GetMapHeaders(const FEasyCsvInfo& CSV_Info)
{
	return CSV_Info.CSV_Headers;
}

int32 UEasyCsv::GetMapKeyIndex(const FEasyCsvInfo& CSV_Info, const FName Key)
{
	int32 ReturnValue = -1;
	const TArray<FName> Keys = GetMapKeys(CSV_Info);

	if (Keys.Num() > 0)
	{
		ReturnValue = Keys.Find(Key);
	}

	return ReturnValue;
}

int32 UEasyCsv::GetRowCount(const FEasyCsvInfo& CSV_Info)
{
	return CSV_Info.CSV_Keys.Num();
}

int32 UEasyCsv::GetColumnCount(const FEasyCsvInfo& CSV_Info)
{
	return CSV_Info.CSV_Headers.Num();
}

bool UEasyCsv::MakeCsvInfoStructFromString(FString InString, FEasyCsvInfo& OutCsvInfo, bool ParseHeaders, bool ParseKeys)
{
	// Clear current values
	OutCsvInfo = FEasyCsvInfo();
	
	// Provision string
	// Remove encapsulating parentheses
	if (InString.Left(1) == "(") { InString = InString.RightChop(1); }
	if (InString.Right(1) == ")") { InString = InString.LeftChop(1); }

	TArray<TArray<FString>> FullSheet = ReadCsv(InString);

	if (FullSheet.Num() == 0)
	{
		FEasyCsvModule::Print(
			FString::Printf(TEXT("%hs: Unable to load the file specified."), __FUNCTION__),
			FEasyCsvModule::ELogType::Error);
		return false;
	}

	if (ParseHeaders) //Get CSV_Headers
	{
		OutCsvInfo.CSV_Headers = FullSheet[0];
		if (ParseKeys)
		{
			OutCsvInfo.CSV_Headers.RemoveAt(0, 1, true); //  If ParseKeys == true, take the key header out
		}
	}
	else // Otherwise Generate Headers
	{
		TArray<FString> GeneratedHeaders;
		// If ParseKeys == true, start at 1 to avoid 'Key' header
		for (uint8 HeaderCount = (ParseKeys ? 1 : 0); HeaderCount < FullSheet[0].Num(); HeaderCount++) 
		{
			GeneratedHeaders.Add("Header" + FString::FromInt(GeneratedHeaders.Num())); // Header0, Header1, ... Header13 ...
		}

		OutCsvInfo.CSV_Headers = GeneratedHeaders;
	}

	// If ParseHeaders == true, start at 1 to avoid creating row for headers
	for (int32 LineIndex = (ParseHeaders ? 1 : 0); LineIndex < FullSheet.Num(); LineIndex++)
	{
		TArray<FString> Row = FullSheet[LineIndex];

		FName LineKey; 
		
		if (ParseKeys)
		{
			LineKey = FName(*Row[0]);
			Row.RemoveAt(0, 1, true); //Take the key out of the row
		}
		else // Otherwise Generate keys
		{
			const FString& NewLineName = "Row" + FString::FromInt(LineIndex - (ParseHeaders ? 1 : 0));
			LineKey = FName(*NewLineName); // Row0, Row1, ... Row13, ... Row228 ...
		}
			
		OutCsvInfo.CSV_Keys.Add(LineKey);
		FEasyCsvStringValueArray NewTextArray;
		NewTextArray.StringValues = Row;
		OutCsvInfo.CSV_Map.Add(LineKey, NewTextArray);
	}

	return true;
}

bool UEasyCsv::MakeCsvInfoStructFromFile(const FString& InPath, FEasyCsvInfo& OutCsvInfo, bool ParseHeaders, bool ParseKeys)
{
	FString LoadedCSV;
	const bool SuccessfulLoad = LoadStringFromLocalFile(InPath, LoadedCSV);

	if (SuccessfulLoad)
	{
		return MakeCsvInfoStructFromString(
			LoadedCSV, OutCsvInfo, ParseHeaders, ParseKeys);
	}
	
	FEasyCsvModule::Print(
		FString::Printf(TEXT("%hs: Unable to load the file specified."), __FUNCTION__),
		FEasyCsvModule::ELogType::Error);
	return false;
}

bool UEasyCsv::DoesStringRepresentContainer(FString InString)
{
	const FString WrapperCharacter = "\"";
	const char Wrapper = WrapperCharacter.ReplaceEscapedCharWithChar().GetCharArray()[0];

	bool IsContainerString = false;

	// Remove quotes from beginning and end
	if (InString.Len() > 0 && InString[0] == '"')
	{
		do
		{
			InString = InString.RightChop(1);
		} while (InString[0] == '"');

		if (InString.Len() > 0) //two of the same ifs because the previous while loop could wipe out all text if only quotes exist
		{
			while (InString[InString.Len() - 1] == '"')
			{
				InString = InString.LeftChop(1);
			}
		}
	}

	// Basically, if we see an opening parenthesis before a wrapper then it's a container. If it's a wrapper first, it's NOT a valid container.
	// Any other character we skip and keep looking

	for (wchar_t c : InString) { // use wchar_t to support a wider character set (accents, Cyrillic, etc)

		if (c == '(')
		{
			IsContainerString = true;
			break;
		}
		else if (c == Wrapper)
		{
			break;
		}
	}

	return IsContainerString;
}

void UEasyCsv::GetFTextComponentsFromRepresentativeFString(FString InString, FString& Namespace, FString& Key, FString& SourceString)
{
	if (InString.Contains("LOCTEXT"))
	{
		// First strip out the literal
		InString = InString.Replace(TEXT("NSLOCTEXT"), TEXT("")).Replace(TEXT("LOCTEXT"), TEXT(""));

		FEasyCsvInfo ParsedInfo;
		MakeCsvInfoStructFromString(InString, ParsedInfo, false, false);

		int32 MemberCount = ParsedInfo.CSV_Headers.Num();
		if (MemberCount == 2) // This is LOCTEXT (only Key and Source)
		{
			Key = ParsedInfo.CSV_Map[FName(*FString("Row0"))].StringValues[0].TrimStartAndEnd();
			SourceString = ParsedInfo.CSV_Map[FName(*FString("Row0"))].StringValues[1].TrimStartAndEnd();
		}
		else if (MemberCount == 3) // This is NSLOCTEXT (Namespace, Key and Source)
		{
			Namespace = ParsedInfo.CSV_Map[FName(*FString("Row0"))].StringValues[0].TrimStartAndEnd();
			Key = ParsedInfo.CSV_Map[FName(*FString("Row0"))].StringValues[1].TrimStartAndEnd();
			SourceString = ParsedInfo.CSV_Map[FName(*FString("Row0"))].StringValues[2].TrimStartAndEnd();
		}
	}

	if (InString.Contains("INVTEXT"))
	{
		SourceString = InString.Replace(TEXT("INVTEXT(\""), TEXT("")).Replace(TEXT("\")"), TEXT(""));
	}
}
