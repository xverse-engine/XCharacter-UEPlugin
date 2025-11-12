// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TTS/TTSServer.h"
#include "TTS/TTSSetting.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "MotionGen/MotionGenService.h"

#include "TTSPreviewManager.generated.h"

// 音频片段信息结构
struct FAudioSegmentInfo
{
    float StartTime;        // 开始时间（秒）
    float EndTime;          // 结束时间（秒）
    FString Label;          // 标签文本
    FLinearColor Color;     // 标记线颜色
    bool bIsSilence;        // 是否为静音区域
    
    FAudioSegmentInfo() : StartTime(0.0f), EndTime(0.0f), bIsSilence(false) {}
    FAudioSegmentInfo(float InStart, float InEnd, const FString& InLabel, const FLinearColor& InColor, bool InIsSilence = false)
        : StartTime(InStart), EndTime(InEnd), Label(InLabel), Color(InColor), bIsSilence(InIsSilence) {}
};

/**
 * TTS预览页面管理器
 * 负责管理TTS音频预览、编辑、重新合成、音质升级等功能
 */
UCLASS()
class CHARACTERGENEDITOR_API UTTSPreviewManager : public UObject
{
    GENERATED_BODY()

public:
    UTTSPreviewManager();
    ~UTTSPreviewManager();

    // 获取单例实例
    static UTTSPreviewManager* Get();

    // 核心预览功能
    void ShowTTSPreviewWindow(const TArray<FTTSSegmentResult>& Results, UTTSSetting* CurrentTTSSettings = nullptr, const TMap<int32, float>& PreSilenceValues = TMap<int32, float>(), const TMap<int32, float>& PostSilenceValues = TMap<int32, float>());
    void OnTTSAllCompletedHandler(const TArray<FTTSSegmentResult>& Results, UTTSSetting* TTSSettings);
    void PreviewAllTTSAudio(const TArray<FTTSSegmentResult>& Results);
    void UpdatePreviewUI();
    
    // 静音信息管理方法
    void ParseAndStoreSilenceInfo(const TArray<FString>& OriginalTexts);
    void SetSilenceInfo(const TMap<int32, float>& PreSilenceValues, const TMap<int32, float>& PostSilenceValues);

    // 音频处理相关
    void ProcessAndSaveSingleAudio(const FTTSSegmentResult& Result, const FString& Text, float PreSilence, float PostSilence, bool bStereo = false);
    void ProcessAndSaveMergedAudio(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, const FString& CustomFileName = TEXT(""), bool bStereo = false);
    void ProcessAndSaveMergedAudioWithUpgrade(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, const FString& CustomFileName = TEXT(""), bool bStereo = false);
    void SaveSelectedTTSAudio(const TArray<FTTSSegmentResult>& Results, const FString& CustomFileName = TEXT(""), bool bStereo = false);
    
    // 新的保存对话框相关方法
    void ShowSaveDialog();
    void OnSaveDialogConfirmed(const FString& FileName, bool bUpgradeQuality, bool bStereo);
    void OnSaveDialogCancelled();
    bool CheckFileExistsAndConfirmOverwrite(const FString& FileName, TSharedPtr<SWindow> DialogWindow, bool bUpgrade, bool bStereo);

    // 重新合成功能
    void RegenerateTTSForSegment(int32 SegmentIndex, const FString& Text, const FString& SpeakerName, const FString& EmotionName, float Speed, float Pitch);
    void OnEnhanceAudioCompleted(bool bSuccess, const FString& OriginalAssetPath, const FString& EnhancedAssetPath);

    // 状态管理
    void SavePreviewWindowState();
    void MergePreviewResults(const TArray<FTTSSegmentResult>& NewResults);
    void RestorePreviewWindowState();
    void ClearAllTTSPreviewState();

    // 重试机制
    void ValidateTTSResults(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& OriginalTexts);
    void RetryFailedTTS(const TArray<FString>& OriginalTexts, UTTSSetting* TTSSettings);
    void OnRetryTTSCompleted(const TArray<FTTSSegmentResult>& Results, UTTSSetting* TTSSettings);
    bool HasFailedSegments() const;
    void ClearRetryState();

    // 工具函数
    void RecalculateSelectedAudioDuration();
    void PrefillAudioSampleRateCache();
    void CreateWaveformSegments(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, TArray<FAudioSegmentInfo>& OutSegments);
    bool IsTTSPreviewWindowOpen() const;

    // 回调
    FSimpleDelegate OnTTSCompleted;

private:
    // 窗口和UI组件
    TSharedPtr<SWindow> CurrentTTSPreviewWindow;           // 当前TTS预览窗口
    TSharedPtr<SVerticalBox> CurrentPreviewVBox;          // 当前预览窗口的垂直布局
    TSharedPtr<SScrollBox> CurrentPreviewScrollBox;       // 当前预览窗口的滚动框
    TSharedPtr<SVerticalBox> PreviewArea;                 // 合并预览区域（波形与进度）

    // 音频数据和状态
    TArray<FTTSSegmentResult> CurrentPreviewResults;      // 当前预览窗口的音频结果
    TArray<uint8> PreviewPCMData;                         // 预览PCM数据（16-bit）
    uint32 PreviewSampleRate = 0;                         // 预览采样率
    uint16 PreviewNumChannels = 0;                        // 预览声道数
    TWeakObjectPtr<USoundWave> PreviewSoundWave;          // 预览音频对象
    TWeakObjectPtr<UAudioComponent> PreviewAudioComponent; // 预览音频组件
    TSharedPtr<STextBlock> PreviewInfoText;               // 预览信息文本
    TSharedPtr<float> PreviewPlayPercent;                 // 预览播放进度
    bool bPreviewPaused = false;                          // 预览是否暂停

    // 用户选择和配置
    TMap<int32, int32> SelectedReplicateBySegment;        // 每个段选择的复本索引
    TMap<int32, TSharedPtr<FString>> EditedTextBySegment; // 每段可编辑的文本内容
    TMap<int32, TSharedPtr<FString>> SegmentSpeakerSelections; // 每个段落的音色选择
    TMap<int32, float> SegmentSpeedValues;                // 每个段落的语速值
    TMap<int32, float> SegmentPitchValues;                // 每个段落的音高值
    TMap<int32, float> SegmentPreSilenceValues;           // 每个段落的段前静音
    TMap<int32, float> SegmentPostSilenceValues;          // 每个段落的段后静音

    // 重新合成和升级状态
    TMap<int32, bool> RegeneratingSegments;               // 记录哪些段落正在重新合成
    bool bIsUpdatingFromRegeneration = false;             // 是否来自重新合成/升级的更新
    bool bGlobalUpgradeEnabled = false;                   // 是否启用全局音质升级
    bool bIsUpgrading = false;                            // 是否正在升级音质
    TSharedPtr<SProgressBar> UpgradeProgressBar;          // 升级进度条
    TSharedPtr<STextBlock> UpgradeProgressText;           // 升级进度文本

    // 时长计算
    float TotalSelectedAudioDuration = 0.0f;              // 选中音频总时长
    float TotalSelectedAudioDurationWithSilence = 0.0f;   // 选中音频静音后总时长

    // 重试机制
    TMap<int32, TArray<int32>> FailedSegments;            // 记录失败的段落和复本索引
    TMap<int32, bool> RetryingSegments;                   // 记录哪些段落正在重试
    int32 TotalRetryAttempts = 0;                         // 总重试次数
    int32 MaxRetryAttempts = 3;                           // 最大重试次数

    // 缓存和优化
    TMap<FIntPoint, uint32> CachedAudioSampleRates;       // 缓存音频采样率
    TMap<FIntPoint, uint32> CachedAudioDataHashes;        // 缓存音频数据哈希值
    TArray<FAudioSegmentInfo> CachedPreviewWaveformSegments; // 缓存的预览波形片段信息

    // 音频播放管理
    TMap<int32, TWeakObjectPtr<UAudioComponent>> PlayingComponents; // 播放中音频组件
    
    // 交互式波形控件相关
    int32 SelectedWaveformSegmentIndex = -1;    // 当前选中的波形片段索引
    int32 HoveredWaveformSegmentIndex = -1;     // 当前悬停的波形片段索引
    
    // 片段播放进度跟踪
    int32 CurrentPlayingSegmentIndex = -1;      // 当前正在播放的片段索引
    float CurrentSegmentStartTime = 0.0f;       // 当前播放片段的开始时间
    float CurrentSegmentDuration = 0.0f;        // 当前播放片段的持续时间
    bool bIsPlayingSegment = false;             // 是否正在播放片段
    
    // 波形控件缩放状态存储
    float SavedZoomLevel = 1.0f;                // 保存的缩放级别
    float SavedPanOffset = 0.0f;                // 保存的平移偏移
    bool bHasSavedZoomState = false;            // 是否有保存的缩放状态
    
    // 交互式波形控件事件处理函数
    void OnWaveformSegmentClicked(int32 SegmentIndex);
    void OnWaveformSegmentHovered(int32 SegmentIndex, bool bIsHovered);
    void PlayWaveformSegment(int32 SegmentIndex);
    
    // 缩放状态管理函数
    void SaveWaveformZoomState(float ZoomLevel, float PanOffset);
    void RestoreWaveformZoomState();
};

