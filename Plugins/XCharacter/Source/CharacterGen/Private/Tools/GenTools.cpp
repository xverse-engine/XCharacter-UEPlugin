// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/GenTools.h"
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
#include "Interfaces/IPluginManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Editor.h"
#include "HAL/PlatformProcess.h"

UGenTools* UGenTools::Get()
{
    return GetMutableDefault<UGenTools>();
}

bool UGenTools::SaveAudioAsSoundWaveAsset(const TArray<uint8>& AudioData, const FString& AssetPackagePath, const FString& AssetName, bool bSync, bool bForceStereo)
{
	FString FinalAssetPackagePath = AssetPackagePath;
	FString FinalAssetName = AssetName;
	
	// 如果AssetPackagePath为空，使用配置的TTS存储路径
	if (FinalAssetPackagePath.IsEmpty())
	{
		// 使用配置的TTS存储路径，参考AudioAssetOperation.cpp中的逻辑
		const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
		FString AssetPathDir, WavDir;
		if (UGenTools::SetupTTSStoragePaths(Settings->TTSStoragePath, AssetPathDir, WavDir))
		{
			FinalAssetPackagePath = AssetPathDir;
		}
		else
		{
			// 如果配置路径设置失败，使用备用路径
			FinalAssetPackagePath = TEXT("/Game/YDAutomation/Audio/");
			UE_LOG(LogTemp, Warning, TEXT("SaveAudioAsSoundWaveAsset: 使用配置路径失败，使用备用路径: %s"), *FinalAssetPackagePath);
		}
	}
	
	// 如果AssetName为空，使用默认命名
	if (FinalAssetName.IsEmpty())
	{
		FinalAssetName = TEXT("TTS_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	}
	
	FString FullPackageName = FinalAssetPackagePath + FinalAssetName;

	// 2. 如果需要双声道转换，先处理音频数据
	TArray<uint8> FinalAudioData = AudioData;
	if (bForceStereo)
	{
		// 先解析原始音频数据获取声道信息
		FWaveModInfo TempWaveInfo;
		if (TempWaveInfo.ReadWaveInfo(AudioData.GetData(), AudioData.Num()))
		{
			uint16 NumChannels = *TempWaveInfo.pChannels;
			uint32 SampleRate = *TempWaveInfo.pSamplesPerSec;
			
			// 如果当前是单声道，转换为双声道
			if (NumChannels == 1)
			{
				// 提取PCM数据
				TArray<uint8> PCMData;
				PCMData.SetNum(TempWaveInfo.SampleDataSize);
				FMemory::Memcpy(PCMData.GetData(), TempWaveInfo.SampleDataStart, TempWaveInfo.SampleDataSize);
				
				// 转换为双声道WAV
				if (UGenTools::ExportPCMToWavData(PCMData, SampleRate, NumChannels, FinalAudioData, true))
				{
					UE_LOG(LogTemp, Log, TEXT("成功将单声道音频转换为双声道"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("双声道转换失败，使用原始音频"));
					FinalAudioData = AudioData;
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("音频已经是多声道，无需转换"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("无法解析音频数据，使用原始音频"));
			FinalAudioData = AudioData;
		}
	}

	// 3. 创建Package
	UPackage* Package = CreatePackage(*FullPackageName);
	Package->Modify();

	// 4. 创建SoundWave对象
	USoundWave* SoundWave = NewObject<USoundWave>(Package, *FinalAssetName, RF_Public | RF_Standalone | RF_MarkAsRootSet);
	SoundWave->Modify();

	// 5. 解析WAV数据并填充SoundWave
	FWaveModInfo WaveInfo;
	if (WaveInfo.ReadWaveInfo(FinalAudioData.GetData(), FinalAudioData.Num()))
	{
		SoundWave->InvalidateCompressedData();
		const FSharedBuffer UpdatedBuffer = FSharedBuffer::Clone(FinalAudioData.GetData(), FinalAudioData.Num());
		SoundWave->RawData.UpdatePayload(UpdatedBuffer);

		if (const int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec)
		{
			SoundWave->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
		}
		else
		{
			SoundWave->Duration = 0.0f;
		}

		SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
		SoundWave->NumChannels = *WaveInfo.pChannels;
		SoundWave->RawPCMDataSize = WaveInfo.SampleDataSize;
		SoundWave->SoundGroup = SOUNDGROUP_Default;
		SoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SoundWave->RawPCMDataSize));
		FMemory::Memmove(SoundWave->RawPCMData, WaveInfo.SampleDataStart, SoundWave->RawPCMDataSize);
		SoundWave->Volume = 1.0f;
	}
	else
	{
		return false;
	}

	// 5. 注册并保存
	SoundWave->AddToRoot();
	FAssetRegistryModule::AssetCreated(SoundWave);
	Package->Modify();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPackageName, FPackageName::GetAssetPackageExtension());
	bool bSaved = UPackage::SavePackage(Package, SoundWave, *PackageFileName, FSavePackageArgs{});

	// 6. 根据sync参数决定是否同步资产到内容浏览器
	if (bSaved && bSync)
	{
		SyncToAsset(FullPackageName);
	}
	
	return bSaved;
}


// 资产同步接口（合成资产后调用）- 单个资产路径
void UGenTools::SyncToAsset(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
        return;
    TArray<FString> Paths;
    Paths.Add(AssetPath);
    SyncToAsset(Paths);
}

// 资产同步接口（合成资产后调用）- 多个资产路径
void UGenTools::SyncToAsset(const TArray<FString>& AssetPaths)
{
    if (AssetPaths.Num() == 0)
        return;

    // 使用延迟执行来避免窗口失焦
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        // 使用定时器延迟执行同步操作，避免在HTTP回调中直接执行导致窗口失焦
        const float DelaySeconds = 0.5f;  // 增加延迟时间，确保资产完全注册
        FTimerDelegate SyncDelegate;
        SyncDelegate.BindLambda([AssetPaths]()
        {
            // 导入必要的模块
            FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
            FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
            
            TArray<FAssetData> AssetsToSync;
            
            // 遍历所有资产路径，获取资产数据
            for (const FString& AssetPath : AssetPaths)
            {
                // 将UE路径格式转换为资产对象路径
                FString AssetObjectPath = AssetPath + TEXT(".") + FPaths::GetBaseFilename(AssetPath);
                FSoftObjectPath SoftPath(AssetObjectPath);
                
                // 尝试多次获取资产数据，确保资产已完全注册
                FAssetData AssetData;
                int32 RetryCount = 0;
                const int32 MaxRetries = 5;
                
                while (RetryCount < MaxRetries && !AssetData.IsValid())
                {
                    AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(SoftPath);
                    if (!AssetData.IsValid())
                    {
                        // 等待一小段时间后重试
                        FPlatformProcess::Sleep(0.1f);
                        RetryCount++;
                    }
                }
                
                if (AssetData.IsValid())
                {
                    AssetsToSync.Add(AssetData);
                }
                else
                {
                    // 如果仍然无法获取，尝试通过路径直接同步
                    UE_LOG(LogTemp, Warning, TEXT("无法获取资产数据: %s，尝试直接同步路径"), *AssetPath);
                }
            }
            
            // 同步到内容浏览器
            if (AssetsToSync.Num() > 0)
            {
                ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);
                
                // 显示同步完成通知
                FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("已同步 %d 个音频资产到内容浏览器"), AssetsToSync.Num())));
                Info.ExpireDuration = 5.0f;
                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
            }
            
                 // 如果某些资产无法同步，尝试通过路径直接同步
                 if (AssetsToSync.Num() < AssetPaths.Num())
                 {
                     // 尝试通过路径直接同步剩余的资产
                     for (const FString& AssetPath : AssetPaths)
                     {
                         bool bFound = false;
                         for (const FAssetData& SyncedAsset : AssetsToSync)
                         {
                             if (SyncedAsset.GetObjectPathString() == AssetPath)
                             {
                                 bFound = true;
                                 break;
                             }
                         }
                         
                         if (!bFound)
                         {
                             UE_LOG(LogTemp, Warning, TEXT("尝试直接同步路径: %s"), *AssetPath);
                             // 这里可以添加额外的同步逻辑
                         }
                     }
                     
                     // 强制刷新内容浏览器 - 使用正确的方法
                     // 通过重新同步所有资产来强制刷新
                     if (AssetsToSync.Num() > 0)
                     {
                         ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);
                     }
                 }
        });
        FTimerHandle TimerHandle;
        World->GetTimerManager().SetTimer(TimerHandle, SyncDelegate, DelaySeconds, false);
    }
}

// 将PCM数据导出为WAV格式
bool UGenTools::ExportPCMToWavData(const TArray<uint8>& AudioData, uint32 SampleRate, uint16 NumChannels, TArray<uint8>& OutWavData, bool bForceStereo)
{
    if (AudioData.Num() == 0 || SampleRate == 0 || NumChannels == 0) return false;
    
    uint16 BitsPerSample = 16; // 16位PCM
    uint16 OutputChannels = NumChannels;
    uint32 DataSize = AudioData.Num();
    
    // 如果需要强制双声道且当前是单声道，则扩展数据
    if (bForceStereo && NumChannels == 1)
    {
        OutputChannels = 2;
        DataSize = DataSize * 2; // 双声道数据是单声道的两倍
    }
    
    OutWavData.SetNumUninitialized(44 + DataSize);
    
    // RIFF头
    FMemory::Memcpy(OutWavData.GetData(), "RIFF", 4);
    *(uint32*)(OutWavData.GetData() + 4) = 36 + DataSize;
    FMemory::Memcpy(OutWavData.GetData() + 8, "WAVE", 4);
    
    // fmt子块
    FMemory::Memcpy(OutWavData.GetData() + 12, "fmt ", 4);
    *(uint32*)(OutWavData.GetData() + 16) = 16; // PCM
    *(uint16*)(OutWavData.GetData() + 20) = 1; // PCM格式
    *(uint16*)(OutWavData.GetData() + 22) = OutputChannels;
    *(uint32*)(OutWavData.GetData() + 24) = SampleRate;
    *(uint32*)(OutWavData.GetData() + 28) = SampleRate * OutputChannels * BitsPerSample / 8;
    *(uint16*)(OutWavData.GetData() + 32) = OutputChannels * BitsPerSample / 8;
    *(uint16*)(OutWavData.GetData() + 34) = BitsPerSample;
    
    // data子块
    FMemory::Memcpy(OutWavData.GetData() + 36, "data", 4);
    *(uint32*)(OutWavData.GetData() + 40) = DataSize;
    
    // PCM数据
    if (bForceStereo && NumChannels == 1)
    {
        // 将单声道数据复制到双声道
        const int16* MonoPCM = reinterpret_cast<const int16*>(AudioData.GetData());
        int16* StereoPCM = reinterpret_cast<int16*>(OutWavData.GetData() + 44);
        
        int32 SampleCount = AudioData.Num() / sizeof(int16);
        for (int32 i = 0; i < SampleCount; ++i)
        {
            int16 Sample = MonoPCM[i];
            StereoPCM[i * 2] = Sample;     // 左声道
            StereoPCM[i * 2 + 1] = Sample; // 右声道
        }
        
        UE_LOG(LogTemp, Log, TEXT("成功将单声道PCM转换为双声道WAV，原始大小: %d 字节，转换后大小: %d 字节"), 
               AudioData.Num(), DataSize);
    }
    else
    {
        // 直接复制PCM数据
        FMemory::Memcpy(OutWavData.GetData() + 44, AudioData.GetData(), AudioData.Num());
    }
    
    return true;
}


// 处理TTS存储路径的子目录创建
bool UGenTools::SetupTTSStoragePaths(const FString& BasePath, FString& OutAssetPath, FString& OutWavDir)
{
    if (BasePath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("BasePath为空，无法设置TTS存储路径"));
        return false;
    }
    
    // 设置Asset路径（UE路径格式）
    OutAssetPath = BasePath + TEXT("Asset/");
    
    // 设置Wav目录（实际文件系统路径）- 仅设置路径，不创建目录
    FString ProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    FString RelativePath = BasePath;
    if (RelativePath.StartsWith(TEXT("/Game/")))
    {
        RelativePath = RelativePath.RightChop(6); // 移除 "/Game/"
    }
    OutWavDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectContentDir, RelativePath, TEXT("Wav")));
    
    // 创建目录
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    // 创建Asset目录（对应的文件系统路径）
    FString AssetDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectContentDir, RelativePath, TEXT("Asset")));
    if (!PlatformFile.DirectoryExists(*AssetDir))
    {
        if (!PlatformFile.CreateDirectoryTree(*AssetDir))
        {
            UE_LOG(LogTemp, Error, TEXT("创建Asset目录失败: %s"), *AssetDir);
            return false;
        }
        UE_LOG(LogTemp, Log, TEXT("已创建Asset目录: %s"), *AssetDir);
    }
    
    UE_LOG(LogTemp, Log, TEXT("TTS存储路径设置完成 - AssetPath: %s, WavDir: %s"), *OutAssetPath, *OutWavDir);
    return true;
}

// 恢复插件窗口焦点
void UGenTools::BringPluginWindowToFront()
{
#if WITH_EDITOR
    // Intentionally left blank after editor-module separation.
#endif
}

// 从音频数据创建SoundWave（用于预览播放）
USoundWave* UGenTools::CreateSoundWaveFromAudioData(const TArray<uint8>& AudioData, bool bForceStereo)
{
    if (AudioData.Num() == 0)
    {
        return nullptr;
    }

    // 如果需要双声道转换，先处理音频数据
    TArray<uint8> FinalAudioData = AudioData;
    if (bForceStereo)
    {
        // 先解析原始音频数据获取声道信息
        FWaveModInfo TempWaveInfo;
        if (TempWaveInfo.ReadWaveInfo(AudioData.GetData(), AudioData.Num()))
        {
            uint16 NumChannels = *TempWaveInfo.pChannels;
            uint32 SampleRate = *TempWaveInfo.pSamplesPerSec;
            
            // 如果当前是单声道，转换为双声道
            if (NumChannels == 1)
            {
                // 提取PCM数据
                TArray<uint8> PCMData;
                PCMData.SetNum(TempWaveInfo.SampleDataSize);
                FMemory::Memcpy(PCMData.GetData(), TempWaveInfo.SampleDataStart, TempWaveInfo.SampleDataSize);
                
                // 转换为双声道WAV
                if (UGenTools::ExportPCMToWavData(PCMData, SampleRate, NumChannels, FinalAudioData, true))
                {
                    UE_LOG(LogTemp, Log, TEXT("CreateSoundWaveFromAudioData: 成功将单声道音频转换为双声道"));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("CreateSoundWaveFromAudioData: 双声道转换失败，使用原始音频"));
                    FinalAudioData = AudioData;
                }
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("CreateSoundWaveFromAudioData: 音频已经是多声道，无需转换"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("CreateSoundWaveFromAudioData: 无法解析音频数据，使用原始音频"));
            FinalAudioData = AudioData;
        }
    }

    // 创建临时SoundWave对象（不保存到磁盘）
    USoundWave* SoundWave = NewObject<USoundWave>(GetTransientPackage());
    if (!SoundWave)
    {
        return nullptr;
    }

    // 解析WAV数据并填充SoundWave
    FWaveModInfo WaveInfo;
    if (WaveInfo.ReadWaveInfo(FinalAudioData.GetData(), FinalAudioData.Num()))
    {
        SoundWave->InvalidateCompressedData();
        const FSharedBuffer UpdatedBuffer = FSharedBuffer::Clone(FinalAudioData.GetData(), FinalAudioData.Num());
        SoundWave->RawData.UpdatePayload(UpdatedBuffer);

        if (const int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec)
        {
            SoundWave->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
        }
        else
        {
            SoundWave->Duration = 0.0f;
        }

        SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
        SoundWave->NumChannels = *WaveInfo.pChannels;
        SoundWave->RawPCMDataSize = WaveInfo.SampleDataSize;
        SoundWave->SoundGroup = SOUNDGROUP_Default;
        SoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SoundWave->RawPCMDataSize));
        FMemory::Memmove(SoundWave->RawPCMData, WaveInfo.SampleDataStart, SoundWave->RawPCMDataSize);
        SoundWave->Volume = 1.0f;
        
        return SoundWave;
    }
    
    return nullptr;
}

// 从WAV文件创建SoundWave（用于预览播放）
USoundWave* UGenTools::CreateSoundWaveFromWavFile(const FString& WavFilePath)
{
    if (WavFilePath.IsEmpty())
    {
        return nullptr;
    }

    // 读取WAV文件
    TArray<uint8> AudioData;
    if (!FFileHelper::LoadFileToArray(AudioData, *WavFilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("无法读取WAV文件: %s"), *WavFilePath);
        return nullptr;
    }

    // 使用音频数据创建SoundWave
    return CreateSoundWaveFromAudioData(AudioData);
}

// 检查音频资产是否已存在
bool UGenTools::CheckAudioAssetExists(const FString& AssetName, const FString& AssetPackagePath)
{
    if (AssetName.IsEmpty())
    {
        return false;
    }
    
    // 获取配置的TTS存储路径
    FString FinalAssetPackagePath = AssetPackagePath;
    if (FinalAssetPackagePath.IsEmpty())
    {
        // 使用配置的TTS存储路径，参考AudioAssetOperation.cpp中的逻辑
        const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
        FString AssetPathDir, WavDir;
        if (UGenTools::SetupTTSStoragePaths(Settings->TTSStoragePath, AssetPathDir, WavDir))
        {
            FinalAssetPackagePath = AssetPathDir;
        }
        else
        {
            // 如果配置路径设置失败，使用备用路径
            FinalAssetPackagePath = TEXT("/Game/YDAutomation/Audio/");
            UE_LOG(LogTemp, Warning, TEXT("使用配置路径失败，使用备用路径: %s"), *FinalAssetPackagePath);
        }
    }
    
    // 确保路径以/结尾
    if (!FinalAssetPackagePath.EndsWith(TEXT("/")))
    {
        FinalAssetPackagePath += TEXT("/");
    }
    
    FString FullPackageName = FinalAssetPackagePath + AssetName;
    FString AssetObjectPath = FullPackageName + TEXT(".") + AssetName;
    
    // 使用资产注册表检查资产是否存在
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    FSoftObjectPath SoftPath(AssetObjectPath);
    FAssetData ExistingAsset = AssetRegistryModule.Get().GetAssetByObjectPath(SoftPath);
    
    UE_LOG(LogTemp, Log, TEXT("检查资产是否存在: %s"), *AssetObjectPath);
    
    return ExistingAsset.IsValid();
}
