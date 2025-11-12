// Fill out your copyright notice in the Description page of Project Settings.

#include "TTS/AudioAssetOperationSetting.h"
#include "TTS/AudioAssetOperation.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "Tools/GenTools.h"
#include "Math/UnrealMathUtility.h"
#include "Interfaces/IPluginManager.h"
#include "AssetToolsModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"



#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION <= 2)
#include "AssetRegistryModule.h"
#else
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#endif

// === 基础方法实现 ===

void UAudioAssetOperationSetting::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
}

UAudioAssetOperationSetting* UAudioAssetOperationSetting::Get()
{
    static UAudioAssetOperationSetting* DefaultSettings = nullptr;
    if (!DefaultSettings)
    {
        // This is a singleton, use default object
        DefaultSettings = DuplicateObject(GetMutableDefault<UAudioAssetOperationSetting>(), GetTransientPackage());
        DefaultSettings->AddToRoot();
    }

    return DefaultSettings;
}

TArray<FString> UAudioAssetOperationSetting::GetTrackOptions() const
{
    return TrackOptions;
}

FString UAudioAssetOperationSetting::GetTrackHierarchyInfo() const
{
    if (SelectedLevelSequence.IsValid())
    {
        // 生成简单的轨道层次结构信息
        TArray<FAudioTrackInfo> LocalTrackInfos = UAudioAssetOperation::GetAllAudioTracksInfo(SelectedLevelSequence.Get());
        
        FString HierarchyInfo = FString::Printf(TEXT("总轨道数: %d\n"), LocalTrackInfos.Num());
        
        for (int32 i = 0; i < LocalTrackInfos.Num(); ++i)
        {
            const FAudioTrackInfo& TrackInfo = LocalTrackInfos[i];
            HierarchyInfo += FString::Printf(TEXT("%d. %s\n"), i + 1, *TrackInfo.DisplayName);
        }
        
        return HierarchyInfo;
    }
    return TEXT("无法获取轨道层次结构信息：LevelSequence无效");
}

void UAudioAssetOperationSetting::RefreshTrackOptions()
{
    TrackOptions.Empty();
    TrackInfos.Empty();
    
    // 清除选中的轨道，因为原有轨道可能已被删除
    SelectedTrack.Empty();

    // 清理静态变量，确保每次刷新都从零开始计数
    UAudioAssetOperation::ClearStaticVariables();
    
    if (SelectedLevelSequence.IsValid())
    {
        // 获取所有音频轨道信息（仅来源于文件夹/对象绑定/根级别的规范命名，不混入视觉嵌套）
        TrackInfos = UAudioAssetOperation::GetAllAudioTracksInfo(SelectedLevelSequence.Get());
        
        // 生成轨道选项列表（DisplayName 已由收集阶段包含 Row 信息时添加）
        for (const FAudioTrackInfo& TrackInfo : TrackInfos)
        {
            TrackOptions.Add(TrackInfo.DisplayName);
        }
        
        UE_LOG(LogTemp, Log, TEXT("刷新轨道选项完成，找到 %d 个音频轨道（包括嵌套轨道）"), TrackInfos.Num());
        
        // 输出轨道信息
        for (int32 i = 0; i < TrackInfos.Num(); ++i)
        {
            const FAudioTrackInfo& TrackInfo = TrackInfos[i];
            UE_LOG(LogTemp, Log, TEXT("轨道 %d: %s"), i + 1, *TrackInfo.DisplayName);
        }
    }
}

void UAudioAssetOperationSetting::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAudioAssetOperationSetting, SelectedLevelSequence))
    {
        if (SelectedLevelSequence.IsValid())
        {
            RefreshTrackOptions();
        }
    }
    else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAudioAssetOperationSetting, SelectedTrack))
    {
        // 轨道选择变更时的处理逻辑
    }
}

// 合并音频片段
bool UAudioAssetOperationSetting::MergeAudioSegments()
{
    ULevelSequence* LevelSequence = SelectedLevelSequence.LoadSynchronous();
    if (!LevelSequence)
    {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence无效"));
        return false;
    }

    if (SelectedTrack.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("SelectedTrack为空"));
        return false;
    }

    // 调用UAudioAssetOperation的合并方法，使用自动上采样到最高采样率的逻辑
    USoundWave* MergedAudioResult = nullptr;
    bool bSuccess = UAudioAssetOperation::MergeAudioSegments(LevelSequence, SelectedTrack, MergedAudioResult, 0);
    
    if (bSuccess && MergedAudioResult)
    {
        MergedAudio = MergedAudioResult;
        UE_LOG(LogTemp, Log, TEXT("音频合并成功"));
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("音频合并失败"));
        return false;
    }
}

// 应用合并后的音频到轨道
bool UAudioAssetOperationSetting::ApplyMergedAudioToTrack()
{
    ULevelSequence* LevelSequence = SelectedLevelSequence.LoadSynchronous();
    if (!LevelSequence)
    {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence无效"));
        return false;
    }

    // 使用新的函数：创建新轨道而不是替换原有音频
    bool bSuccess = UAudioAssetOperation::CreateNewTrackWithMergedAudio(LevelSequence, SelectedTrack, MergedAudio);
    
    if (bSuccess)
    {
        // 应用成功后，开始监听保存事件
        ListenForLevelSequenceSave();
        
        // 立即刷新轨道列表
        RefreshTrackOptions();
        
        UE_LOG(LogTemp, Log, TEXT("音频应用成功，已创建新轨道并保留原有音频"));
    }
    
    return bSuccess;
}

// 监听LevelSequence保存事件并刷新轨道列表
void UAudioAssetOperationSetting::ListenForLevelSequenceSave()
{
    if (!SelectedLevelSequence.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("SelectedLevelSequence无效，无法监听保存事件"));
        return;
    }

    ULevelSequence* LevelSequence = SelectedLevelSequence.LoadSynchronous();
    if (!LevelSequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("无法加载LevelSequence，无法监听保存事件"));
        return;
    }

    UWorld* World = GEngine->GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("无法获取World，无法监听保存事件"));
        return;
    }

    // 设置定时器，定期检查LevelSequence是否被修改
    FTimerHandle SaveCheckTimer;
    World->GetTimerManager().SetTimer(
        SaveCheckTimer,
        [this, LevelSequence]()
        {
            // 检查LevelSequence是否被修改
            if (LevelSequence && LevelSequence->GetPackage()->IsDirty())
            {
                UE_LOG(LogTemp, Log, TEXT("检测到LevelSequence被修改，刷新轨道列表"));
                RefreshTrackOptions();
                FNotificationInfo Info(FText::FromString(TEXT("轨道列表已刷新")));
                Info.bUseLargeFont = false;
                Info.ExpireDuration = 2.0f;
                FSlateNotificationManager::Get().AddNotification(Info);
            }
        },
        1.0f,  // 每秒检查一次
        true   // 重复执行
    );
    UE_LOG(LogTemp, Log, TEXT("开始监听LevelSequence保存事件: %s"), *LevelSequence->GetName());
}

 