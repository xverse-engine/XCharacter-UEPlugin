// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProtocolStruct.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DataSynthesizerLibrary.generated.h"

/**
 * 
 */
UCLASS()
class PROTOCOL_API UDataSynthesizerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	//UFUNCTION(BlueprintCallable, Category = "DataSynthesizer | BPFL")
	static void Decode(const FString& InEncodedAudioData, FResponse& OutResponseConfig);

	//UFUNCTION(BlueprintCallable, Category = "DataSynthesizer | BPFL")
	static void DecodeGesture(const FString& InEncodedAudioData,
		TArray<float>& OutDecodedAudioData);

	//UFUNCTION(BlueprintCallable, Category = "DataSynthesizer | BPFL")
	static void DecodePCM(const FString& InEncodedAudioData,
		TArray<uint8>& OutDecodedAudioData);

	//UFUNCTION(BlueprintCallable, Category = "DataSynthesizer | BPFL")
	static void DecodeFaceCurve(TArray<float>& OutDecodedFaceCurve,
		const FString& InEncodedFaceCurve,
		int NumFrames,
		int NumFields);

	//UFUNCTION(BlueprintCallable, Category = "DataSynthesizer | BPFL")
	static void EncodeAudioBase64(const float InAudioCaptureData, 
		FString OutBase64String);


};
