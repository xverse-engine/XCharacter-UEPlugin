// Copyright Jared Therriault 2019, 2022

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "EasyCsv.generated.h"

USTRUCT(BlueprintType)
struct FEasyCsvStringValueArray
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "easyCSV")
		TArray<FString> StringValues;
};

USTRUCT(BlueprintType)
struct FEasyCsvInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "easyCSV")
		TMap<FName, FEasyCsvStringValueArray> CSV_Map;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "easyCSV")
		TArray<FName> CSV_Keys;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "easyCSV")
		TArray<FString> CSV_Headers;
};

UCLASS()
class EASYCSV_API UEasyCsv : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	static TArray<TArray<FString>> ReadCsv(const FString& CsvContent);

	/**
	 * Saves a new file using an input string. Always overwrites and does not validate paths, so please be sure the directory exists. 
	 * Meant for saving a CSV, but can be used for any text-based file. Returns true if write is successful.
	 * @param InString The string to save to a file
	 * @param InDirectory The folder in which to save the new file
	 * @param Filename The new file's name
	 * @param Extension The file extension for the new file
	 */
	UFUNCTION(BlueprintCallable, Category = "easyCSV|Utlities")
		static bool SaveStringToFile(
			const FString& InString, const FString InDirectory, const FString Filename = "New_CSV", const FString Extension = ".csv");

	/**
	 * Saves a new file using an input string. Always overwrites and does not validate paths, so please be sure the directory exists. 
	 * Meant for saving a CSV, but can be used for any text-based file. Returns true if write is successful.
	 * @param InString The string to save to a file
	 * @param InFullPath The folder, filename and extension used to save the new file
	 */
	UFUNCTION(BlueprintCallable, Category = "easyCSV|Utlities")
		static bool SaveStringToFileWithFullPath(const FString& InString, const FString InFullPath);

	/**
	 * Loads a text-based file from the specified path and outputs its contents as a string.
	 * @return Whether or not the file could be successfully loaded
	 * @param InPath The file path to the file you'd like to load
	 * @param OutString The contents of the file as a string 
	 */
	UFUNCTION(BlueprintCallable, Category = "Google Sheets Operator|Utilities")
		static bool LoadStringFromLocalFile(const FString& InPath, FString& OutString);

	/**
	 * Returns all values in a column, given the name of said column in the CSV, as an array of strings.
	 * @return All values in a column, given the name of said column in the CSV, as an array of strings
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 * @param ColumnName The name of the column in the CSV
	 * @param Success Whether or not the column could be found by name
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static TArray<FString> GetColumnAsStringArray(const FEasyCsvInfo& CSV_Info, const FString& ColumnName, bool& Success);

	/**
	 * Returns all values in a row given the key of a row in the CSV, as an array of strings.
	 * @return All values in a row given the key of a row in the CSV, as an array of strings
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 * @param RowKey The key of the row in the CSV
	 * @param Success Whether or not the row could be found by key
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static TArray<FString> GetRowAsStringArray(const FEasyCsvInfo& CSV_Info, const FName RowKey, bool& Success);

	/**
	 * Returns a single value given a column name and a row key as a string.
	 * @return A single value given a column name and a row key as a string
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 * @param ColumnName The name of the column in the CSV
	 * @param RowKey The key for the given row in the CSV
	 * @param Success Whether or not the value could be found by column name and/or row key
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static FString GetRowValueAsString(const FEasyCsvInfo& CSV_Info, const FString& ColumnName, const FName RowKey, bool& Success);

	/**
	 * Returns the keys for the given CSV as an array of FNames. 
	 * This is deterministic; the keys will always be in the same order and will correspond to the order of the CSV. 
	 * If no CSV is loaded, this array will be blank.
	 * @return An array of FNames corresponding to the CSV's row keys
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static TArray<FName> GetMapKeys(const FEasyCsvInfo& CSV_Info);

	/**
	 * Returns the headers/column names for the given CSV as an array of FString, not counting the "Key" header.
	 * If no CSV is loaded, this array will be blank.
	 * @return An array of column names / headers as FString
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static TArray<FString> GetMapHeaders(const FEasyCsvInfo& CSV_Info);

	/**
	 * Returns the index of the given key in the CSV. -1 will be returned if the key is not found or if the CSV is not loaded.
	 * This function can be used to find a specific row index in the CSV given the row key.
	 * @return The index of the given key in the CSV
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 * @param Key The key for the given row in the CSV
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static int32 GetMapKeyIndex(const FEasyCsvInfo& CSV_Info, const FName Key);

	/**
	 * Returns the number of rows in a given CSV, not counting the column headers.
	 * If no valid CSV_Info struct is passed in, this will return -1.
	 * @return An integer reflecting the number of rows in a given CSV_Info struct's CSV map.
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static int32 GetRowCount(const FEasyCsvInfo& CSV_Info);

	/**
	 * Returns the number of columns in a given CSV, not counting the Row Keys.
	 * If no valid CSV_Info struct is passed in, this will return -1.
	 * @return An integer reflecting the number of columns in a given CSV_Info struct's CSV map.
	 * @param CSV_Info A structure containing parsed CSV data. Can be created using MakeCSV_InfoFromString.
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Post-Parse Operations")
		static int32 GetColumnCount(const FEasyCsvInfo& CSV_Info);

	/**
	 * Used to parse a CSV into a map containing each cell's data as part of an array of FString. This is the node you want to start with.
	 * @return Whether or not the parsing was successful
	 * @param OutCsvInfo A struct with parsed CSV information. This can be used to access the information directly or pass into other easyCSV functions.
	 * @param InString This is the string data found inside the CSV file. Can be loaded from a file using LoadStringFromFile.
	 * @param ParseHeaders If true, the parser will expect the first row of the CSV to be column labels, or headers. If false, vales will be generated.
	 * @param ParseKeys If true, the parser will expect the first column of the CSV to be row labels, or keys. If false, values will be generated.
	 */
	UFUNCTION(BlueprintCallable, Category = "easyCSV|Main", meta = (Keywords = "parse", DisplayName = "Make CSV Info From String"))
		static bool MakeCsvInfoStructFromString(
			FString InString, FEasyCsvInfo& OutCsvInfo, bool ParseHeaders = true, bool ParseKeys = true);

	/**
	 * Used to parse a CSV into a map containing each cell's data as part of an array of FString. This is the node you want to start with.
	 * @return Whether or not the parsing was successful
	 * @param OutCsvInfo A struct with parsed CSV information. This can be used to access the information directly or pass into other easyCSV functions.
	 * @param InPath This is the path to the CSV file
	 * @param ParseHeaders If true, the parser will expect the first row of the CSV to be column labels, or headers. If false, vales will be generated.
	 * @param ParseKeys If true, the parser will expect the first column of the CSV to be row labels, or keys. If false, values will be generated.
	 */
	UFUNCTION(BlueprintCallable, Category = "easyCSV|Main", meta = (Keywords = "parse", DisplayName = "Make CSV Info From File"))
		static bool MakeCsvInfoStructFromFile(
		const FString& InPath, FEasyCsvInfo& OutCsvInfo, bool ParseHeaders = true, bool ParseKeys = true);

	/**
	 * Returns true if the specified string represents a struct, array, map or set.
	 * @return bool
	 * @param InString A string that may represent a container. If you've already parsed a CSV and have nested structs, arrays, maps or sets in specific cells this method will detect them.
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|String")
		static bool DoesStringRepresentContainer(FString InString);

	/**
	 * Returns an const FString& with all escaped characters turned into characters ('\"', '\r', '\n', etc). Also removes all leading backslashes.
	 * @return const FString& A string with all escaped characters converted into standard characters ('\"', '\r', '\n', etc)
	 * @param InString A string that may contain escaped characters ('\"', '\r', '\n', etc)
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|String")
		static FString ReplaceEscapedCharacters(const FString& InString)
		{
			return InString.ReplaceEscapedCharWithChar();
		}

	/**
	 * Returns an const FString& with all characters that could be escaped turned into escaped characters ('\"', '\r', '\n', etc)
	 * @return const FString& A string with all escaped characters converted into standard characters ('\"', '\r', '\n', etc)
	 * @param InString A string that may contain escaped characters ('\"', '\r', '\n', etc)
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|String")
		static FString EscapeCharacters(const FString& InString)
		{
			return InString.ReplaceCharWithEscapedChar();
		}

	/**
	 * Returns the namespace (if applicable), key (if applicable), and source string from an const FString& that represents a localizable FText with metadata (NSLOCTEXT, LOCTEXT, INVTEXT).
	 * This function can be used to strip the metadata out of an const FString& that represents a localizable FText if you just use the return value 'SourceString.'
	 * @param InString A string that represents a localizable FText with metadata (NSLOCTEXT, LOCTEXT, INVTEXT)
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|String")
		static void GetFTextComponentsFromRepresentativeFString(FString InString, FString& Namespace, FString& Key, FString& SourceString);

	//Conversions

	/**
	 * Convert a string into an FRotator. Supports more formats than the built-in engine conversion.
	 * @return FRotator
	 * @param InString A rotator expressed as a string (ex: "P=0 Y=0 R=0" or "Pitch=0 Yaw=0 Roll=0"). Does not work for Quaternions expressed as string (use ConvertStringToQuat or ConvertQuatStringToRotator for that).
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Conversion")
		static FRotator ConvertStringToRotator(FString InString)
		{
			if (InString.Contains("Pitch", ESearchCase::IgnoreCase))
			{
				InString =
					InString.Replace(TEXT("Pitch"), TEXT("P"), ESearchCase::IgnoreCase)
					.Replace(TEXT("Roll"), TEXT("R"), ESearchCase::IgnoreCase)
					.Replace(TEXT("Yaw"), TEXT("Y"), ESearchCase::IgnoreCase);
			}

			FRotator ReturnVal;
			ReturnVal.InitFromString(InString);
			return ReturnVal;
		}

	/**
	 * Converts a string into an array of FRotators. Supports more formats than the built-in engine conversion.
	 * @return Array of FRotators
	 * @param InString An array of rotators expressed as a string (ex: "("P=0 Y=0 R=0","P=1 Y=1 R=1","P=2 Y=2 R=2")"). Does not work for Quaternions expressed as string (use ConvertStringToQuatArray or ConvertQuatStringToRotatorArray for that).
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Conversion")
		static TArray<FRotator> ConvertStringToRotatorArray(const FString& InString)
		{
			TArray<FRotator> ReturnVal;

			TArray<TArray<FString>> Arr = ReadCsv(InString);

			for (int32 index = 0; index < Arr[0].Num(); index++)
			{
				ReturnVal.Add(ConvertStringToRotator(Arr[0][index]));
			}

			return ReturnVal;
		}

	/**
	 * Convert a string into an FQuat.
	 * @return FQuat
	 * @param InString An FQuat expressed as a string (ex: "X=0 Y=0 Z=0 W=0"). ConvertQuatStringToRotator can convert quaternions to FRotators.
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Conversion")
		static FQuat ConvertStringToQuat(const FString& InString)
		{
			FQuat ReturnVal;
			ReturnVal.InitFromString(InString);
			return ReturnVal;
		}

	/**
	 * Convert a string into an array of FQuats.
	 * @return Array of FQuat
	 * @param InString An array of FQuats expressed as a string (ex: "("X=0 Y=0 Z=0 W=0","X=0 Y=0 Z=0 W=0","X=0 Y=0 Z=0 W=0")"). ConvertQuatStringToRotatorArray can convert quaternions to FRotators.
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Conversion")
		static TArray<FQuat> ConvertStringToQuatArray(const FString& InString)
		{
			TArray<FQuat> ReturnVal;

			TArray<TArray<FString>> Arr = ReadCsv(InString);

			for (int32 index = 0; index < Arr[0].Num(); index++)
			{
				ReturnVal.Add(ConvertStringToQuat(Arr[0][index]));
			}

			return ReturnVal;
		}

	/**
	 * Convert a string that represents an FQuat into an FRotator.
	 * @return FRotator
	 * @param InString An FQuat expressed as a string (ex: "X=0 Y=0 Z=0 W=0").
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Conversion")
		static FRotator ConvertQuatStringToRotator(const FString& InString)
		{
			FQuat Quatern;
			Quatern.InitFromString(InString);
			return Quatern.Rotator();
		}

	/**
	 * Convert a string into an array of FRotators.
	 * @return Array of FRotator
	 * @param InString An array of FQuats expressed as a string (ex: "("X=0 Y=0 Z=0 W=0","X=0 Y=0 Z=0 W=0","X=0 Y=0 Z=0 W=0")").
	 */
	UFUNCTION(BlueprintPure, Category = "easyCSV|Conversion")
		static TArray<FRotator> ConvertQuatStringToRotatorArray(const FString& InString)
		{
			TArray<FRotator> ReturnVal;

			TArray<TArray<FString>> Arr = ReadCsv(InString);

			for (int32 index = 0; index < Arr[0].Num(); index++)
			{
				ReturnVal.Add(ConvertQuatStringToRotator(Arr[0][index]));
			}

			return ReturnVal;
		}
};

/* Changelog:
 * --- 4.27.4 ---
 * Removed unnecessary includes
 * Updated copyright notices
 * Now parses using built-in parser, no more empty row errors! But now no more custom delimiters or custom wrappers. Sorry!
 * Fixed an issue in which calling MakeCsvInfoFromString/File multiple times would load the same data into the map multiple times
 * FCSV_Info is now FEasyCsvInfo
 * FStringValueArray is now FEasyCsvStringValueArray
 * LoadStringFromFile has been deprecated, use LoadStringFromLocalFile
 * Updated copyright notices
*/
