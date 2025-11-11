// Fill out your copyright notice in the Description page of Project Settings.


#include "DataRecorder.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "XVCPluginSettings.h"
//#include "EditorAssetLibrary.h"

#include "Factories/AnimSequenceFactory.h"
#include "Serialization/JsonSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Sound/SoundWave.h"


DEFINE_LOG_CATEGORY_STATIC(LogUDataRecoder, Verbose, All);


void UDataRecorder::FSaveAudio(int32 InSampleRate, int32 InNumChannels,TArray<uint8> AudioBuffer)
{
	const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();

	FString AudioBaseName = FString("Audio_") + FGuid::NewGuid().ToString();
	FString AudioPackageName = Settings->MultiModalChatWorkSpace + AudioBaseName;

	UPackage* Package = CreatePackage(*AudioPackageName);
	check(Package);

	Package->Modify();

	USoundWave* sw = NewObject<USoundWave>(
		Package, *AudioBaseName, RF_Public | RF_Standalone | RF_MarkAsRootSet);
	FWaveModInfo WaveInfo;
	sw->Modify();

	auto rawFile = ConvertAudioBufferRawDataToWav(InSampleRate, AudioBuffer);

	if (WaveInfo.ReadWaveInfo(rawFile.GetData(), rawFile.Num()))
	{
		sw->InvalidateCompressedData(); //changes the GUID and flushes all the compressed data

		FSharedBuffer UpdatedBuffer = FSharedBuffer::Clone(rawFile.GetData(), rawFile.Num());
		sw->RawData.UpdatePayload(UpdatedBuffer);

		int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;
		if (DurationDiv) //IF dureation div is not null
		{
			sw->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
		}
		else
		{
			sw->Duration = 0.0f; //otherwise, it sets the duration to 0.0f
		}

		sw->SetSampleRate(*WaveInfo.pSamplesPerSec); //sets the sample rate of the USoundWave
		sw->NumChannels = *WaveInfo.pChannels; //sets the number of channels of the USoundWave
		sw->RawPCMDataSize = WaveInfo.SampleDataSize; //sets the rawPCMDataSize of the USoundWave
		sw->SoundGroup = ESoundGroup::SOUNDGROUP_Default; //sets the sound group of the USoundWave
		sw->RawPCMDataSize = WaveInfo.SampleDataSize;
		sw->RawPCMData = (uint8*)FMemory::Malloc(sw->RawPCMDataSize);
		sw->Volume = 1.0;
		FMemory::Memmove(sw->RawPCMData, rawFile.GetData(), rawFile.Num());
	}

	const float OutDuration = sw->Duration;

	sw->AddToRoot();
	sw->Modify();

	FAssetRegistryModule::AssetCreated(sw);

	Package->Modify();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(AudioPackageName,
		FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, sw, *PackageFileName, FSavePackageArgs{});
	UPackageTools::ReloadPackages(TArray<UPackage*>{Package});

}

void UDataRecorder::FSaveBSFaceAnim(const FString& CurrentTime,
	const FString& AudioPackageName,
	float AnimationLength,
	USkeletalMesh* CurrentSelectedSpeakMesh,
	bool bEaseOutExpressionFrames,
	TArray<TArray<float>> ExpressionFrames,
	int32 EaseOutNumber,
	TArray<FName> BS_List
)
{
	/*UE_LOG(LogUDataRecoder, Warning, TEXT("StartGeneratingAnimAndAudioFile: Starting to generate animation file"));

	FString AnimationBaseName = "FacialAnim_" + CurrentTime;
	FString AnimationPackageName = GFacialAndAudioFolder + AnimationBaseName;

	UPackage* Package = CreatePackage(*AnimationPackageName);
	Package->Modify();

	UE_LOG(LogUDataRecoder, Warning, TEXT("Read %d frames of data"), ExpressionFrames.Num());

	auto AnimSequenceFactory = NewObject<UAnimSequenceFactory>();

	AnimSequenceFactory->TargetSkeleton = CurrentSelectedSpeakMesh->GetSkeleton();
	AnimSequenceFactory->PreviewSkeletalMesh = CurrentSelectedSpeakMesh;

	UAnimSequence* AnimSequence = (UAnimSequence*)AnimSequenceFactory->FactoryCreateNew(
		UAnimSequence::StaticClass(), Package, *AnimationBaseName, RF_Standalone | RF_Public, NULL, GWarn);

	AnimSequence->GetController().
		SetNumberOfFrames(AnimSequence->GetController().ConvertSecondsToFrameNumber(AnimationLength));

	AnimSequence->Modify();
	AnimSequence->AdditiveAnimType = EAdditiveAnimationType::AAT_LocalSpaceBase;
	AnimSequence->RefPoseType = EAdditiveBasePoseType::ABPT_RefPose;

	if (bEaseOutExpressionFrames)
	{
		auto CachedArray = ExpressionFrames[ExpressionFrames.Num() - EaseOutNumber];
		for (int EaseFrame = ExpressionFrames.Num() - EaseOutNumber; EaseFrame < ExpressionFrames.Num(); EaseFrame++)
		{
			for (int EaseNum = 0; EaseNum < 51; EaseNum++)
			{
				ExpressionFrames[EaseFrame][EaseNum] = CachedArray[EaseNum] *
					(static_cast<float>(ExpressionFrames.Num() - EaseFrame) / static_cast<float>(EaseOutNumber));
			}
		}
	}

	for (size_t another_i = 0; another_i < 51; another_i++)
	{
		FAnimationCurveIdentifier Curve{ BS_List[another_i], ERawCurveTrackTypes::RCT_Float };
		AnimSequence->GetController().AddCurve(Curve, false);

		TArray<FRichCurveKey> Keys{};
		for (size_t j = 0; j < ExpressionFrames.Num(); j++)
		{
			FRichCurveKey Key{ static_cast<float>(j) / 25.f, ExpressionFrames[j][another_i] };
			Keys.Add(Key);
		}

		AnimSequence->GetController().SetCurveKeys(Curve, Keys, false);
	}

	AnimSequence->AddToRoot();
	AnimSequence->Modify();

	FAssetRegistryModule::AssetCreated(AnimSequence);

	Package->Modify();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(AnimationPackageName,
		FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(Package, AnimSequence, *PackageFileName, FSavePackageArgs{});
	bool Res = UPackageTools::ReloadPackages(TArray<UPackage*>{Package});

	ExpressionFrames.Empty();

	if (Res)
	{
		UE_LOG(LogUDataRecoder, Warning, TEXT("GenerateAudioFile: Finishing generating animation and audio file"));
		AnimSequence->RemoveFromRoot();
		AnimSequence->MarkAsGarbage();
	}*/
}

void UDataRecorder::FSaveGestureAnim()
{
}

TArray<uint8> UDataRecorder::ConvertAudioBufferRawDataToWav(int32 SampleRate ,TArray<uint8> AudioBuffer)
{
	TArray<uint8> BytesArr;

	TArray<uint8> AudioHeader;
	uint16 BitDepth = 16; // How to retrieve information directly from the SoundWave ??

	AudioHeader.Append(ThisClass::FStringToBytes("RIFF")); // 1-4
	AudioHeader.Append(ThisClass::uint32ToBytes(AudioBuffer.Num() + 36)); // 5-8
	AudioHeader.Append(ThisClass::FStringToBytes("WAVEfmt ")); // 9-16
	AudioHeader.Append(ThisClass::uint32ToBytes(16)); // 17-20
	AudioHeader.Append(ThisClass::uint16ToBytes(1)); // 21-22
	AudioHeader.Append(ThisClass::uint16ToBytes(1)); // 23-24
	AudioHeader.Append(ThisClass::uint32ToBytes(SampleRate)); // 25-28
	AudioHeader.Append(ThisClass::uint32ToBytes(SampleRate * BitDepth * 1 / 8)); // 29-32
	AudioHeader.Append(ThisClass::uint16ToBytes(BitDepth * 1 / 8)); // 33-34
	AudioHeader.Append(ThisClass::uint16ToBytes(BitDepth)); // 35-36
	AudioHeader.Append(ThisClass::FStringToBytes("data")); // 37-40
	AudioHeader.Append(ThisClass::uint32ToBytes(AudioBuffer.Num())); // 41-44

	BytesArr.Append(AudioHeader);
	BytesArr.Append(AudioBuffer);

	AudioBuffer.Empty();

	return BytesArr;
}

TArray<uint8> UDataRecorder::FStringToBytes(const FString& String)
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

TArray<uint8> UDataRecorder::uint32ToBytes(const uint32 Value, bool UseLittleEndian)
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

TArray<uint8> UDataRecorder::uint16ToBytes(const uint16 Value, bool UseLittleEndian)
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