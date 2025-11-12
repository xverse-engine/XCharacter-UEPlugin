// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"
#include "LevelSequence.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sound/SoundWave.h"
#include "Containers/Array.h"
#include "AudioAssetOperation.generated.h"

// 包装TArray<uint8>的结构体，避免嵌套TArray的语法问题
USTRUCT(BlueprintType)
struct CHARACTERGEN_API FAudioDataWrapper
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioDataWrapper")
    TArray<uint8> AudioData;
};

// 前置声明（放在全局命名空间，避免出现 UAudioAssetOperation::UMovieSceneFolder）
class UMovieSceneFolder;

USTRUCT(BlueprintType)
struct CHARACTERGEN_API FAudioSegment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSegment")
    USoundWave* SoundWave = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSegment")
    FFrameNumber StartTime = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSegment")
    FFrameNumber EndTime = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSegment")
    FFrameNumber AudioOffset = 0;
};

USTRUCT(BlueprintType)
struct CHARACTERGEN_API FCameraCutSegment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraCutSegment")
    FFrameNumber StartTime = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraCutSegment")
    FFrameNumber EndTime = 0;
};

USTRUCT(BlueprintType)
struct CHARACTERGEN_API FAudioTrackInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioTrackInfo")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioTrackInfo")
    FString FullPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioTrackInfo")
    UMovieSceneAudioTrack* AudioTrack = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioTrackInfo")
    int32 HierarchyLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioTrackInfo")
    int32 TrackIndex = 0;

    // 如果该条目表示同一音频轨道中的某一行，则为该行索引；否则为 INDEX_NONE
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioTrackInfo")
    int32 RowIndex = INDEX_NONE;

    // Row的自定义名称（如果存在）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioTrackInfo")
    FString RowName;
};

UCLASS()
class CHARACTERGEN_API UAudioAssetOperation : public UObject
{
    GENERATED_BODY()

public:
    // 获取所有音频轨道信息（包括嵌套轨道）
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static TArray<FAudioTrackInfo> GetAllAudioTracksInfo(ULevelSequence* LevelSequence);

    

    // 获取轨道列表（兼容旧版本）
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static TArray<FString> GetTracksFromLevelSequence(ULevelSequence* LevelSequence);

    // 合并音频的核心方法（与Camera Cut Track对齐）
    // UserTargetSampleRate: 0=自动上采样到最高采样率, >0=使用指定采样率
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool MergeAudioSegments(ULevelSequence* LevelSequence, const FString& SelectedTrack, USoundWave*& OutMergedAudio, int32 UserTargetSampleRate = 0);

    // 应用合并后的音频到轨道
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ApplyMergedAudioToTrack(ULevelSequence* LevelSequence, const FString& SelectedTrack, USoundWave* MergedAudio);
    
    // 创建新的音轨并应用合并后的音频（保留原有音频）
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool CreateNewTrackWithMergedAudio(ULevelSequence* LevelSequence, const FString& SelectedTrack, USoundWave* MergedAudio);

    // 获取当前轨道上的音频片段数量
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static int32 GetAudioSegmentCount(ULevelSequence* LevelSequence, const FString& SelectedTrack);

    // 获取指定索引的音频片段
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool GetAudioSegmentByIndex(ULevelSequence* LevelSequence, const FString& SelectedTrack, int32 Index, FAudioSegment& OutSegment);

    // 获取Camera Cut Track的时间范围
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static TArray<FCameraCutSegment> GetCameraCutSegments(ULevelSequence* LevelSequence);

    // 清理静态变量（用于重置名称计数）
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static void ClearStaticVariables();

    // 调试打印所有轨道信息
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static void DebugPrintAllTracksInfo(ULevelSequence* LevelSequence);

    // TTS音频处理：添加静音并保存单个音频
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ProcessSingleAudioWithSilence(const FString& AssetPath, const FString& Text, float PreSilence, float PostSilence, USoundWave*& OutProcessedAudio, bool bForceStereo = false);

    // TTS音频处理：添加静音并合并多个音频
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ProcessMultipleAudioWithSilence(const TArray<FString>& AssetPaths, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, USoundWave*& OutMergedAudio, const FString& CustomFileName = TEXT(""), bool bForceStereo = false);

    // TTS音频处理：从内存数据添加静音并保存单个音频
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ProcessSingleAudioWithSilenceFromMemory(const TArray<uint8>& AudioData, const FString& Text, float PreSilence, float PostSilence, USoundWave*& OutProcessedAudio, bool bForceStereo = false);

    // TTS音频处理：从文件添加静音并保存单个音频
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ProcessSingleAudioWithSilenceFromFile(const FString& FilePath, const FString& Text, float PreSilence, float PostSilence, USoundWave*& OutProcessedAudio, bool bForceStereo = false);

    // TTS音频处理：从内存数据添加静音并合并多个音频
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ProcessMultipleAudioWithSilenceFromMemory(const TArray<FAudioDataWrapper>& AudioDataArray, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, USoundWave*& OutMergedAudio, const FString& CustomFileName = TEXT(""), bool bForceStereo = false);
    
    // TTS音频处理：从内存数据添加静音并合并多个音频（不保存资产，只返回音频数据）
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ProcessMultipleAudioWithSilenceFromMemoryNoSave(const TArray<FAudioDataWrapper>& AudioDataArray, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, TArray<uint8>& OutMergedAudioData, int32& OutSampleRate, int32& OutNumChannels);

    // TTS音频处理：从文件添加静音并合并多个音频
    UFUNCTION(BlueprintCallable, Category = "AudioAssetOperation")
    static bool ProcessMultipleAudioWithSilenceFromFiles(const TArray<FString>& FilePaths, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, USoundWave*& OutMergedAudio, const FString& CustomFileName = TEXT(""), bool bForceStereo = false);


  private:
    // 递归获取所有音频轨道信息
    static void GetAllAudioTracksRecursive(UMovieScene* MovieScene, TArray<FAudioTrackInfo>& OutTrackInfos, const FString& ParentPath = TEXT(""), int32 CurrentLevel = 0);
    
    // 处理文件夹中的轨道
    static void ProcessFolderTracks(UMovieSceneFolder* Folder, UMovieScene* MovieScene, TArray<FAudioTrackInfo>& OutTrackInfos, const FString& ParentPath, int32 CurrentLevel);
    // 公开的分组查询（供外部/蓝图）
    

    // 获取音频片段
    static TArray<FAudioSegment> GetAudioSegmentsFromTrack(ULevelSequence* LevelSequence, const FString& SelectedTrack);

    // 获取Camera Cut Track的时间范围（内部使用）
    static TArray<FCameraCutSegment> GetCameraCutSegmentsInternal(ULevelSequence* LevelSequence);

    // 合并音频片段到PCM数据（与Camera Cut对齐）
    // UserTargetSampleRate: 0=自动上采样到最高采样率, >0=使用指定采样率
    static bool MergeAudioSegmentsToPCM(const TArray<FAudioSegment>& Segments, const TArray<FCameraCutSegment>& CameraCutSegments, TArray<uint8>& OutAudioData, uint32& OutSampleRate, uint16& OutNumChannels, const FFrameRate& TickResolution, int32 UserTargetSampleRate);

    // 音频格式转换函数
    static bool ConvertAudioFormat(const TArray<uint8>& InputData, uint32 InputSampleRate, uint16 InputChannels,
                                  uint32 OutputSampleRate, uint16 OutputChannels, TArray<uint8>& OutputData);

    // 声道数转换
    static bool ConvertChannels(const TArray<uint8>& InputData, uint16 InputChannels, uint16 OutputChannels, TArray<uint8>& OutputData);

    // 采样率转换
    static bool ConvertSampleRate(const TArray<uint8>& InputData, uint32 InputSampleRate, uint32 OutputSampleRate, uint16 Channels, TArray<uint8>& OutputData);

    // Lanczos重采样（高质量，适用于降采样）
    static bool LanczosResample(int16* InputPCM, int32 InputSamplesPerChannel, 
                               int16* OutputPCM, int32 OutputSamplesPerChannel,
                               uint32 InputSampleRate, uint32 OutputSampleRate, uint16 Channels);

    // 三次样条插值重采样（平衡质量和性能，适用于升采样）
    static bool CubicSplineResample(int16* InputPCM, int32 InputSamplesPerChannel,
                                   int16* OutputPCM, int32 OutputSamplesPerChannel,
                                   uint32 InputSampleRate, uint32 OutputSampleRate, uint16 Channels);

    // Lanczos内核函数
    static float LanczosKernel(float x, int32 a);

    // 使用RuntimeAudioImporter进行高质量音频重采样
    static bool RuntimeAudioResample(const TArray<uint8>& InputData, uint32 InputSampleRate, uint16 InputChannels,
                                    uint32 OutputSampleRate, uint16 OutputChannels, TArray<uint8>& OutputData);

    // 工具：针对一个音频轨道，按 Row 拆分并填充条目
    static void AddAudioTrackEntries(UMovieSceneAudioTrack* AudioTrack,
                                     UMovieScene* MovieScene,
                                     const FString& ParentPath,
                                     int32 CurrentLevel,
                                     TArray<FAudioTrackInfo>& OutTrackInfos,
                                     const FString& DisplayFolderName = TEXT(""),
                                     const FString& DisplayActorName = TEXT(""),
                                     int32 Ordinal = -1);

    // 工具：通过对象绑定 Guid 获取其显示名称
    static FString GetBindingNameByGuid(UMovieScene* MovieScene, const FGuid& ObjectBinding);

    // 重新编号轨道，确保编号顺序与显示顺序一致
    static void RenumberTracksByDisplayOrder(TArray<FAudioTrackInfo>& TrackInfos);

    // 获取Row的自定义名称
    static FString GetRowDisplayName(UMovieSceneAudioTrack* AudioTrack, int32 RowIndex);

    // 音频混音函数（使用交叉调制算法，支持音量控制）
    static int16 MixAudioSamples(int16 Sample1, int16 Sample2, float Volume1 = 1.0f, float Volume2 = 1.0f);
}; 