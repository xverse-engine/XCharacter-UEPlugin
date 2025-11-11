// Fill out your copyright notice in the Description page of Project Settings.
#include "DataSynthesizerLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Animation/AnimInstance.h"
#include "JsonObjectConverter.h"
#include "Misc/Base64.h"


/**Avatar Driven Server��Interface */
void UDataSynthesizerLibrary::Decode(const FString& EncodeDrivenData, FResponse& OutResponseConfig) {
	TSharedPtr<FJsonObject> MessageObj = MakeShared<FJsonObject>();
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(EncodeDrivenData);
	FJsonSerializer::Deserialize(JsonReader, MessageObj);
	
	FJsonObjectConverter::JsonObjectStringToUStruct(EncodeDrivenData, &OutResponseConfig, 0, 0);
}

/**������Ƶ*/
void UDataSynthesizerLibrary::DecodePCM(const FString& InEncodedAudioData, TArray<uint8>& OutDecodedAudioData)
{
	//TArray<uint8> DecodedBytes;
	FBase64::Decode(InEncodedAudioData, OutDecodedAudioData);
	/*OutDecodedAudioData.Empty();
	OutDecodedAudioData.AddUninitialized(DecodedBytes.Num());
	FMemory::Memcpy(OutDecodedAudioData.GetData(), DecodedBytes.GetData(), DecodedBytes.Num() * sizeof(uint8));*/
}

/**�������ƣ�������Ҫ������ָ���Ĺ�����ʽ*/
void UDataSynthesizerLibrary::DecodeGesture(const FString& InEncodedAudioData, TArray<float>& OutDecodedAudioData)
{
	TArray<uint8> DecodedBytes;
	FBase64::Decode(InEncodedAudioData, DecodedBytes);
	OutDecodedAudioData.Empty();
	OutDecodedAudioData.AddUninitialized(DecodedBytes.Num() * sizeof(uint8) / sizeof(float));
	FMemory::Memcpy(OutDecodedAudioData.GetData(), DecodedBytes.GetData(), DecodedBytes.Num() * sizeof(uint8));
}

/**ѹ����Ƶ�ļ�*/
void UDataSynthesizerLibrary::EncodeAudioBase64(const float InAudioCaptureData, FString OutBase64String) {
	
}

/**�����沿��BS����ϵ��*/
void UDataSynthesizerLibrary::DecodeFaceCurve(TArray<float>& OutDecodedFaceCurve,
	const FString& InEncodedFaceCurve,
	int NumFrames,
	int NumFields)
{
	TArray<uint8> Data;
	FBase64::Decode(InEncodedFaceCurve, Data);

	for (size_t i = 0; i < Data.Num(); i += 4)
	{
		float Fa = 0.f;
		uint8 Temp[4];
		Temp[0] = Data[i];
		Temp[1] = Data[i + 1];
		Temp[2] = Data[i + 2];
		Temp[3] = Data[i + 3];
		memcpy(&Fa, Temp, 4);
		OutDecodedFaceCurve.Push(Fa);
	}
}


//int UDataSynthesizerLibrary::GetNowTimeTickMs()
//{
//	return (FDateTime::Now() - InitDateTime_).GetTotalMilliseconds();
//}
