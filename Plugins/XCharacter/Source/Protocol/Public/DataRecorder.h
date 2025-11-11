// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DataRecorder.generated.h"

/**
 * 
 */
UCLASS()
class PROTOCOL_API UDataRecorder : public UObject
{
	GENERATED_BODY()

public:
		void static FSaveAudio(int32 InSampleRate, int32 InNumChannels, TArray<uint8> AudioBuffer);
		
		void static FSaveBSFaceAnim(const FString& CurrentTime,
									const FString& AudioPackageName, 
									float AnimationLength, 
									USkeletalMesh* CurrentSelectedSpeakMesh, 
									bool bEaseOutExpressionFrames, 
									TArray<TArray<float>> ExpressionFrames, 
									int32 EaseOutNumber, 
									TArray<FName> BS_List);

		void static FSaveGestureAnim();

private:
		TArray<uint8> static ConvertAudioBufferRawDataToWav(int32 SampleRate, TArray<uint8> AudioBuffer);
		TArray<uint8> static uint32ToBytes(const uint32 Value, bool UseLittleEndian = true);
		TArray<uint8> static uint16ToBytes(const uint16 Value, bool UseLittleEndian = true);
		TArray<uint8> static FStringToBytes(const FString& String);
};
