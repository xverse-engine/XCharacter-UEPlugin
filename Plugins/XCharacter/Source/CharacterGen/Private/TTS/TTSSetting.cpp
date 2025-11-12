// Fill out your copyright notice in the Description page of Project Settings.

#include "TTS/TTSSetting.h"
#include "TTS/TTSServer.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Engine/Engine.h"
#include "HAL/UnrealMemory.h"
#include "Misc/PackageName.h"


// === 基础方法实现 ===

void UTTSSetting::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
}

UTTSSetting* UTTSSetting::Get()
{
    static UTTSSetting* DefaultSettings = nullptr;
    if (!DefaultSettings)
    {
        // This is a singleton, use default object
        DefaultSettings = DuplicateObject(GetMutableDefault<UTTSSetting>(), GetTransientPackage());
        DefaultSettings->AddToRoot();
    }

    return DefaultSettings;
}

TArray<FString> UTTSSetting::GetSpeakerOptions()
{
    TArray<FString> Result = UTTSServer::GetCachedSpeakerOptions();
    UE_LOG(LogTemp, Log, TEXT("GetSpeakerOptions: 从服务器获取到 %d 个音色选项"), Result.Num());
    
    if (Result.Num() == 0)
    {
        Result.Add(TEXT("未加载音色列表"));
        UE_LOG(LogTemp, Warning, TEXT("音色列表为空，添加默认选项"));
    }
    
    // 如果当前音色为空且音色列表不为空，自动设置第一个音色为默认值
    UTTSSetting* CurrentSettings = Get();
    if (CurrentSettings && CurrentSettings->SpeakerName.IsEmpty() && Result.Num() > 0 && Result[0] != TEXT("未加载音色列表"))
    {
        CurrentSettings->SpeakerName = Result[0];
        UE_LOG(LogTemp, Log, TEXT("自动设置默认音色: %s"), *CurrentSettings->SpeakerName);
    }
    
    return Result;
}


bool UTTSSetting::SynthesizeTTSAudio()
{
    if (TTSInputText.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("TTS合成失败: TTS文本为空"));
        return false;
    }

    if (SpeakerName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("TTS合成失败: 未选择音色"));
        return false;
    }

    // 调用TTS服务（默认使用分批处理，批次大小为2）
    UTTSServer::CallTTSServer(this);
    UE_LOG(LogTemp, Log, TEXT("TTS音频合成请求已发送"));
    return true;
}

bool UTTSSetting::SynthesizeTTSAudioBatched(int32 BatchSize)
{
    if (TTSInputText.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("TTS合成失败: TTS文本为空"));
        return false;
    }

    if (SpeakerName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("TTS合成失败: 未选择音色"));
        return false;
    }

    // 调用TTS服务（分批处理）
    UTTSServer::CallTTSServerBatched(this, BatchSize);
    UE_LOG(LogTemp, Log, TEXT("TTS音频合成请求已发送（分批处理，批次大小: %d）"), BatchSize);
    return true;
}

// === 音频处理方法实现 ===

bool UTTSSetting::GetCurrentSpeakerData(FString& OutSpeakerName, TArray<uint8>& OutAudioBytes)
{
    // 简化版本：只返回音色名称，不返回音频数据（豆包TTS不需要音频数据）
    OutAudioBytes.Empty();
    OutSpeakerName = SpeakerName;
    return false; // 表示没有音频数据
} 