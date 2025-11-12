// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TTSSetting.h"
#include "XVCPluginSettings.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "TTSServer.generated.h"

// 前向声明
class UTTSSetting;

// TTS单段合成结果
USTRUCT()
struct FTTSSegmentResult
{
    GENERATED_BODY()

    UPROPERTY()
    int32 SegmentIndex = 0;

    // 第几次合成（从0开始）
    UPROPERTY()
    int32 ReplicateIndex = 0;

    UPROPERTY()
    FString Text;

    // 生成的SoundWave资产路径（/Game/...）- 仅在保存后有效
    UPROPERTY()
    FString AssetPath;

    // 音频数据（用于预览播放，不保存到磁盘）
    UPROPERTY()
    TArray<uint8> AudioData;

    // 是否已保存为资产
    UPROPERTY()
    bool bIsSavedAsAsset = false;

    // 段前静音（秒）
    UPROPERTY()
    float PreSilence = 0.0f;

    // 段后静音（秒）
    UPROPERTY()
    float PostSilence = 0.0f;
};

// TTS请求参数结构体
USTRUCT()
struct FTTSRequestParams
{
    GENERATED_BODY()

    // TTS设置
    UPROPERTY()
    UTTSSetting* LocalSettings = nullptr;

    // 插件设置
    UPROPERTY()
    const UXVCPluginSettings* Settings = nullptr;

    // 段索引
    UPROPERTY()
    int32 SegmentIndex = 0;

    // 资产路径
    UPROPERTY()
    FString AssetPath;

    // WAV文件目录
    UPROPERTY()
    FString WavDir;

    // 完成计数器
    TSharedPtr<int32> CompletedCount;

    // 总计数
    TSharedPtr<int32> TotalCount;

    // 完成的资产路径数组
    TSharedPtr<TArray<FString>> CompletedAssetPaths;

    // 收集的段结果（用于预览窗口）
    TSharedPtr<TArray<FTTSSegmentResult>> SegmentResults;

    // 第几次合成（从0开始）
    int32 ReplicateIndex = 0;

    // 批次信息（使用原始指针，避免UPROPERTY限制）
    int32* CurrentBatchIndex = nullptr;
    int32* BatchSize = nullptr;
    TArray<FString>* AllTextSegments = nullptr;
    TArray<UTTSSetting*>* AllTempSettings = nullptr;
    UTTSSetting* OriginalTTSSettings = nullptr;

    // 静音信息（段前静音和段后静音，单位：秒）
    float PreSilence = 0.0f;
    float PostSilence = 0.0f;

    // 重试计数（用于429错误重试）
    int32 RetryCount = 0;

    FTTSRequestParams() = default;

    FTTSRequestParams(UTTSSetting* InLocalSettings, 
                     const UXVCPluginSettings* InSettings, 
                     int32 InSegmentIndex, 
                     const FString& InAssetPath, 
                     const FString& InWavDir, 
                     TSharedPtr<int32> InCompletedCount, 
                     TSharedPtr<int32> InTotalCount, 
                     TSharedPtr<TArray<FString>> InCompletedAssetPaths,
                     TSharedPtr<TArray<FTTSSegmentResult>> InSegmentResults,
                     int32 InReplicateIndex)
        : LocalSettings(InLocalSettings)
        , Settings(InSettings)
        , SegmentIndex(InSegmentIndex)
        , AssetPath(InAssetPath)
        , WavDir(InWavDir)
        , CompletedCount(InCompletedCount)
        , TotalCount(InTotalCount)
        , CompletedAssetPaths(InCompletedAssetPaths)
        , SegmentResults(InSegmentResults)
        , ReplicateIndex(InReplicateIndex)
    {
    }

    // 带批次信息的构造函数
    FTTSRequestParams(UTTSSetting* InLocalSettings, 
                     const UXVCPluginSettings* InSettings, 
                     int32 InSegmentIndex, 
                     const FString& InAssetPath, 
                     const FString& InWavDir, 
                     TSharedPtr<int32> InCompletedCount, 
                     TSharedPtr<int32> InTotalCount, 
                     TSharedPtr<TArray<FString>> InCompletedAssetPaths,
                     TSharedPtr<TArray<FTTSSegmentResult>> InSegmentResults,
                     int32 InReplicateIndex,
                     int32* InCurrentBatchIndex,
                     int32* InBatchSize,
                     TArray<FString>* InAllTextSegments,
                     TArray<UTTSSetting*>* InAllTempSettings,
                     UTTSSetting* InOriginalTTSSettings,
                     float InPreSilence = 0.0f,
                     float InPostSilence = 0.0f)
        : LocalSettings(InLocalSettings)
        , Settings(InSettings)
        , SegmentIndex(InSegmentIndex)
        , AssetPath(InAssetPath)
        , WavDir(InWavDir)
        , CompletedCount(InCompletedCount)
        , TotalCount(InTotalCount)
        , CompletedAssetPaths(InCompletedAssetPaths)
        , SegmentResults(InSegmentResults)
        , ReplicateIndex(InReplicateIndex)
        , CurrentBatchIndex(InCurrentBatchIndex)
        , BatchSize(InBatchSize)
        , AllTextSegments(InAllTextSegments)
        , AllTempSettings(InAllTempSettings)
        , OriginalTTSSettings(InOriginalTTSSettings)
        , PreSilence(InPreSilence)
        , PostSilence(InPostSilence)
    {
    }
};

UCLASS()
class CHARACTERGEN_API UTTSServer : public UObject
{
    GENERATED_BODY()
public:
    // 调用TTS服务
    static void CallTTSServer(UTTSSetting* TTSSettings);
    // 调用TTS服务（分批处理）
    static void CallTTSServerBatched(UTTSSetting* TTSSettings, int32 BatchSize = 2);
    // 重置TTS批次状态
    static void ResetTTSBatchState();
    // 获取说话人列表
    static void FetchSpeakerList(TFunction<void(const TArray<TSharedPtr<FString>>&)> OnComplete);
    // 发送TTS请求
    static void SendTTSRequest(const FTTSRequestParams& Params);
    // 发送批次请求
    static void SendBatchRequests(int32* CurrentBatchIndex, 
                                 int32* BatchSize, 
                                 TArray<FString>* AllTextSegments,
                                 TArray<UTTSSetting*>* AllTempSettings,
                                 UTTSSetting* OriginalTTSSettings,
                                 const UXVCPluginSettings* Settings,
                                 const FString& AssetPath,
                                 const FString& WavDir,
                                 TSharedPtr<int32> CompletedCount,
                                 TSharedPtr<int32> TotalCount,
                                 TSharedPtr<TArray<FString>> CompletedAssetPaths,
                                 TSharedPtr<TArray<FTTSSegmentResult>> SegmentResults,
                                 int32 ReplicatesPerSegment);
    // 获取已缓存的说话人显示字符串（若未加载则返回空）
    static TArray<FString> GetCachedSpeakerOptions();
    
    // 验证豆包TTS API配置参数
    static bool ValidateDoubaoTTSConfig(const UXVCPluginSettings* Settings, const FString& Context = TEXT(""));

    // 全部TTS完成事件（仅编辑器使用）
    DECLARE_MULTICAST_DELEGATE_TwoParams(FTTSAllCompleted, const TArray<FTTSSegmentResult>&, UTTSSetting*);
    static FTTSAllCompleted& OnTTSAllCompleted();
    
    // TTS进度更新事件
    DECLARE_MULTICAST_DELEGATE_TwoParams(FTTSProgressUpdate, int32, int32);
    static FTTSProgressUpdate& OnTTSProgressUpdate();
    
    // 音频增强完成事件
    DECLARE_MULTICAST_DELEGATE_ThreeParams(FEnhanceAudioCompleted, bool, const FString&, const FString&);
    static FEnhanceAudioCompleted& OnEnhanceAudioCompleted();
    
    // 音频增强接口
    static void EnhanceAudio(const FString& AudioAssetPath, const FString& EnhancedAssetPath);
    
    // 从内存数据直接进行音频增强
    static void EnhanceAudioFromMemory(const TArray<uint8>& AudioData, const FString& EnhancedAssetPath, 
        TFunction<void(bool bSuccess, const TArray<uint8>& EnhancedAudioData)> OnComplete);
}; 