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

// === 静态成员定义 ===
TMap<FString, TArray<uint8>> UTTSSetting::CachedVoiceData;
TMap<FString, TArray<uint8>> UTTSSetting::CachedEmotionData;

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
    
    // 添加缓存中的自定义音色
    UE_LOG(LogTemp, Log, TEXT("GetSpeakerOptions: 缓存中有 %d 个自定义音色"), CachedVoiceData.Num());
    for (const auto& CachedVoice : CachedVoiceData)
    {
        Result.Add(CachedVoice.Key);
        UE_LOG(LogTemp, Log, TEXT("添加缓存音色: %s"), *CachedVoice.Key);
    }
    
    // 获取当前TTS设置实例来检查自定义音色
    UTTSSetting* CurrentSettings = Get();
    if (CurrentSettings && !CurrentSettings->VoiceAudioFileName.IsEmpty())
    {
        FString CustomSpeakerName = FString::Printf(TEXT("新音色_%s"), *CurrentSettings->VoiceAudioFileName);
        // 检查是否已经存在，避免重复添加
        if (!Result.Contains(CustomSpeakerName))
        {
            Result.Add(CustomSpeakerName);
        }
    }
    
    // 如果当前音色为空且音色列表不为空，自动设置第一个音色为默认值
    if (CurrentSettings && CurrentSettings->SpeakerName.IsEmpty() && Result.Num() > 0 && Result[0] != TEXT("未加载音色列表"))
    {
        CurrentSettings->SpeakerName = Result[0];
        UE_LOG(LogTemp, Log, TEXT("自动设置默认音色: %s"), *CurrentSettings->SpeakerName);
    }
    
    return Result;
}

TArray<FString> UTTSSetting::GetEmotionOptions()
{
    TArray<FString> Result;
    
    // 添加标准情感选项
    Result.Add(TEXT("自适应"));
    Result.Add(TEXT("开心"));
    Result.Add(TEXT("伤心"));
    Result.Add(TEXT("愤怒"));
    Result.Add(TEXT("恐惧"));
    Result.Add(TEXT("厌恶"));
    Result.Add(TEXT("惊讶"));
    Result.Add(TEXT("中性"));
    
    // 添加缓存中的自定义情感
    for (const auto& CachedEmotion : CachedEmotionData)
    {
        if (!Result.Contains(CachedEmotion.Key))
        {
            Result.Add(CachedEmotion.Key);
        }
    }
    
    // 获取当前TTS设置实例来检查自定义情感
    UTTSSetting* CurrentSettings = Get();
    if (CurrentSettings && !CurrentSettings->CustomEmotionName.IsEmpty())
    {
        if (!Result.Contains(CurrentSettings->CustomEmotionName))
        {
            Result.Add(CurrentSettings->CustomEmotionName);
        }
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

void UTTSSetting::ProcessVoiceAudioSelection()
{
    if (!VoiceAudio.IsValid())
    {
        // 清除之前的音色音频数据
        VoiceAudioBytes.Empty();
        VoiceAudioFileName = TEXT("");
        bUseCustomVoiceAudio = false;
        return;
    }

    USoundWave* SoundWave = VoiceAudio.LoadSynchronous();
    if (!SoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("无法加载音色音频文件"));
        return;
    }

    // 从路径中提取文件名（不包含扩展名）
    FString AudioFilePath = SoundWave->GetPathName();
    FString FileName = FPaths::GetBaseFilename(AudioFilePath);
    VoiceAudioFileName = FileName;

    // 尝试获取音频的原始文件数据
    TArray<uint8> AudioFileData;
    if (GetAudioFileData(SoundWave, AudioFileData))
    {
        VoiceAudioBytes = AudioFileData;
        bUseCustomVoiceAudio = true;
        UE_LOG(LogTemp, Log, TEXT("音色音频文件数据已获取，大小: %d 字节，文件名: %s"), VoiceAudioBytes.Num(), *FileName);
        
        // 自动设置音色为新的自定义音色
        FString CustomSpeakerName = FString::Printf(TEXT("新音色_%s"), *VoiceAudioFileName);
        SpeakerName = CustomSpeakerName;
        
        // 添加到缓存
        CachedVoiceData.Add(CustomSpeakerName, VoiceAudioBytes);
        UE_LOG(LogTemp, Log, TEXT("音色音频已添加到缓存: %s"), *CustomSpeakerName);
        
        // 打印音频字节流信息
        PrintAudioBytesInfo(TEXT("音色音频"), VoiceAudioBytes);
        return;
    }
    
    // 如果无法获取文件数据，尝试PCM数据作为备选
    TArray<uint8> PCMData;
    uint16 NumChannels = 0;
    uint32 SampleRate = 0;
    
    if (SoundWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
    {
        // 将PCM数据转换为WAV格式
        if (ConvertPCMToWAV(PCMData.GetData(), PCMData.Num(), SampleRate, NumChannels, VoiceAudioBytes))
        {
            bUseCustomVoiceAudio = true;
            UE_LOG(LogTemp, Log, TEXT("音色音频PCM数据已转换为WAV格式，大小: %d 字节，文件名: %s"), VoiceAudioBytes.Num(), *FileName);
            
            // 自动设置音色为新的自定义音色
            FString CustomSpeakerName = FString::Printf(TEXT("新音色_%s"), *VoiceAudioFileName);
            SpeakerName = CustomSpeakerName;
            
            // 添加到缓存
            CachedVoiceData.Add(CustomSpeakerName, VoiceAudioBytes);
            UE_LOG(LogTemp, Log, TEXT("音色音频已添加到缓存: %s"), *CustomSpeakerName);
            
            // 打印音频字节流信息
            PrintAudioBytesInfo(TEXT("音色音频"), VoiceAudioBytes);
            return;
        }
    }
    else
    {
        // 如果RawPCMData不可用，尝试获取压缩数据
        FByteBulkData* CompressedBulkData = SoundWave->GetCompressedData(SoundWave->GetRuntimeFormat());
        if (CompressedBulkData && CompressedBulkData->GetBulkDataSize() > 0)
        {
            int32 CompressedDataSize = CompressedBulkData->GetBulkDataSize();
            VoiceAudioBytes.SetNum(CompressedDataSize);
            
            void* CompressedData = nullptr;
            CompressedBulkData->GetCopy(&CompressedData, true);
            if (CompressedData)
            {
                FMemory::Memcpy(VoiceAudioBytes.GetData(), CompressedData, CompressedDataSize);
                bUseCustomVoiceAudio = true;
                UE_LOG(LogTemp, Log, TEXT("音色音频压缩数据已编码为字节流，大小: %d 字节，文件名: %s"), VoiceAudioBytes.Num(), *FileName);
                
                // 自动设置音色为新的自定义音色
                FString CustomSpeakerName = FString::Printf(TEXT("新音色_%s"), *VoiceAudioFileName);
                SpeakerName = CustomSpeakerName;
                
                // 添加到缓存
                CachedVoiceData.Add(CustomSpeakerName, VoiceAudioBytes);
                UE_LOG(LogTemp, Log, TEXT("音色音频已添加到缓存: %s"), *CustomSpeakerName);
                
                // 打印音频字节流信息
                PrintAudioBytesInfo(TEXT("音色音频"), VoiceAudioBytes);
                return;
            }
        }
        
        // 如果压缩数据也不可用，尝试获取流式数据
        TArrayView<const uint8> ZerothChunk = SoundWave->GetZerothChunk();
        if (ZerothChunk.Num() > 0)
        {
            VoiceAudioBytes.SetNum(ZerothChunk.Num());
            FMemory::Memcpy(VoiceAudioBytes.GetData(), ZerothChunk.GetData(), ZerothChunk.Num());
            bUseCustomVoiceAudio = true;
            UE_LOG(LogTemp, Log, TEXT("音色音频流式数据已编码为字节流，大小: %d 字节，文件名: %s"), VoiceAudioBytes.Num(), *FileName);
            
            // 自动设置音色为新的自定义音色
            FString CustomSpeakerName = FString::Printf(TEXT("新音色_%s"), *VoiceAudioFileName);
            SpeakerName = CustomSpeakerName;
            
            // 添加到缓存
            CachedVoiceData.Add(CustomSpeakerName, VoiceAudioBytes);
            UE_LOG(LogTemp, Log, TEXT("音色音频已添加到缓存: %s"), *CustomSpeakerName);
            
            // 打印音频字节流信息
            PrintAudioBytesInfo(TEXT("音色音频"), VoiceAudioBytes);
            return;
        }
        
        UE_LOG(LogTemp, Error, TEXT("无法获取音色音频数据，PCM、压缩数据和流式数据都为空"));
        VoiceAudioBytes.Empty();
        bUseCustomVoiceAudio = false;
    }
}

void UTTSSetting::ProcessEmotionAudioSelection()
{
    if (!EmotionAudio.IsValid())
    {
        // 清除之前的情感音频数据
        EmotionAudioBytes.Empty();
        EmotionAudioFileName = TEXT("");
        bUseCustomEmotionAudio = false;
        return;
    }

    USoundWave* SoundWave = EmotionAudio.LoadSynchronous();
    if (!SoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("无法加载情感音频文件"));
        return;
    }

    // 从路径中提取文件名（不包含扩展名）
    FString AudioFilePath = SoundWave->GetPathName();
    FString FileName = FPaths::GetBaseFilename(AudioFilePath);
    EmotionAudioFileName = FileName;

    // 尝试获取音频的原始文件数据
    TArray<uint8> AudioFileData;
    if (GetAudioFileData(SoundWave, AudioFileData))
    {
        EmotionAudioBytes = AudioFileData;
        bUseCustomEmotionAudio = true;
        UE_LOG(LogTemp, Log, TEXT("情感音频文件数据已获取，大小: %d 字节，文件名: %s"), EmotionAudioBytes.Num(), *FileName);
        
        // 自动设置情感为新的自定义情感
        CustomEmotionName = FString::Printf(TEXT("新情感_%s"), *EmotionAudioFileName);
        EmotionName = CustomEmotionName;
        
        // 添加到缓存
        CachedEmotionData.Add(CustomEmotionName, EmotionAudioBytes);
        UE_LOG(LogTemp, Log, TEXT("情感音频已添加到缓存: %s"), *CustomEmotionName);
        
        // 打印音频字节流信息
        PrintAudioBytesInfo(TEXT("情感音频"), EmotionAudioBytes);
        return;
    }
    
    // 如果无法获取文件数据，尝试PCM数据作为备选
    TArray<uint8> PCMData;
    uint16 NumChannels = 0;
    uint32 SampleRate = 0;
    
    if (SoundWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
    {
        // 将PCM数据转换为WAV格式
        if (ConvertPCMToWAV(PCMData.GetData(), PCMData.Num(), SampleRate, NumChannels, EmotionAudioBytes))
        {
            bUseCustomEmotionAudio = true;
            UE_LOG(LogTemp, Log, TEXT("情感音频PCM数据已转换为WAV格式，大小: %d 字节，文件名: %s"), EmotionAudioBytes.Num(), *FileName);
            
            // 自动设置情感为新的自定义情感
            CustomEmotionName = FString::Printf(TEXT("新情感_%s"), *EmotionAudioFileName);
            EmotionName = CustomEmotionName;
            
            // 添加到缓存
            CachedEmotionData.Add(CustomEmotionName, EmotionAudioBytes);
            UE_LOG(LogTemp, Log, TEXT("情感音频已添加到缓存: %s"), *CustomEmotionName);
            
            // 打印音频字节流信息
            PrintAudioBytesInfo(TEXT("情感音频"), EmotionAudioBytes);
            return;
        }
    }
    else
    {
        // 如果RawPCMData不可用，尝试获取压缩数据
        FByteBulkData* CompressedBulkData = SoundWave->GetCompressedData(SoundWave->GetRuntimeFormat());
        if (CompressedBulkData && CompressedBulkData->GetBulkDataSize() > 0)
        {
            int32 CompressedDataSize = CompressedBulkData->GetBulkDataSize();
            EmotionAudioBytes.SetNum(CompressedDataSize);
            
            void* CompressedData = nullptr;
            CompressedBulkData->GetCopy(&CompressedData, true);
            if (CompressedData)
            {
                FMemory::Memcpy(EmotionAudioBytes.GetData(), CompressedData, CompressedDataSize);
                bUseCustomEmotionAudio = true;
                UE_LOG(LogTemp, Log, TEXT("情感音频压缩数据已编码为字节流，大小: %d 字节，文件名: %s"), EmotionAudioBytes.Num(), *FileName);
                
                // 自动设置情感为新的自定义情感
                CustomEmotionName = FString::Printf(TEXT("新情感_%s"), *EmotionAudioFileName);
                EmotionName = CustomEmotionName;
                
                // 添加到缓存
                CachedEmotionData.Add(CustomEmotionName, EmotionAudioBytes);
                UE_LOG(LogTemp, Log, TEXT("情感音频已添加到缓存: %s"), *CustomEmotionName);
                
                // 打印音频字节流信息
                PrintAudioBytesInfo(TEXT("情感音频"), EmotionAudioBytes);
                return;
            }
        }
        
        // 如果压缩数据也不可用，尝试获取流式数据
        TArrayView<const uint8> ZerothChunk = SoundWave->GetZerothChunk();
        if (ZerothChunk.Num() > 0)
        {
            EmotionAudioBytes.SetNum(ZerothChunk.Num());
            FMemory::Memcpy(EmotionAudioBytes.GetData(), ZerothChunk.GetData(), ZerothChunk.Num());
            bUseCustomEmotionAudio = true;
            UE_LOG(LogTemp, Log, TEXT("情感音频流式数据已编码为字节流，大小: %d 字节，文件名: %s"), EmotionAudioBytes.Num(), *FileName);
            
            // 自动设置情感为新的自定义情感
            CustomEmotionName = FString::Printf(TEXT("新情感_%s"), *EmotionAudioFileName);
            EmotionName = CustomEmotionName;
            
            // 添加到缓存
            CachedEmotionData.Add(CustomEmotionName, EmotionAudioBytes);
            UE_LOG(LogTemp, Log, TEXT("情感音频已添加到缓存: %s"), *CustomEmotionName);
            
            // 打印音频字节流信息
            PrintAudioBytesInfo(TEXT("情感音频"), EmotionAudioBytes);
            return;
        }
        
        UE_LOG(LogTemp, Error, TEXT("无法获取情感音频数据，PCM、压缩数据和流式数据都为空"));
        EmotionAudioBytes.Empty();
        bUseCustomEmotionAudio = false;
    }
}

bool UTTSSetting::GetCurrentSpeakerData(FString& OutSpeakerName, TArray<uint8>& OutAudioBytes)
{
    OutAudioBytes.Empty();
    
    // 首先检查是否有自定义音色音频数据
    if (VoiceAudioBytes.Num() > 0)
    {
        // 使用自定义音色音频
        OutSpeakerName = FString::Printf(TEXT("新音色_%s"), *VoiceAudioFileName);
        OutAudioBytes = VoiceAudioBytes;
        return true; // 有音频数据
    }
    else
    {
        // 检查是否从缓存中选择音色
        if (!SpeakerName.IsEmpty())
        {
            // 检查缓存中是否有这个音色
            if (CachedVoiceData.Contains(SpeakerName))
            {
                OutSpeakerName = SpeakerName;
                OutAudioBytes = CachedVoiceData[SpeakerName];
                return true; // 有音频数据
            }
            else
            {
                // 缓存中不存在这个音色，说明是预设音色，直接使用音色字符串
                OutSpeakerName = SpeakerName;
                return false; // 没有音频数据，但有音色字符串
            }
        }
        
        // 使用原有音色逻辑
        OutSpeakerName = SpeakerName;
        return false; // 没有音频数据
    }
}

bool UTTSSetting::GetCurrentEmotionData(FString& OutEmotionName, TArray<uint8>& OutAudioBytes)
{
    OutAudioBytes.Empty();
    
    // 首先检查是否有自定义情感音频数据
    if (EmotionAudioBytes.Num() > 0)
    {
        // 使用自定义情感音频
        OutEmotionName = CustomEmotionName.IsEmpty() ? 
            FString::Printf(TEXT("新情感_%s"), *EmotionAudioFileName) : 
            CustomEmotionName;
        OutAudioBytes = EmotionAudioBytes;
        return true;
    }
    else
    {
        // 检查是否从缓存中选择情感
        if (!EmotionName.IsEmpty())
        {
            // 检查缓存中是否有这个情感
            if (CachedEmotionData.Contains(EmotionName))
            {
                OutEmotionName = EmotionName;
                OutAudioBytes = CachedEmotionData[EmotionName];
                return true;
            }
            else
            {
                // 缓存中不存在这个情感，说明是预设情感，直接使用情感字符串
                OutEmotionName = EmotionName;
                return false; // 没有音频数据，但有情感字符串
            }
        }
        
        // 使用原有情感逻辑
        OutEmotionName = EmotionName.IsEmpty() ? TEXT("自适应") : EmotionName;
        return false; // 表示没有音频字节流
    }
}

// === 音频格式转换方法实现 ===

bool UTTSSetting::GetAudioFileData(USoundWave* SoundWave, TArray<uint8>& OutAudioData)
{
    if (!SoundWave)
    {
        return false;
    }
    
    // 尝试从原始文件路径读取数据
    FString AudioFilePath = SoundWave->GetPathName();
    if (AudioFilePath.StartsWith(TEXT("/Game/")))
    {
        // 这是一个UE资产路径，需要转换为实际文件路径
        FString PackagePath = FPackageName::LongPackageNameToFilename(AudioFilePath, TEXT(".uasset"));
        FString ContentDir = FPaths::ProjectContentDir();
        FString FullPath = FPaths::Combine(ContentDir, PackagePath);
        
        // 尝试读取原始音频文件
        if (FFileHelper::LoadFileToArray(OutAudioData, *FullPath))
        {
            UE_LOG(LogTemp, Log, TEXT("成功从文件路径读取音频数据: %s"), *FullPath);
            return true;
        }
        
        // 如果.uasset文件不存在，尝试其他格式
        TArray<FString> AudioExtensions = { TEXT(".wav"), TEXT(".mp3"), TEXT(".ogg"), TEXT(".flac") };
        for (const FString& Ext : AudioExtensions)
        {
            FString AudioPath = FPackageName::LongPackageNameToFilename(AudioFilePath, Ext);
            FString AudioFullPath = FPaths::Combine(ContentDir, AudioPath);
            if (FFileHelper::LoadFileToArray(OutAudioData, *AudioFullPath))
            {
                UE_LOG(LogTemp, Log, TEXT("成功从音频文件读取数据: %s"), *AudioFullPath);
                return true;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("无法获取音频文件的原始数据"));
    return false;
}

bool UTTSSetting::ConvertPCMToWAV(const uint8* PCMData, int32 PCMDataSize, int32 SampleRate, int32 NumChannels, TArray<uint8>& OutWAVData)
{
    if (!PCMData || PCMDataSize <= 0 || SampleRate <= 0 || NumChannels <= 0)
    {
        return false;
    }
    
    // WAV文件头结构
    struct FWAVHeader
    {
        // RIFF头
        char ChunkID[4];        // "RIFF"
        uint32 ChunkSize;       // 文件大小 - 8
        char Format[4];         // "WAVE"
        
        // fmt子块
        char Subchunk1ID[4];    // "fmt "
        uint32 Subchunk1Size;   // 16 for PCM
        uint16 AudioFormat;     // 1 for PCM
        uint16 NumChannels;     // 声道数
        uint32 SampleRate;      // 采样率
        uint32 ByteRate;        // 字节率 = SampleRate * NumChannels * BitsPerSample/8
        uint16 BlockAlign;      // 块对齐 = NumChannels * BitsPerSample/8
        uint16 BitsPerSample;   // 位深度
        
        // data子块
        char Subchunk2ID[4];    // "data"
        uint32 Subchunk2Size;   // 音频数据大小
    };
    
    const int32 BitsPerSample = 16; // 假设16位PCM
    const int32 BytesPerSample = BitsPerSample / 8;
    
    // 计算WAV文件大小
    int32 WAVFileSize = sizeof(FWAVHeader) + PCMDataSize;
    
    // 分配内存
    OutWAVData.SetNum(WAVFileSize);
    
    // 创建WAV头
    FWAVHeader Header;
    FMemory::Memzero(&Header, sizeof(Header));
    
    // 设置RIFF头
    FMemory::Memcpy(Header.ChunkID, "RIFF", 4);
    Header.ChunkSize = WAVFileSize - 8;
    FMemory::Memcpy(Header.Format, "WAVE", 4);
    
    // 设置fmt子块
    FMemory::Memcpy(Header.Subchunk1ID, "fmt ", 4);
    Header.Subchunk1Size = 16;
    Header.AudioFormat = 1; // PCM
    Header.NumChannels = NumChannels;
    Header.SampleRate = SampleRate;
    Header.ByteRate = SampleRate * NumChannels * BytesPerSample;
    Header.BlockAlign = NumChannels * BytesPerSample;
    Header.BitsPerSample = BitsPerSample;
    
    // 设置data子块
    FMemory::Memcpy(Header.Subchunk2ID, "data", 4);
    Header.Subchunk2Size = PCMDataSize;
    
    // 写入WAV头
    FMemory::Memcpy(OutWAVData.GetData(), &Header, sizeof(Header));
    
    // 写入PCM数据
    FMemory::Memcpy(OutWAVData.GetData() + sizeof(Header), PCMData, PCMDataSize);
    
    UE_LOG(LogTemp, Log, TEXT("PCM数据已转换为WAV格式，大小: %d 字节，采样率: %d，声道数: %d"), 
           OutWAVData.Num(), SampleRate, NumChannels);
    
    return true;
}

void UTTSSetting::PrintAudioBytesInfo(const FString& AudioType, const TArray<uint8>& AudioBytes)
{
    if (AudioBytes.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s音频字节流为空"), *AudioType);
        return;
    }

    // 打印基本信息
    UE_LOG(LogTemp, Log, TEXT("=== %s音频字节流信息 ==="), *AudioType);
    UE_LOG(LogTemp, Log, TEXT("字节流大小: %d 字节"), AudioBytes.Num());
    UE_LOG(LogTemp, Log, TEXT("字节流大小(KB): %.2f KB"), AudioBytes.Num() / 1024.0f);
    UE_LOG(LogTemp, Log, TEXT("字节流大小(MB): %.2f MB"), AudioBytes.Num() / (1024.0f * 1024.0f));

    // 打印前32个字节的十六进制表示
    FString HexString = TEXT("前32字节(十六进制): ");
    int32 BytesToShow = FMath::Min(32, AudioBytes.Num());
    for (int32 i = 0; i < BytesToShow; ++i)
    {
        HexString += FString::Printf(TEXT("%02X "), AudioBytes[i]);
    }
    UE_LOG(LogTemp, Log, TEXT("%s"), *HexString);

    // 如果是PCM数据，尝试解析音频信息
    if (AudioType.Contains(TEXT("PCM")))
    {
        UE_LOG(LogTemp, Log, TEXT("检测到PCM数据，尝试解析音频信息..."));
        
        // 这里可以添加PCM数据解析逻辑
        // 例如：采样率、声道数、位深度等
    }
}

void UTTSSetting::ClearAllCachedData()
{
    int32 VoiceCacheCount = CachedVoiceData.Num();
    int32 EmotionCacheCount = CachedEmotionData.Num();
    
    CachedVoiceData.Empty();
    CachedEmotionData.Empty();
    
    // 获取当前TTS设置实例并清空选中的值
    UTTSSetting* CurrentSettings = Get();
    if (CurrentSettings)
    {
        // 清空音色选择
        CurrentSettings->SpeakerName = TEXT("");
        CurrentSettings->VoiceAudioBytes.Empty();
        CurrentSettings->VoiceAudioFileName = TEXT("");
        
        // 清空情感选择
        CurrentSettings->EmotionName = TEXT("自适应");
        CurrentSettings->EmotionAudioBytes.Empty();
        CurrentSettings->EmotionAudioFileName = TEXT("");
        CurrentSettings->CustomEmotionName = TEXT("");
        
        UE_LOG(LogTemp, Log, TEXT("已清除所有缓存数据并重置选中值 - 音色缓存: %d 个，情感缓存: %d 个"), VoiceCacheCount, EmotionCacheCount);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("已清除所有缓存数据 - 音色缓存: %d 个，情感缓存: %d 个"), VoiceCacheCount, EmotionCacheCount);
    }
} 