#include "Widgets/GenerateDialogueWidget.h"

#include "EasyCsv.h"
#include "EFDFunctionLibrary.h"
#include "BPLib/MultiModalService.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundWave.h"

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 2 || ENGINE_MINOR_VERSION == 1)
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#endif

TArray<float> ConvertStringArrayToFloatArray(const TArray<FString>& InArray)
{
	TArray<float> OutArray;
	for (const auto& Str : InArray)
	{
		OutArray.Add(FCString::Atof(*Str));
	}
	return OutArray;
}

FString UGenerateDialogueWidget::OpenFileDialogAndSelectFileName(bool bIsJson)
{
	TArray<FString> OutFileNames;
	if (!bIsJson)
	{
		UEFDFunctionLibrary::OpenFileDialog("Select a csv file", "c://", "", "CSV files (*.csv)|*.csv",
		                                    EEasyFileDialogFlags::Single,
		                                    OutFileNames);
	}
	else
	{
		UEFDFunctionLibrary::OpenFileDialog("Select a json file", "c://", "", "Json files (*.json)|*.json",
		                                    EEasyFileDialogFlags::Single,
		                                    OutFileNames);
	}

	if (OutFileNames.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("No File Selected"));
		DisplayMessage = TEXT("No File Selected");
		return FString(TEXT("ERROR"));
	}
	FString FullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*OutFileNames[0]);
	return FullPath;
}

FString UGenerateDialogueWidget::ReadCSVFacialCaptureData(const FCharacterSetupData& InputCharacterSetup)
{
	FString CSVFilePath = OpenFileDialogAndSelectFileName();
	CSVFilePath = CSVFilePath.Replace(TEXT("/"),TEXT("\\"));
	UE_LOG(LogTemp, Error, TEXT("CSV File Path: %s"), *CSVFilePath);

	FString CSVResult;
	bool LoadFileSuccess = FFileHelper::LoadFileToString(CSVResult, *CSVFilePath);
	if (!LoadFileSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Load File Error"));
		DisplayMessage = TEXT("Load File Error");
		return FString(TEXT("ERROR"));
	}

	FEasyCsvInfo CsvInfo;
	bool bConvertToInfoSuccess = UEasyCsv::MakeCsvInfoStructFromString(CSVResult, CsvInfo);

	if (!bConvertToInfoSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Convert To Info Error"));
		DisplayMessage = TEXT("Convert To Info Error");
		return FString(TEXT("ERROR"));
	}

	TMap<FName, TArray<float>> ArkitBlendshapeData;
	for (int32 i = 0; i < ArkitBlendshapeNameStringList.Num(); i++)
	{
		bool bSuccess = false;
		auto BSData = UEasyCsv::GetColumnAsStringArray(CsvInfo, ArkitBlendshapeNameStringList[i], bSuccess);
		if (bSuccess)
		{
			ArkitBlendshapeData.Add(ArkitBlendshapeNameList[i], ConvertStringArrayToFloatArray(BSData));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Read %s Column From CSV Error"), *ArkitBlendshapeNameStringList[i]);
		}
	}

	TArray<float> Value = ArkitBlendshapeData[ArkitBlendshapeNameList[0]];
	UE_LOG(LogTemp, Error, TEXT("Frame Length: %d"), Value.Num());

	TArray<TArray<float>> BlendshapeFrameData;

	for (int32 i = 0; i < Value.Num(); i += 2)
	{
		TArray<float> FrameData;

		for (const auto& BS : ArkitBlendshapeNameList)
		{
			if (i > Value.Num())
			{
				break;
			}
			auto res = ArkitBlendshapeData[BS][i];
			FrameData.Add(res);
		}

		BlendshapeFrameData.Add(FrameData);
	}

	UE_LOG(LogTemp, Error, TEXT("Frame Number: %d"), BlendshapeFrameData.Num());

	const FString CharacterName = InputCharacterSetup.Mesh.GetName();

	GenerateProgress = 0.05f;
	DisplayMessage = TEXT("正在生成");

	FGeneratedDialogueAssetsInfo ProcessedInfo{};
	ProcessedInfo.SentenceNumber = 0;
	this->ProcessedInfos.Push(ProcessedInfo);

	auto NewInputSetup = InputCharacterSetup;

	NewInputSetup.UpLipsExtent = 1.0f;
	NewInputSetup.NoseExtent = 1.0f;
	NewInputSetup.SmileExtent = 1.0f;

	auto Service = UMultiModalService::CSVService(
		CharacterName,
		BlendshapeFrameData,
		NewInputSetup,
		FTTSGlobalSettings{this->VoiceVolume, false, this->TtsAnimSmoothOutFrameNum}
	);

	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("UGenerateDialogueWidget::CallAudioToFaceService: A2FService Object Create Error"));
		OnTTSFinish_CallBack(CharacterName, false, "", false, "", 0, 0, 0);
		return "";
	}

	Service->OnMultiModalDataProcessed.AddDynamic(this, &ThisClass::OnCSVFinish_CallBack);

	Service->StartCSVReading();

	GenerateProgress = 0.15f;

	return MainSequencePath;
}

FString UGenerateDialogueWidget::CallAudioToFaceServiceByInfo(USoundWave* InputSoundWave,
                                                              const FCharacterSetupData&
                                                              InputCharacterSetup)
{
	if (bIsWorking)
	{
		DisplayMessage = TEXT("已经在生成，请勿重复点击");
		return "Already Calling, Do not Call Again";
	}

	if (!CleanupData() || !InputSoundWave)
	{
		DisplayMessage = TEXT("错误");
		GenerateProgress = 0.0f;
		return FString(TEXT("ERROR"));
	}

	const FString CharacterName = InputCharacterSetup.Mesh.GetName();

	GenerateProgress = 0.05f;
	DisplayMessage = TEXT("正在生成");

	FGeneratedDialogueAssetsInfo ProcessedInfo{};
	ProcessedInfo.SentenceNumber = 0;
	this->ProcessedInfos.Push(ProcessedInfo);


	TArray<TArray<float>> BlendshapeFrameData;
	for (int32 i = 0; i < 1; i += 1)
	{
		TArray<float> FrameData;
		for (const auto& BS : ArkitBlendshapeNameList)
		{
			FrameData.Add(0);
		}
		BlendshapeFrameData.Add(FrameData);
	}

	auto NewInputSetup = InputCharacterSetup;

	auto Service = UMultiModalService::MultiModalSoundToFaceWithCSVService(
		SoundToFaceAddress,
		InputSoundWave,
		CharacterName,
		NewInputSetup,
		FTTSGlobalSettings{this->VoiceVolume, this->bTtsAnimSmoothOut, this->TtsAnimSmoothOutFrameNum},
		BlendshapeFrameData
	);

	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("UGenerateDialogueWidget::CallAudioToFaceService: A2FService Object Create Error"));
		OnTTSFinish_CallBack(CharacterName, false, "", false, "", 0, 0, 0);
		return "";
	}

	Service->OnMultiModalDataProcessed.AddDynamic(this, &ThisClass::OnA2FFinish_CallBack);
	Service->OnServiceDisconnect.AddDynamic(this, &ThisClass::OnWsDisconnect_CallBack);
	Service->OnA2FOneChunkOver.AddDynamic(this, &ThisClass::UpdateGenerateProgress);

	this->SoundWaveStash = InputSoundWave;

	GenerateProgress = 0.15f;

	return MainSequencePath;
}

FString UGenerateDialogueWidget::CallTTSServiceByInfo(const FString& InputText,
                                                      const FCharacterSetupData& InputCharacterSetup,
                                                      bool bGenerateAudioOnly)
{
	if (bIsWorking)
	{
		DisplayMessage = TEXT("已经在生成，请勿重复点击");
		return "Already Calling, Do not Call Again";
	}

	if (!CleanupData())
	{
		DisplayMessage = TEXT("错误");
		GenerateProgress = 0.0f;
		return FString(TEXT("ERROR"));
	}

	const FString CharacterName = InputCharacterSetup.Mesh.GetName();

	GenerateProgress = 0.05f;
	DisplayMessage = TEXT("正在生成");

	FGeneratedDialogueAssetsInfo ProcessedInfo{};
	ProcessedInfo.SentenceNumber = 0;
	this->ProcessedInfos.Push(ProcessedInfo);

	auto NewInputSetup = InputCharacterSetup;

	auto Service = UMultiModalService::MultiModalTextToSoundService(
		TextToSoundServiceAddress,
		bGenerateAudioOnly,
		0, 1,
		InputText,
		CharacterName,
		NewInputSetup,
		FTTSGlobalSettings{this->VoiceVolume, this->bTtsAnimSmoothOut, this->TtsAnimSmoothOutFrameNum}
	);

	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("UGenerateDialogueWidget::CallAudioToFaceService: A2FService Object Create Error"));
		OnTTSFinish_CallBack(CharacterName, false, "", false, "", 0, 0, 0);
		return "";
	}

	Service->OnMultiModalDataProcessed.AddDynamic(this, &ThisClass::OnTTSFinish_CallBack);
	Service->OnServiceDisconnect.AddDynamic(this, &ThisClass::OnWsDisconnect_CallBack);

	GenerateProgress = 0.15f;

	return MainSequencePath;
}

FString UGenerateDialogueWidget::ReadMetahumanS2FDataWithExtraCSV(const FCharacterSetupData& InputCharacterSetup,
                                                                  bool bAddCSV,
                                                                  bool bAddBlink,
                                                                  float Extent)
{
	FString JsonFilePath = OpenFileDialogAndSelectFileName(true);
	JsonFilePath = JsonFilePath.Replace(TEXT("/"),TEXT("\\"));
	UE_LOG(LogTemp, Error, TEXT("Metahuman S2F Json File Path: %s"), *JsonFilePath);

	FString JsonResult;
	bool LoadFileSuccess = FFileHelper::LoadFileToString(JsonResult, *JsonFilePath);
	if (!LoadFileSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("Load File Error"));
		DisplayMessage = TEXT("Load File Error");
		return FString(TEXT("ERROR"));
	}

	TMap<FString, TArray<float>> ArkitBlendshapeData;

	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<CHAR>::Create(JsonResult);
	TSharedPtr<FJsonObject> ExpressionJsonObject;
	if (!FJsonSerializer::Deserialize(JsonReader, ExpressionJsonObject))
	{
		UE_LOG(LogTemp,
		       Error,
		       TEXT("GenerateExpressionAnim: Json File Format Error, Please Use Right Json Format"));
		return "";
	}

	for (const auto& BS : MetahumanS2FBlendshapeList)
	{
		auto ValueList = ExpressionJsonObject->GetArrayField(*BS);
		TArray<float> Values;
		for (const auto& V : ValueList)
		{
			Values.Add(V->AsObject()->GetNumberField(TEXT("value")));
		}

		ArkitBlendshapeData.Add(BS, Values);
	}

	TArray<float> MetaArkitValue = ArkitBlendshapeData[MetahumanS2FBlendshapeList[0]];
	UE_LOG(LogTemp, Error, TEXT("Frame Length: %d"), MetaArkitValue.Num());

	TArray<TArray<float>> BlendshapeFrameData;

	for (int32 i = 0; i < MetaArkitValue.Num(); i += 1)
	{
		TArray<float> FrameData;

		for (const auto& BS : ArkitBlendshapeNameStringListWithoutMouthClose)
		{
			if (i > MetaArkitValue.Num())
			{
				break;
			}

			if (i == 0)
			{
				UE_LOG(LogTemp, Error, TEXT("Finding Blendshape: %s"), *BS);
			}

			auto res = ArkitBlendshapeData.Find(BS);
			auto res_data = 0.f;
			if (res)
			{
				res_data = (*res)[i];
			}

			auto clamped_result = FMath::Clamp(res_data, 0.f, 1.f);
			clamped_result *= Extent;

			FrameData.Add(clamped_result);
		}

		BlendshapeFrameData.Add(FrameData);
	}

	UE_LOG(LogTemp, Error, TEXT("Frame Number: %d"), BlendshapeFrameData.Num());

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	///
	if (bAddCSV)
	{
		FString CSVFilePath = OpenFileDialogAndSelectFileName();
		CSVFilePath = CSVFilePath.Replace(TEXT("/"),TEXT("\\"));
		UE_LOG(LogTemp, Error, TEXT("CSV File Path: %s"), *CSVFilePath);

		FString CSVResult;
		if (bool LoadCSVFileSuccess = FFileHelper::LoadFileToString(CSVResult, *CSVFilePath); !LoadCSVFileSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("Load File Error"));
			DisplayMessage = TEXT("Load File Error");
			return FString(TEXT("ERROR"));
		}

		FEasyCsvInfo CsvInfo;

		if (bool bConvertToInfoSuccess = UEasyCsv::MakeCsvInfoStructFromString(CSVResult, CsvInfo); !
			bConvertToInfoSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("Convert To Info Error"));
			DisplayMessage = TEXT("Convert To Info Error");
			return FString(TEXT("ERROR"));
		}

		TMap<FName, TArray<float>> CSVArkitBlendshapeData;
		for (int32 i = 0; i < ArkitBlendshapeNameStringList.Num(); i++)
		{
			bool bSuccess = false;
			auto BSData = UEasyCsv::GetColumnAsStringArray(CsvInfo, ArkitBlendshapeNameStringList[i], bSuccess);
			if (bSuccess)
			{
				CSVArkitBlendshapeData.Add(ArkitBlendshapeNameList[i], ConvertStringArrayToFloatArray(BSData));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Read %s Column From CSV Error"), *ArkitBlendshapeNameStringList[i]);
			}
		}

		TArray<TArray<float>> CSVBlendshapeFrameData;

		TArray<float> CSVArkitValue = CSVArkitBlendshapeData[ArkitBlendshapeNameList[0]];
		UE_LOG(LogTemp, Error, TEXT("CSV Frame Length: %d"), CSVArkitValue.Num());

		for (int32 i = 0; i < CSVArkitValue.Num(); i += 2)
		{
			TArray<float> FrameData;

			for (const auto& BS : ArkitBlendshapeNameList)
			{
				if (i > CSVArkitValue.Num())
				{
					break;
				}
				auto res = CSVArkitBlendshapeData[BS][i];
				if (BS.ToString().Contains(TEXT("mouth")) || BS.ToString().Contains(TEXT("jaw")) || BS.ToString().
					Contains(TEXT("eyeBlink")))
				{
					res = 0.0;
				}

				FrameData.Add(res);
			}

			CSVBlendshapeFrameData.Add(FrameData);
		}

		const auto csv_data_length = CSVBlendshapeFrameData.Num();

		TArray<TArray<float>> FinalResult{};
		for (int32 index = 0; index < BlendshapeFrameData.Num(); index += 1)
		{
			TArray<float> FinalFrameData{};
			const int32 corresponding_csv_index = index % csv_data_length;
			for (int32 i = 0; i < BlendshapeFrameData[index].Num(); i++)
			{
				auto res = FMath::Max(BlendshapeFrameData[index][i],
				                      CSVBlendshapeFrameData[corresponding_csv_index][i]);
				FinalFrameData.Add(res);
			}

			FinalResult.Add(FinalFrameData);
		}

		BlendshapeFrameData = FinalResult;
	}

	const FString CharacterName = InputCharacterSetup.Mesh.GetName();

	GenerateProgress = 0.15f;
	DisplayMessage = TEXT("正在生成");

	FGeneratedDialogueAssetsInfo ProcessedInfo{};
	ProcessedInfo.SentenceNumber = 0;
	this->ProcessedInfos.Push(ProcessedInfo);

	auto NewInputSetup = InputCharacterSetup;

	NewInputSetup.UpLipsExtent = 1.0f;
	NewInputSetup.DownLipsExtent = 1.0f;
	NewInputSetup.NoseExtent = 1.0f;
	NewInputSetup.SmileExtent = 1.0f;
	NewInputSetup.SmileBase = 0.0f;
	NewInputSetup.BlinkSettings.bShouldBlink = bAddBlink;

	auto Service = UMultiModalService::CSVService(
		CharacterName,
		BlendshapeFrameData,
		NewInputSetup,
		FTTSGlobalSettings{this->VoiceVolume, false, this->TtsAnimSmoothOutFrameNum}
	);

	if (!Service)
	{
		UE_LOG(LogTemp, Error, TEXT("UGenerateDialogueWidget::CallAudioToFaceService: A2FService Object Create Error"));
		OnTTSFinish_CallBack(CharacterName, false, "", false, "", 0, 0, 0);
		return "";
	}

	Service->OnMultiModalDataProcessed.AddDynamic(this, &ThisClass::OnCSVFinish_CallBack);

	Service->StartCSVReading();
	
	return MainSequencePath;
}

bool UGenerateDialogueWidget::CleanupData()
{
	this->MainSequencePath.Empty();
	this->TotalSentenceNumber = 0;
	this->CurrentSentenceNumber = 0;
	this->TotalFrameNumber = 0;
	this->OriginInfos.Empty();
	this->ProcessedInfos.Empty();
	this->SoundWaveStash = nullptr;
	this->GenerateProgress = 0.0f;
	this->DisplayMessage = TEXT("等待");
	return true;
}

void UGenerateDialogueWidget::OnTTSFinish_CallBack(FString CharacterName,
                                                   bool bGenAnimSuccess,
                                                   FString AnimPath,
                                                   bool bGenAudioSuccess,
                                                   FString AudioPath,
                                                   float AudioTime,
                                                   int32 CurrentIndex,
                                                   int32 TotalIndex)
{
	// 将语音服务生成的音频和口型动画文件的路径进行记录
	this->ProcessedInfos[CurrentIndex].AudioPath = AudioPath;
	this->ProcessedInfos[CurrentIndex].AnimPath = AnimPath;
	this->ProcessedInfos[CurrentIndex].AudioLength = AudioTime;

	UE_LOG(LogTemp, Warning,
	       TEXT("OnTTSAudioAndAnimGenerateFinish_CallBack, Current Sentence: %d, Total Sentence: %d"),
	       this->CurrentSentenceNumber + 1, this->TotalSentenceNumber);

	this->CurrentSentenceNumber = 0;
	OnEverythingFinish();
}

void UGenerateDialogueWidget::OnWsDisconnect_CallBack(bool bIsClean)
{
	UE_LOG(LogTemp, Error,
	       TEXT("Enter Websocket Disconnect Call Back, Current Sentence: %d, Total Sentence: %d"),
	       this->CurrentSentenceNumber, this->TotalSentenceNumber);
}

void UGenerateDialogueWidget::OnA2FFinish_CallBack(FString CharacterName,
                                                   bool bGenAnimSuccess,
                                                   FString AnimPath,
                                                   bool bGenAudioSuccess,
                                                   FString AudioPath,
                                                   float AudioTime,
                                                   int32 CurrentIndex,
                                                   int32 TotalIndex)
{
	GenerateProgress = 1.0f;
	DisplayMessage = TEXT("生成完成");
	bIsWorking = false;
}

void UGenerateDialogueWidget::OnCSVFinish_CallBack(FString CharacterName, bool bGenAnimSuccess, FString AnimPath,
                                                   bool bGenAudioSuccess, FString AudioPath, float AudioTime,
                                                   int32 CurrentIndex, int32 TotalIndex)
{
	UE_LOG(LogTemp, Error, TEXT("OnCSVFinish_CallBack, Current Sentence: %d, Audio Time: %f"),
	       CurrentIndex + 1, AudioTime);
	GenerateProgress = 1.0f;
	DisplayMessage = TEXT("生成完成");
	bIsWorking = false;
}

void UGenerateDialogueWidget::UpdateGenerateProgress(int32 CurrentChunk, int32 TotalChunk)
{
	const float Progress = 0.75f / static_cast<float>(TotalChunk);
	GenerateProgress += Progress;
}

void UGenerateDialogueWidget::OnEverythingFinish()
{
	GenerateProgress = 1.0f;
	DisplayMessage = TEXT("生成完成");
	bIsWorking = false;
}
