// Fill out your copyright notice in the Description page of Project Settings.

#include "BPLib/MultiModalService.h"

#include "EditorAssetLibrary.h"
#include "XSequencerDefines.h"

#include "JsonObjectConverter.h"
#include "XSequencer.h"

#include "PackageTools.h"
#include "RuntimeAudioExporter.h"
#include "RuntimeAudioImporterLibrary.h"
#include "UObject/SavePackage.h"

#include "Serialization/JsonSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundWave.h"

#include "Factories/AnimSequenceFactory.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 2 || ENGINE_MINOR_VERSION == 1)
#include "Misc/Base64.h"
#endif

DEFINE_LOG_CATEGORY(MultimodalServiceCall);

const FString GFacialAnimationFolder = "/Game/YDAutomation/Animation/";
const FString GAudioFolder = "/Game/YDAutomation/Audio/";
const FString GExpressionFolder = "/Game/YDAutomation/Expression/";
constexpr int32 GAudioBase64ChunkSize = 94080;


UMultiModalService* UMultiModalService::MultiModalTextToSoundService(const FString& NewTTSAddress,
                                                                     const bool bGenerateAudioOnly,
                                                                     const int32 CurrentSentence,
                                                                     const int32 TotalSentenceNumber,
                                                                     const FString& SpeakContent,
                                                                     const FString& CharacterName,
                                                                     const FCharacterSetupData& CharacterSetup,
                                                                     const FTTSGlobalSettings& GlobalSettings)
{
	// ----------------------------------------- Validate Inputs -----------------------------------------
	if (SpeakContent.IsEmpty())
	{
		UE_LOG(MultimodalServiceCall, Error, TEXT("GInvalid Speak Content Input"));
		return nullptr;
	}

	if (!bGenerateAudioOnly)
	{
		if (!CharacterSetup.Mesh)
		{
			UE_LOG(MultimodalServiceCall, Error, TEXT("Invalid Mesh"));
			return nullptr;
		}

		if (CharacterSetup.UseArkitSkeletonAnim)
		{
			if (!CharacterSetup.ArkitAnim || CharacterSetup.ArkitAnim->GetNumberOfSampledKeys() != 52 ||
				CharacterSetup.ArkitAnim->GetSkeleton() != CharacterSetup.Mesh->GetSkeleton())
			{
				UE_LOG(MultimodalServiceCall, Error, TEXT("Invalid Skeletal Arkit Animation"));
				return nullptr;
			}
		}
	}

	// ----------------------------------------- Make Talking Face Json -----------------------------------------

	UMultiModalService* Service = NewObject<UMultiModalService>(StaticClass());
	Service->AddToRoot();

	Service->TextToSoundAddress = NewTTSAddress;
	Service->bGenerateAnimation = !bGenerateAudioOnly;

	Service->ConnectToServer(EModalType::TextToSound);

	FEditorTalkingFaceJson TalkingFaceJson;
	TalkingFaceJson.spkr = CharacterSetup.Speaker;
	TalkingFaceJson.face = true;
	TalkingFaceJson.face_scale = CharacterSetup.FaceScale + 0.00001;
	TalkingFaceJson.face_var_scale = CharacterSetup.FaceVarScale + 0.00001;
	TalkingFaceJson.text = SpeakContent;

	FString RequestMessage;
	FJsonObjectConverter::UStructToJsonObjectString<FEditorTalkingFaceJson>(TalkingFaceJson, RequestMessage);

	Service->CurrentChunk = CurrentSentence;
	Service->ChunkAmount = TotalSentenceNumber;
	Service->CharacterName = CharacterName;
	Service->CharacterSetup = CharacterSetup;
	Service->GlobalSettings = GlobalSettings;
	Service->Chunks.Push(RequestMessage);

	Service->ConnectToService();

	return Service;
}

UMultiModalService* UMultiModalService::CSVService(const FString& CharacterName,
                                                   const TArray<TArray<float>>& ExpressionFrames,
                                                   const FCharacterSetupData& CharacterSetup,
                                                   const FTTSGlobalSettings& GlobalSettings)
{
	UMultiModalService* Service = NewObject<UMultiModalService>(StaticClass());

	Service->AddToRoot();
	Service->ConnectToServer(EModalType::CsvService);

	Service->ExpressionFrames = ExpressionFrames;
	Service->CharacterName = CharacterName;
	Service->CharacterSetup = CharacterSetup;
	Service->GlobalSettings = GlobalSettings;
	Service->OriginAnimationFPS = 30.f;

	// Service->Request.multimodal_config.anim_config.face_channel.face_scale = CharacterSetup.FaceScale + 0.00001;
	// Service->Request.multimodal_config.anim_config.face_channel.face_var_scale = CharacterSetup.FaceVarScale + 0.00001;
	// Service->Request.multimodal_config.anim_config.audio_enc.sr = 16000;

	UE_LOG(MultimodalServiceCall, Warning, TEXT("CSVService Service Object Created"));

	// ------------------------------------ Make First Call To Service -----------------------------------------

	return Service;
}

UMultiModalService* UMultiModalService::MultiModalSoundToFaceWithCSVService(const FString& NewS2FAddress,
                                                                            const TObjectPtr<USoundWave>&
                                                                            InputSoundWave,
                                                                            const FString& CharacterName,
                                                                            const FCharacterSetupData& CharacterSetup,
                                                                            const FTTSGlobalSettings& GlobalSettings,
                                                                            const TArray<TArray<float>>&
                                                                            ExtraExpressionFrames)
{
	// ----------------------------------------- Validate Inputs -----------------------------------------
	if (!InputSoundWave)
	{
		UE_LOG(MultimodalServiceCall, Error, TEXT("Invalid Input Sound Wave Asset"));
		return nullptr;
	}

	if (!CharacterSetup.Mesh)
	{
		UE_LOG(MultimodalServiceCall, Error, TEXT("Invalid Mesh Asset"));
		return nullptr;
	}

	if (CharacterSetup.UseArkitSkeletonAnim)
	{
		if (!CharacterSetup.ArkitAnim || CharacterSetup.ArkitAnim->GetNumberOfSampledKeys() != 52 ||
			CharacterSetup.ArkitAnim->GetSkeleton() != CharacterSetup.Mesh->GetSkeleton())
		{
			UE_LOG(MultimodalServiceCall, Error, TEXT("Invalid Skeletal Arkit Animation"));
			return nullptr;
		}
	}

	// ------------------------------------ Audio To Base64 To Chunks -----------------------------------------

	TArray<uint8> AudioRawData;
	uint32 AudioImportSampleRate;
	uint16 NumChannels;
	if (const bool Res = InputSoundWave->GetImportedSoundWaveData(AudioRawData, AudioImportSampleRate, NumChannels); !
		Res)
	{
		return nullptr;
	}

	if (AudioImportSampleRate != 16000)
	{
		// First Resample audio to 16000 and 1 channel
		const auto ImportedSoundWave = URuntimeAudioImporterLibrary::ConvertRegularToImportedSoundWave(InputSoundWave);

		FRuntimeAudioExportOverrideOptions ResampleOptions;
		ResampleOptions.SampleRate = 16000;
		ResampleOptions.NumOfChannels = 1;

		const TArray<uint8> ExportedSoundWave = URuntimeAudioExporter::ExportSoundWaveToRAWBuffer(ImportedSoundWave,
			ERuntimeRAWAudioFormat::Int16,
			ResampleOptions);

		// AudioRawData = TArray(&ExportedSoundWave.GetData()[44], ExportedSoundWave.Num() - 44);
		AudioRawData = ExportedSoundWave;
	}

	// Convert Audio Raw Data to Base64 String
	const FString AudioBase64 = ConvertPCMBufferToBase64String(AudioRawData);

	// 将Base64字符串分割成多个小块
	// TArray<FString> Chunks;
	// for (int32 i = 0; i < AudioBase64.Len(); i += GAudioBase64ChunkSize)
	// {
	// 	const int32 EndIndex = FMath::Min(AudioBase64.Len(), i + GAudioBase64ChunkSize);
	// 	auto Item = AudioBase64.Mid(i, EndIndex - i);
	// 	if (Item.Len() < GAudioBase64ChunkSize)
	// 	{
	// 		for (int RestLen = Item.Len(); RestLen < GAudioBase64ChunkSize; RestLen++)
	// 		{
	// 			Item.Append("=");
	// 		}
	// 	}
	// 	Chunks.Add(Item);
	// }

	// ------------------------------------ Create Service Object And Set Params -----------------------------------------

	UMultiModalService* Service = NewObject<UMultiModalService>(StaticClass());

	Service->AddToRoot();

	Service->SoundToFaceAddress = NewS2FAddress;

	Service->ConnectToServer(EModalType::SoundToFaceWithCSV);

	// Service->Chunks = Chunks;
	Service->ChunkAmount = 1;
	Service->AudioName = InputSoundWave->GetName();

	Service->CharacterName = CharacterName;
	Service->CharacterSetup = CharacterSetup;
	Service->GlobalSettings = GlobalSettings;
	Service->ExtraExpressionFrames = ExtraExpressionFrames;
	Service->OriginAnimationFPS = 25.f;

	Service->AudioBase64 = AudioBase64;
	Service->Request.face_scale = CharacterSetup.FaceScale + 0.00001;
	Service->Request.face_var_scale = CharacterSetup.FaceVarScale + 0.00001;
	Service->Request.chunk_size = AudioBase64.Len();
	Service->Request.num_left_chunks = 1;

	UE_LOG(MultimodalServiceCall, Warning, TEXT("SoundToFace Service Object Created"));
	UE_LOG(MultimodalServiceCall, Warning, TEXT("Current Audio Total Sample: %d"), AudioRawData.Num());
	UE_LOG(MultimodalServiceCall, Warning, TEXT("Current Audio Total Base64: %d"), AudioBase64.Len());
	// UE_LOG(MultimodalServiceCall, Warning, TEXT("Current Audio Chunk Amount: %d"), Chunks.Num());
	// UE_LOG(MultimodalServiceCall, Warning, TEXT("Chunk Size: %d"), Service->Chunks[0].Len());

	// ------------------------------------ Make First Call To Service -----------------------------------------

	Service->ConnectToService();

	return Service;
}

void UMultiModalService::ConnectToServer(const EModalType Type)
{
	FString TargetUrl;

	switch (Type)
	{
	case EModalType::SoundToFace:
		TargetUrl = SoundToFaceAddress;
		break;

	case EModalType::TextToSound:
		TargetUrl = TextToSoundAddress;
		break;

	case EModalType::SoundToFaceWithCSV:
		TargetUrl = SoundToFaceAddress;
		break;

	case EModalType::CsvService:
		break;
	}

	this->ModalType = Type;

	UE_LOG(MultimodalServiceCall, Error, TEXT("Connecting To Server: %s"), *TargetUrl);

	WebSocket = WebSocketModule->CreateWebSocket(TargetUrl, TEXT("ws"));
	WebSocket->OnConnected().AddUObject(this, &ThisClass::OnWebSocketConnectionSuccess);
	WebSocket->OnConnectionError().AddUObject(this, &ThisClass::OnWebSocketConnectionError);
	WebSocket->OnClosed().AddUObject(this, &ThisClass::OnWebSocketDisconnectionSuccess);
	WebSocket->OnMessage().AddUObject(this, &ThisClass::OnWebSocketMessageReceived);
	WebSocket->OnMessageSent().AddUObject(this, &ThisClass::OnWebSocketMessageSent);
}

#pragma region CallService


void UMultiModalService::ConnectToService()
{
	UE_LOG(MultimodalServiceCall, Log, TEXT("Starting To Connect To Service"));

	bIsGenerating = true;
	WebSocket->Connect();
}

void UMultiModalService::StreamSendAudio()
{
	// FRequest NewRequest = Request;
	// FString AudioSlice = Chunks[CurrentChunk];
	//
	// NewRequest.multimodal_config.anim_config.audio = AudioSlice;
	//
	// FString RequestJson;
	// FJsonObjectConverter::UStructToJsonObjectString<FRequest>(NewRequest, RequestJson);
	//
	// CurrentChunk += 1;
	//
	// WebSocket->Send(RequestJson);
}

#pragma endregion

#pragma region Callbacks

void UMultiModalService::OnWebSocketConnectionSuccess()
{
	UE_LOG(MultimodalServiceCall, Log, TEXT("WebSocket Connection Success"));
	FString ConnectJson;

	switch (ModalType)
	{
	case EModalType::TextToSound:
		// If Using Text To Sound, The Json String will be saved at Chunks[0]
		WebSocket->Send(Chunks[0]);
		break;

	case EModalType::SoundToFace:
		StreamSendAudio();
		break;

	case EModalType::SoundToFaceWithCSV:
		FJsonObjectConverter::UStructToJsonObjectString<FSoundToFaceInitJson>(Request, ConnectJson);
		AudioToFaceFirstFrame = true;
		CurrentChunk = 1;
		WebSocket->Send(ConnectJson);
		// StreamSendAudio();
		break;

	case EModalType::CsvService:
		break;
	}
}

void UMultiModalService::OnWebSocketConnectionError(const FString& ErrorMsg) const
{
	UE_LOG(MultimodalServiceCall,
	       Warning,
	       TEXT("Websocket Connected Failed! Error Message: %s"),
	       *ErrorMsg);
	WebSocket->Close();
	OnServiceDisconnect.Broadcast(false);
}

void UMultiModalService::OnWebSocketMessageReceived(const FString& Message)
{
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Message);
	TSharedPtr<FJsonObject> JsonObject;
	FJsonSerializer::Deserialize(JsonReader, JsonObject);

	FString FaceBytes = "";

	switch (ModalType)
	{
	case EModalType::SoundToFace:
		// ----------------------------------------- Sound To Face -----------------------------------------
		{
			UE_LOG(MultimodalServiceCall, Warning, TEXT("Sound To Face Received Message: %s"), *Message);

			const auto RespondContent = JsonObject->GetObjectField(TEXT("respond_content"));
			const auto AnimChannel = RespondContent->GetObjectField(TEXT("anim_channel"));
			const auto FaceChannel = AnimChannel->GetObjectField(TEXT("face_channel"));
			FaceBytes = FaceChannel->GetStringField(TEXT("face_bytes"));
		}

		break;

	case EModalType::TextToSound:
		// ----------------------------------------- TTS -----------------------------------------
		{
			UE_LOG(MultimodalServiceCall, Warning, TEXT("Text To Sound Received"));
			TArray<uint8> AudioDataBase;
			FBase64::Decode(JsonObject->GetStringField(TEXT("audio")), AudioDataBase);
			AudioBuffer.Append(AudioDataBase);
			FaceBytes = JsonObject->GetStringField(TEXT("face_bytes"));
		}

		break;

	case EModalType::SoundToFaceWithCSV:
		// ----------------------------------------- TTS -----------------------------------------
		{
			UE_LOG(MultimodalServiceCall, Warning, TEXT("Sound To Face Received Message: %s"), *Message);

			// const auto RespondContent = JsonObject->GetObjectField(TEXT("respond_content"));
			// const auto AnimChannel = RespondContent->GetObjectField(TEXT("anim_channel"));
			// const auto FaceChannel = AnimChannel->GetObjectField(TEXT("face_channel"));
			FaceBytes = JsonObject->GetStringField(TEXT("chunk_bytes"));
		}

		break;

	case EModalType::CsvService:
		break;
	}

	TArray<float> FaceDataBase = Base64ToFloatArray(FaceBytes);
	for (size_t i = 0; i < FaceDataBase.Num() / 51; i++)
	{
		TArray<float> SingleFrameData{};
		for (size_t j = 0; j < 51; j++)
		{
			float Value = FaceDataBase[i * 51 + j] >= 0.f ? FaceDataBase[i * 51 + j] : 0;
			SingleFrameData.Add(Value);
		}
		ExpressionFrames.Add(SingleFrameData);
	}

	if (JsonObject->GetBoolField(TEXT("is_end")))
	{
		WebSocket->Close();
		UE_LOG(MultimodalServiceCall,
		       Warning,
		       TEXT("OnWebSocketMessageReceived: TTS Data Receive Over Websocket close"));
	}
}

void UMultiModalService::OnWebSocketMessageSent(const FString& Message)
{
	UE_LOG(MultimodalServiceCall, Warning, TEXT("Message Send Success, Message: %s"), *Message);

	if (AudioToFaceFirstFrame)
	{
		AudioToFaceFirstFrame = false;

		FSoundToFaceRequestJson SoundRequestJson{};
		FString SoundRequestJsonStr;

		SoundRequestJson.audio_bytes = AudioBase64;
		SoundRequestJson.is_end = true;

		FJsonObjectConverter::UStructToJsonObjectString(SoundRequestJson, SoundRequestJsonStr);
		WebSocket->Send(SoundRequestJsonStr);
	}
}

void UMultiModalService::OnWebSocketDisconnectionSuccess(int32 StatusCode, const FString& Reason, bool bIsClean)
{
	UE_LOG(MultimodalServiceCall,
	       Warning,
	       TEXT("Websocket Disconnect,Status Code: %d Reason: %s"),
	       StatusCode,
	       *Reason);

	switch (ModalType)
	{
	case EModalType::SoundToFace:
		// ----------------------------------------- Sound To Face -----------------------------------------
		{
			OnA2FOneChunkOver.Broadcast(CurrentChunk - 1, ChunkAmount);
			OnServiceDisconnect.Broadcast(bIsClean);

			if (CurrentChunk < ChunkAmount)
			{
				WebSocket->Connect();
				return;
			}

			StartGeneratingAudioAndAnimFile(false, true, CharacterSetup.UseArkitSkeletonAnim);
		}

		break;

	case EModalType::TextToSound:
		// ----------------------------------------- TTS -----------------------------------------
		{
			OnServiceDisconnect.Broadcast(bIsClean);
			StartGeneratingAudioAndAnimFile(true, bGenerateAnimation, CharacterSetup.UseArkitSkeletonAnim);
		}
		break;

	case EModalType::CsvService:
		break;

	case EModalType::SoundToFaceWithCSV:
		// ----------------------------------------- Sound To Face -----------------------------------------
		UE_LOG(MultimodalServiceCall, Warning, TEXT("Enter Sound To Face With CSV Process"));
		OnA2FOneChunkOver.Broadcast(CurrentChunk - 1, ChunkAmount);
		OnServiceDisconnect.Broadcast(bIsClean);

		if (CurrentChunk < ChunkAmount)
		{
			WebSocket->Connect();
			return;
		}

		// Preprocess Expression Data Ahead, Before Merging Extra Expression Frames
		PreprocessExpressionData();

		StartGeneratingAudioAndAnimFile(false, true, CharacterSetup.UseArkitSkeletonAnim);

		break;
	}
}

#pragma endregion

#pragma region PublicFunctions

void UMultiModalService::StartGeneratingAudioAndAnimFile(const bool bGenerateAudioFile,
                                                         const bool bGenerateAnimFile,
                                                         const bool bGenerateSkelAnim,
                                                         int32 AnimDataFPS)
{
	const FString CurrentTime = FXSequencerModule::GetDateTimeSnakeCase();

	bool bGenerateAudioSuccess{true};
	bool bGenerateAnimSuccess{false};
	const FString AudioBaseName = "SW_" + this->CharacterName + "_" + CurrentTime;
	const FString AnimationBaseName = "AS_TTS_" + this->CharacterName + "_" + CurrentTime;

	float AudioTime{0.0f};

	if (bGenerateAudioFile)
	{
		bGenerateAudioSuccess = GenerateAudioFile(AudioBaseName, AudioTime);
	}

	if (bGenerateAudioSuccess && bGenerateAnimFile)
	{
		bGenerateAnimSuccess = GenerateAnimationFile(AnimationBaseName, bGenerateSkelAnim, AnimDataFPS);
	}

	if (AudioTime < 0.1f)
	{
		AudioTime = static_cast<float>(this->ExpressionFrames.Num()) / OriginAnimationFPS;
	}

	OnMultiModalDataProcessed.Broadcast(this->CharacterName,
	                                    bGenerateAudioSuccess,
	                                    GFacialAnimationFolder + AnimationBaseName,
	                                    bGenerateAnimSuccess,
	                                    GAudioFolder + AudioBaseName,
	                                    AudioTime,
	                                    this->CurrentChunk,
	                                    this->ChunkAmount);
}

bool UMultiModalService::GenerateAudioFile(const FString& AudioBaseName, float& OutAudioTime) const
{
	UE_LOG(MultimodalServiceCall,
	       Warning,
	       TEXT("Start Generating Audio File, AudioBaseName: %s"),
	       *AudioBaseName);

	const FString AudioPackageName = GAudioFolder + AudioBaseName;

	UPackage* Package = CreatePackage(*AudioPackageName);
	check(Package);

	Package->Modify();

	USoundWave* SoundWave = NewObject<USoundWave>(
		Package,
		*AudioBaseName,
		RF_Public | RF_Standalone | RF_MarkAsRootSet);
	FWaveModInfo WaveInfo;
	SoundWave->Modify();

	if (auto WavBuffer = ConvertPCMBufferToWavBuffer(this->AudioBuffer, this->AudioSampleRate);
		WaveInfo.ReadWaveInfo(WavBuffer.GetData(), WavBuffer.Num()))
	{
		SoundWave->InvalidateCompressedData(); //changes the GUID and flushes all the compressed data

		const FSharedBuffer UpdatedBuffer = FSharedBuffer::Clone(WavBuffer.GetData(), WavBuffer.Num());
		SoundWave->RawData.UpdatePayload(UpdatedBuffer);

		// If the duration can be calculated, set it
		if (const int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec)
		{
			SoundWave->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
		}
		else
		{
			SoundWave->Duration = 0.0f; //otherwise, it sets the duration to 0.0f
		}

		SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec); //sets the sample rate of the USoundWave
		SoundWave->NumChannels = *WaveInfo.pChannels; //sets the number of channels of the USoundWave
		SoundWave->RawPCMDataSize = WaveInfo.SampleDataSize; //sets the rawPCMDataSize of the USoundWave
		SoundWave->SoundGroup = SOUNDGROUP_Default; //sets the sound group of the USoundWave
		SoundWave->RawPCMDataSize = WaveInfo.SampleDataSize;
		SoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SoundWave->RawPCMDataSize));
		SoundWave->Volume = this->GlobalSettings.SoundVolume;
		FMemory::Memmove(SoundWave->RawPCMData, WavBuffer.GetData(), WavBuffer.Num());
	}

	SoundWave->AddToRoot();
	SoundWave->Modify();

	FAssetRegistryModule::AssetCreated(SoundWave);

	Package->Modify();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(AudioPackageName,
	                                                                        FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, SoundWave, *PackageFileName, FSavePackageArgs{});

	OutAudioTime = SoundWave->Duration;

	return UPackageTools::ReloadPackages(TArray<UPackage*>{Package});
}

bool UMultiModalService::GenerateAnimationFile(const FString& AnimBaseName, bool bSkeletalAnim, int32 AnimDataFPS)
{
	UE_LOG(MultimodalServiceCall,
	       Warning,
	       TEXT("Start Generating Expression Animation File, Read %d frames of data"),
	       ExpressionFrames.Num());

	const FString AnimationPackageName = GFacialAnimationFolder + AnimBaseName;

	// ----------------------------------------- Preprocess Data -----------------------------------------
	if (this->ModalType != EModalType::SoundToFaceWithCSV)
	{
		PreprocessExpressionData();
	}

	PostProcessExpressionData();

	// --------------------- Create Anim Sequence using AnimSequenceFactory -----------------------------------------

	UPackage* Package = CreatePackage(*AnimationPackageName);
	Package->Modify();

	auto AnimSequenceFactory = NewObject<UAnimSequenceFactory>();
	auto Model = Cast<USkeletalMesh>(this->CharacterSetup.Mesh);
	AnimSequenceFactory->TargetSkeleton = Model->GetSkeleton();
	AnimSequenceFactory->PreviewSkeletalMesh = Model;

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceFactory->FactoryCreateNew(
		UAnimSequence::StaticClass(),
		Package,
		*AnimBaseName,
		RF_Standalone | RF_Public,
		nullptr,
		GWarn));

	AnimSequence->AdditiveAnimType = EAdditiveAnimationType::AAT_LocalSpaceBase;
	AnimSequence->RefPoseType = EAdditiveBasePoseType::ABPT_RefPose;
	AnimSequence->ImportFileFramerate = 30.f;
	AnimSequence->ImportResampleFramerate = 30;
	AnimSequence->GetController().SetFrameRate(FFrameRate{30, 1}, false);
	AnimSequence->GetController().OpenBracket(FText::FromString("Blendshape"));

	int32 OriginFrameNumber = ExpressionFrames.Num();
	auto TargetFrameNumber = static_cast<int32>(static_cast<float>(OriginFrameNumber) / static_cast<float>(AnimDataFPS)
		* 30.f);

	UE_LOG(MultimodalServiceCall, Warning, TEXT("Origin Frame Number: %d, Target Frame Number: %d"),
	       OriginFrameNumber,
	       TargetFrameNumber);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
	AnimSequence->GetController().SetPlayLength(static_cast<float>(TargetFrameLength) / 30.f);
#else
	AnimSequence->GetController().SetNumberOfFrames(FFrameNumber{TargetFrameNumber});
#endif

	// --------------------- Calculate Arkit Bone Transform Map if Use anim -----------------------------------------

	if (bSkeletalAnim)
	{
		// ---------------------------------- Skeletal Anim -----------------------------------------
		// 一个骨骼名-每一帧骨骼 Transform 的 map
		USkeletalMeshComponent* SkMeshComp = NewObject<USkeletalMeshComponent>(
			USkeletalMeshComponent::StaticClass());
		SkMeshComp->SetSkinnedAsset(Model);

		// 一个骨骼名-每一帧骨骼 Transform 的 map
		TMap<FName, TArray<FTransform>> ArkitBoneTransformMap;

		TArray<FName> BoneNames;
		SkMeshComp->GetBoneNames(BoneNames);
		UE_LOG(LogTemp, Warning, TEXT("BoneNames Num: %d"), BoneNames.Num());

		for (int32 SocketIdx = 1; SocketIdx < BoneNames.Num(); SocketIdx += 1)
		{
			TArray<FTransform> Result;
			for (int32 i = 1; i < 52; i++)
			{
#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
				FTransform CurrentFrameTransform = FTransform::Identity;
				int32 BoneTrackIndex = this->CharacterSetup.ArkitAnim->GetDataModel()->GetBoneTrackByName(
												 BoneNames[SocketIdx]).
											 BoneTreeIndex;
				this->CharacterSetup.ArkitAnim->GetBoneTransform(CurrentFrameTransform, BoneTrackIndex,
																 static_cast<float>(i) / 30.0,
																 true);
#else
				auto CurrentFrameTransform = this->CharacterSetup.ArkitAnim->GetDataModel()->GetBoneTrackTransform(
					BoneNames[SocketIdx],
					FFrameNumber{i});
#endif

				Result.Push(CurrentFrameTransform);
			}

			// 如果一个骨骼每帧数据都相同，则不再加入 Map，不再参与后续计算
			auto FirstFrame = Result[0];
			bool ShouldAddToMap = false;
			for (const auto& Transform : Result)
			{
				if (!Transform.Equals(FirstFrame))
				{
					ShouldAddToMap = true;
					break;
				}
			}
			if (ShouldAddToMap)
			{
				ArkitBoneTransformMap.Add(BoneNames[SocketIdx], Result);
			}
		}

		// 通过 Expression Frames 数据生成表情动画
		for (const auto& Name_Data : ArkitBoneTransformMap)
		{
			FName BoneName = Name_Data.Key;

			FRawAnimSequenceTrack RawTrack;

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
			FTransform OriginTransform = FTransform::Identity;
			int32 BoneTrackIndex = this->CharacterSetup.ArkitAnim->GetDataModel()->GetBoneTrackByName(BoneName).
										 BoneTreeIndex;
			this->CharacterSetup.ArkitAnim->GetBoneTransform(OriginTransform, BoneTrackIndex, 0.0f, true);
#else
			FTransform OriginTransform = this->CharacterSetup.ArkitAnim->GetDataModel()->GetBoneTrackTransform(
				BoneName,
				FFrameNumber{0});
#endif

			FTranslationTrack TranslationTrack;

			for (int32 FrameIdx = 0; FrameIdx < TargetFrameNumber; FrameIdx += 1)
			{
				auto BoneTransform = CalcBoneCurrentFrame(Name_Data.Value,
				                                          GetResampledExpressionFrame(
					                                          FrameIdx, TargetFrameNumber, AnimDataFPS),
				                                          OriginTransform);

				RawTrack.ScaleKeys.Add(FVector3f(BoneTransform.GetScale3D()));
				RawTrack.PosKeys.Add(FVector3f(BoneTransform.GetTranslation()));
				RawTrack.RotKeys.Add(FQuat4f(BoneTransform.GetRotation()));
			}

			//add new track
#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
			AnimSequence->GetController().AddBoneTrack(BoneName);
#else
			AnimSequence->GetController().AddBoneCurve(BoneName);
#endif

			AnimSequence->GetController().SetBoneTrackKeys(BoneName,
			                                               RawTrack.PosKeys,
			                                               RawTrack.RotKeys,
			                                               RawTrack.ScaleKeys);
		}
	}
	else
	{
		// ---------------------------------- Morph Target Anim -----------------------------------------

		for (uint16 MorphIdx = 0; MorphIdx < 51; MorphIdx++)
		{
#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 2 || ENGINE_MINOR_VERSION == 1)
			FAnimationCurveIdentifier Curve{
				FSmartName{ArkitBlendshapeNameList[MorphIdx], MorphIdx}, ERawCurveTrackTypes::RCT_Float
			};
#else
			FAnimationCurveIdentifier Curve{ArkitBlendshapeNameList[MorphIdx], ERawCurveTrackTypes::RCT_Float};
#endif
			AnimSequence->GetController().AddCurve(Curve, false);

			TArray<FRichCurveKey> Keys{};
			for (int32 FrameIdx = 0; FrameIdx < TargetFrameNumber; FrameIdx++)
			{
				auto CurrentExpressionFrame = GetResampledExpressionFrame(FrameIdx, TargetFrameNumber, AnimDataFPS);
				FRichCurveKey Key{static_cast<float>(FrameIdx) / 30.f, CurrentExpressionFrame[MorphIdx]};
				Keys.Add(Key);
			}

			AnimSequence->GetController().SetCurveKeys(Curve, Keys, false);
		}
	}

	// ---------------------------------- Finish Creating Asset -----------------------------------------

	AnimSequence->GetController().UpdateAttributesFromSkeleton(Model->GetSkeleton());
	AnimSequence->GetController().NotifyPopulated();

	AnimSequence->GetController().CloseBracket();
	AnimSequence->AddToRoot();
	AnimSequence->Modify();

	FAssetRegistryModule::AssetCreated(AnimSequence);

	Package->Modify();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(AnimationPackageName,
	                                                                  FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, AnimSequence, *PackageFileName, FSavePackageArgs{});

	return UPackageTools::ReloadPackages(TArray<UPackage*>{Package});
}

void UMultiModalService::PreprocessExpressionData()
{
	if (this->CharacterSetup.BlinkSettings.bShouldBlink)
	{
		UE_LOG(MultimodalServiceCall, Warning, TEXT("Start Adding Blink For Character"));
		AddBlinkForCharacter();
	}

	for (auto& Frame : ExpressionFrames)
	{
		// 缩放 Smile Left 和 Smile Right 表情, AnimCurves [43], [44]
		Frame[43] *= this->CharacterSetup.SmileExtent;
		Frame[44] *= this->CharacterSetup.SmileExtent;

		Frame[43] += this->CharacterSetup.SmileBase;
		Frame[44] += this->CharacterSetup.SmileBase;

		// 缩放鼻子表情基，防止鼻子过度变形, AnimCurves [49], [50]
		Frame[49] *= this->CharacterSetup.NoseExtent;
		Frame[50] *= this->CharacterSetup.NoseExtent;

		// 缩放嘴唇与牙齿不联动的表情基，防止嘴唇过度变形, AnimCurves [33], [34], [47], [48]
		Frame[33] *= this->CharacterSetup.DownLipsExtent;
		Frame[34] *= this->CharacterSetup.DownLipsExtent;

		Frame[47] *= this->CharacterSetup.UpLipsExtent;
		Frame[48] *= this->CharacterSetup.UpLipsExtent;
	}
}

void UMultiModalService::PostProcessExpressionData()
{
	// 如果开启表情平滑，则对 Expression Frames 数据先进行处理
	if (GlobalSettings.bEaseOutExpressionAnim)
	{
		if (GlobalSettings.EaseOutFrameNumber > ExpressionFrames.Num())
		{
			GlobalSettings.EaseOutFrameNumber = ExpressionFrames.Num() - 1;
		}

		auto CachedArrayBack = ExpressionFrames[ExpressionFrames.Num() - GlobalSettings.EaseOutFrameNumber];
		for (int EaseFrame = ExpressionFrames.Num() - GlobalSettings.EaseOutFrameNumber;
		     EaseFrame < ExpressionFrames.Num();
		     EaseFrame++)
		{
			for (int EaseNum = 0; EaseNum < 51; EaseNum++)
			{
				ExpressionFrames[EaseFrame][EaseNum] = CachedArrayBack[EaseNum] *
				(static_cast<float>(ExpressionFrames.Num() - EaseFrame) / static_cast<float>(GlobalSettings.
					EaseOutFrameNumber));
			}
		}

		auto CachedArrayFront = ExpressionFrames[GlobalSettings.EaseOutFrameNumber];
		for (int32 EaseFrame = 0; EaseFrame < GlobalSettings.EaseOutFrameNumber; EaseFrame++)
		{
			for (int32 EaseNum = 0; EaseNum < 51; EaseNum++)
			{
				ExpressionFrames[EaseFrame][EaseNum] = CachedArrayFront[EaseNum] *
					(static_cast<float>(EaseFrame) / static_cast<float>(GlobalSettings.EaseOutFrameNumber));
			}
		}
	}
}

void UMultiModalService::AddBlinkForCharacter()
{
	const int32 TotalFrame = this->ExpressionFrames.Num();
	int32 CurrentFrame = 0;
	const int32 BlinkFrame = static_cast<int32>(OriginAnimationFPS * CharacterSetup.BlinkSettings.BlinkTime);

	UE_LOG(LogTemp, Warning, TEXT("Starting AddBlinkForCharacter()"));
	UE_LOG(LogTemp, Warning, TEXT("Blink Frame: %d"), BlinkFrame);

	while (CurrentFrame < TotalFrame)
	{
		const int32 RandomFrame = static_cast<int32>(FMath::RandRange(
			CharacterSetup.BlinkSettings.BlinkRandomBottom,
			CharacterSetup.BlinkSettings.BlinkRandomTop) * OriginAnimationFPS);
		if (CurrentFrame + RandomFrame + BlinkFrame > TotalFrame)
		{
			// RandomTimeCurrent = TotalTimeCurrent - this->BlinkTime - 0.1;  // Ensure safety
			break;
		}

		const float Step = this->CharacterSetup.BlinkSettings.BlinkExtent / static_cast<float>(BlinkFrame) * 2.f;
		float CurrentNumber = 0.f;

		// Start Adding Blink Frames To AnimCurves [8], [9]
		for (int32 i = 0; i < BlinkFrame; i++)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Current Number: %f"), CurrentNumber);

			if (i <= BlinkFrame / 2)
			{
				CurrentNumber += Step;
				if (CurrentNumber > this->CharacterSetup.BlinkSettings.BlinkExtent)
				{
					CurrentNumber = this->CharacterSetup.BlinkSettings.BlinkExtent;
				}

				this->ExpressionFrames[CurrentFrame + RandomFrame + i][8] = CurrentNumber;
				this->ExpressionFrames[CurrentFrame + RandomFrame + i][9] = CurrentNumber;
			}
			else
			{
				CurrentNumber -= Step;
				if (CurrentNumber < 0)
				{
					CurrentNumber = 0;
				}
				this->ExpressionFrames[CurrentFrame + RandomFrame + i][8] = CurrentNumber;
				this->ExpressionFrames[CurrentFrame + RandomFrame + i][9] = CurrentNumber;
			}
		}

		CurrentFrame += RandomFrame;
		CurrentFrame += BlinkFrame;
	}
}

TArray<float> UMultiModalService::GetResampledExpressionFrame(const int32 CurrentFrame, const int32 TargetFrameLength,
                                                              int32 OriginFPS)
{
	if (OriginFPS == 30)
	{
		return this->ExpressionFrames[CurrentFrame];
	}

	const float ScaledCurrentFrame = static_cast<float>(CurrentFrame) / static_cast<float>(TargetFrameLength)
		* static_cast<float>(this->ExpressionFrames.Num());

	if (ScaledCurrentFrame >= this->ExpressionFrames.Num())
	{
		return this->ExpressionFrames.Last();
	}
	const int32 Ceil = FMath::CeilToInt32(ScaledCurrentFrame);
	const int32 Floor = FMath::FloorToInt32(ScaledCurrentFrame);

	if (Ceil >= this->ExpressionFrames.Num() || Floor >= this->ExpressionFrames.Num())
	{
		return this->ExpressionFrames.Last();
	}

	const TArray<float>& CeilExpressionFrames = this->ExpressionFrames[Ceil];
	const TArray<float>& FloorExpressionFrames = this->ExpressionFrames[Floor];

	const float Lerp = ScaledCurrentFrame - Floor;

	TArray<float> Result;
	for (int i = 0; i < CeilExpressionFrames.Num(); i += 1)
	{
		const auto Number = FMath::Lerp(FloorExpressionFrames[i], CeilExpressionFrames[i], Lerp);
		const auto FinalNumber = FMath::Clamp(Number, 0.0, 1.0);
		Result.Push(FinalNumber);
	}

	return Result;
}

#pragma endregion

#pragma region UtilityFunctions

FTransform UMultiModalService::CalcBoneCurrentFrame(const TArray<FTransform>& ArkitBoneTransforms,
                                                    const TArray<float>& CurrentExpressionFrame,
                                                    const FTransform& OriginTransform)
{
	FTransform OutTransform = FTransform::Identity;
	FQuat ResRotation = OriginTransform.GetRotation();
	FVector ResLocation{0.f};

	for (int32 i = 0; i < 51; i++)
	{
		auto OriginRot = OriginTransform.GetRotation();
		auto TargetRot = ArkitBoneTransforms[i].GetRotation();

		// Must Normalize!!!
		OriginRot.Normalize();
		TargetRot.Normalize();

		auto DeltaQuat = OriginRot.Inverse() * TargetRot;
		DeltaQuat.Normalize();
		ResRotation *= FQuat::Slerp(FQuat::Identity,
		                            DeltaQuat,
		                            CurrentExpressionFrame[i]);

		auto DeltaLoc = ArkitBoneTransforms[i].GetLocation() - OriginTransform.GetLocation();
		ResLocation += DeltaLoc * CurrentExpressionFrame[i];
	}
	const auto ResLoc = ResLocation + OriginTransform.GetLocation();

	ResRotation.Normalize();

	OutTransform.SetLocation(ResLoc);
	OutTransform.SetRotation(ResRotation);

	return OutTransform;
}

FString UMultiModalService::GetAndRemoveSubstring(FString& OriginalString, int32 Length)
{
	if (OriginalString.Len() <= Length)
	{
		// 如果请求的长度大于或等于原始字符串的长度，直接清空原始字符串
		auto Result = OriginalString;
		OriginalString.Empty();

		return Result;
	}

	// 获取从开始到指定长度的子字符串
	FString Part = OriginalString.Left(Length);

	// 从原始字符串中移除这部分
	OriginalString = OriginalString.Right(OriginalString.Len() - Length);

	return Part;
}

TArray<float> UMultiModalService::Base64ToFloatArray(const FString& Base64String)
{
	TArray<uint8> Data;
	FBase64::Decode(Base64String, Data);
	TArray<float> Result;
	for (size_t i = 0; i < Data.Num(); i += 4)
	{
		float Fa = 0.f;
		uint8 Temp[4];
		Temp[0] = Data[i];
		Temp[1] = Data[i + 1];
		Temp[2] = Data[i + 2];
		Temp[3] = Data[i + 3];
		memcpy(&Fa, Temp, 4);

		Result.Push(Fa);
	}
	return Result;
}

FString UMultiModalService::ConvertPCMBufferToBase64String(const TArray<uint8>& PcmBuffer)
{
	return FBase64::Encode(PcmBuffer);
}

TArray<uint8> UMultiModalService::ConvertPCMBufferToWavBuffer(const TArray<uint8>& PcmBuffer,
                                                              const uint32 AudioSampleRate)
{
	TArray<uint8> BytesArr;

	TArray<uint8> AudioHeader;
	constexpr uint16 BitDepth = 16; // How to retrieve information directly from the SoundWave ??

	AudioHeader.Append(FStringToBytes("RIFF")); // 1-4
	AudioHeader.Append(Uint32ToBytes(PcmBuffer.Num() + 36)); // 5-8
	AudioHeader.Append(FStringToBytes("WAVEfmt ")); // 9-16
	AudioHeader.Append(Uint32ToBytes(16)); // 17-20
	AudioHeader.Append(Uint16ToBytes(1)); // 21-22
	AudioHeader.Append(Uint16ToBytes(1)); // 23-24 Num of channels
	AudioHeader.Append(Uint32ToBytes(AudioSampleRate)); // 25-28
	AudioHeader.Append(Uint32ToBytes(AudioSampleRate * BitDepth * 1 / 8)); // 29-32
	AudioHeader.Append(Uint16ToBytes(BitDepth * 1 / 8)); // 33-34
	AudioHeader.Append(Uint16ToBytes(BitDepth)); // 35-36
	AudioHeader.Append(FStringToBytes("data")); // 37-40
	AudioHeader.Append(Uint32ToBytes(PcmBuffer.Num())); // 41-44

	BytesArr.Append(AudioHeader);
	BytesArr.Append(PcmBuffer);

	return BytesArr;
}

TArray<uint8> UMultiModalService::FStringToBytes(const FString& String)
{
	TArray<uint8> OutBytes;

	// Handle empty strings
	if (String.Len() > 0)
	{
		FTCHARToUTF8 Converted(*String); // Convert to UTF8
		OutBytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	return OutBytes;
}

TArray<uint8> UMultiModalService::Uint32ToBytes(const uint32 Value, bool UseLittleEndian)
{
	TArray<uint8> OutBytes;
	if (UseLittleEndian)
	{
		OutBytes.Add(Value >> 0 & 0xFF);
		OutBytes.Add(Value >> 8 & 0xFF);
		OutBytes.Add(Value >> 16 & 0xFF);
		OutBytes.Add(Value >> 24 & 0xFF);
	}
	else
	{
		OutBytes.Add(Value >> 24 & 0xFF);
		OutBytes.Add(Value >> 16 & 0xFF);
		OutBytes.Add(Value >> 8 & 0xFF);
		OutBytes.Add(Value >> 0 & 0xFF);
	}
	return OutBytes;
}

TArray<uint8> UMultiModalService::Uint16ToBytes(const uint16 Value, bool UseLittleEndian)
{
	TArray<uint8> OutBytes;
	if (UseLittleEndian)
	{
		OutBytes.Add(Value >> 0 & 0xFF);
		OutBytes.Add(Value >> 8 & 0xFF);
	}
	else
	{
		OutBytes.Add(Value >> 8 & 0xFF);
		OutBytes.Add(Value >> 0 & 0xFF);
	}
	return OutBytes;
}

#pragma endregion

#pragma region ExpressionAnims

FString UMultiModalService::GenerateExpressionAnim(const FString& CharacterName,
                                                   USkeletalMesh* MeshAsset,
                                                   const FString& ExpressionName,
                                                   float ExpressionExtent)
{
	const FString AnimationBaseName = "AS_Expression_" + CharacterName + "_" + ExpressionName + "_" +
		FString::SanitizeFloat(ExpressionExtent).Replace(TEXT("."),TEXT("_"));
	FString AnimationPackageName = GExpressionFolder + AnimationBaseName;

	if (UEditorAssetLibrary::DoesAssetExist(AnimationPackageName))
	{
		UE_LOG(MultimodalServiceCall,
		       Warning,
		       TEXT(
			       "GenerateExpressionAnim - Expression %s for character %s already exist, returning path"
		       ),
		       *ExpressionName,
		       *CharacterName);
		return AnimationPackageName;
	}

	// Load Target Expression Json And Set Length
	FString ResourceFolder = IPluginManager::Get().FindPlugin("XCharacter")->GetBaseDir() / TEXT("Resources");
	FString JsonPath = ResourceFolder + "/" + ExpressionName + ".json";
	FString ExpressionJsonString;

	if (!FFileHelper::LoadFileToString(ExpressionJsonString, *JsonPath))
	{
		UE_LOG(MultimodalServiceCall,
		       Error,
		       TEXT("GenerateExpressionAnim: Unable To Load Json File: %s, File Not Exist"),
		       *JsonPath);
		return "";
	}

	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<CHAR>::Create(ExpressionJsonString);
	TSharedPtr<FJsonObject> ExpressionJsonObject;
	if (!FJsonSerializer::Deserialize(JsonReader, ExpressionJsonObject))
	{
		UE_LOG(MultimodalServiceCall,
		       Error,
		       TEXT("GenerateExpressionAnim: Json File %s Format Error, Please Use Right Json Format"),
		       *JsonPath);
		return "";
	}

	int32 TotalFrameNumber = 0;

	TMap<FString, TSharedPtr<FJsonValue>> Values = ExpressionJsonObject.Get()->Values; // 获取 Data 中的TMap

	TArray<FString> Keys;
	Values.GetKeys(Keys);

	UPackage* Package = CreatePackage(*AnimationPackageName);
	Package->Modify();

	auto AnimSequenceFactory = NewObject<UAnimSequenceFactory>();
	AnimSequenceFactory->TargetSkeleton = MeshAsset->GetSkeleton();
	AnimSequenceFactory->PreviewSkeletalMesh = MeshAsset;

	UAnimSequence* AnimSequence = (UAnimSequence*)AnimSequenceFactory->FactoryCreateNew(
		UAnimSequence::StaticClass(),
		Package,
		*AnimationBaseName,
		RF_Standalone | RF_Public,
		NULL,
		GWarn);

	AnimSequence->GetController().OpenBracket(FText::FromString("Blendshape"));

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
	AnimSequence->GetController().SetPlayLength(100);
#else
	AnimSequence->GetController().SetNumberOfFrames(FFrameNumber{5000});
#endif

	AnimSequence->AdditiveAnimType = EAdditiveAnimationType::AAT_LocalSpaceBase;
	AnimSequence->RefPoseType = EAdditiveBasePoseType::ABPT_RefPose;

	for (uint16 i = 0; i < Keys.Num(); i++)
	{
		auto Key = Keys[i];
		auto Value = Values[Key];

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 2 || ENGINE_MINOR_VERSION == 1)
		FAnimationCurveIdentifier Curve{
			FSmartName{FName{*Key}, static_cast<uint16>(i + 1)}, ERawCurveTrackTypes::RCT_Float
		};
#else
		FAnimationCurveIdentifier Curve{FName(*Key), ERawCurveTrackTypes::RCT_Float};
#endif

		AnimSequence->GetController().AddCurve(Curve, false);

		TArray<FRichCurveKey> RickKeys{};
		for (size_t j = 0; j < Value->AsArray().Num(); j++)
		{
			FRichCurveKey RickKey{
				static_cast<float>(j) / 30.f,
				static_cast<float>(Value->AsArray()[j]->AsNumber() * ExpressionExtent)
			};
			RickKeys.Add(RickKey);
		}

		AnimSequence->GetController().SetCurveKeys(Curve, RickKeys, false);

		if (Value->AsArray().Num() > TotalFrameNumber)
		{
			TotalFrameNumber = Value->AsArray().Num();
		}
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
	AnimSequence->GetController().SetPlayLength(static_cast<float>(TotalFrameNumber) / 30);
#else
	AnimSequence->GetController().SetNumberOfFrames(FFrameNumber{TotalFrameNumber});
#endif

	AnimSequence->GetController().CloseBracket();
	AnimSequence->AddToRoot();
	AnimSequence->Modify();

	FAssetRegistryModule::AssetCreated(AnimSequence);

	Package->Modify();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(AnimationPackageName,
	                                                                  FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, AnimSequence, *PackageFileName, FSavePackageArgs{});
	if (!UPackageTools::ReloadPackages(TArray{Package}))
	{
		UE_LOG(MultimodalServiceCall,
		       Error,
		       TEXT("GenerateExpressionAnim - Failed to reload package %s"),
		       *PackageFileName);
		return "";
	}

	UE_LOG(MultimodalServiceCall,
	       Log,
	       TEXT(
		       "GenerateExpressionAnim - Generate Expression %s for character %s success, returning path"
	       ),
	       *ExpressionName,
	       *CharacterName);

	return AnimationPackageName;
}

void UMultiModalService::StartCSVReading()
{
	if (this->ModalType == EModalType::CsvService)
	{
		StartGeneratingAudioAndAnimFile(false, true, CharacterSetup.UseArkitSkeletonAnim, 30);
	}
}

#pragma endregion
