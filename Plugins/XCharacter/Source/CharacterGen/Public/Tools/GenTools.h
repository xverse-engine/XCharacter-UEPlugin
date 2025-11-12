// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "XVCPluginSettings.h"

#include "GenTools.generated.h"

UCLASS()
class CHARACTERGEN_API UGenTools : public UObject
{
	GENERATED_BODY()

public:
	static UGenTools* Get();

	// 保存音频数据为SoundWave资产
	static bool SaveAudioAsSoundWaveAsset(const TArray<uint8>& AudioData, const FString& AssetPackagePath = TEXT("/Game/YDAutomation/Audio/"), const FString& AssetName = TEXT(""), bool bSync = true, bool bForceStereo = false);
	
	// 将PCM数据导出为WAV格式
	static bool ExportPCMToWavData(const TArray<uint8>& AudioData, uint32 SampleRate, uint16 NumChannels, TArray<uint8>& OutWavData, bool bForceStereo = false);
	
	
	// 资产同步接口（合成资产后调用）- 单个资产路径
	static void SyncToAsset(const FString& AssetPath);
	
	// 资产同步接口（合成资产后调用）- 多个资产路径
	static void SyncToAsset(const TArray<FString>& AssetPaths);
	
	// 处理TTS存储路径的子目录创建
	static bool SetupTTSStoragePaths(const FString& BasePath, FString& OutAssetPath, FString& OutWavDir);

	// 恢复插件窗口焦点
	static void BringPluginWindowToFront();

	// 从音频数据创建SoundWave（用于预览播放）
	static USoundWave* CreateSoundWaveFromAudioData(const TArray<uint8>& AudioData, bool bForceStereo = false);

	// 从WAV文件创建SoundWave（用于预览播放）
	static USoundWave* CreateSoundWaveFromWavFile(const FString& WavFilePath);
	
	// 检查音频资产是否已存在
	static bool CheckAudioAssetExists(const FString& AssetName, const FString& AssetPackagePath = TEXT(""));
};
