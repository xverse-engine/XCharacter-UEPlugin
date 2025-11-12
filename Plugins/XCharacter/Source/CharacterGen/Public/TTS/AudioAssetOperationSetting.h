#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"
#include "LevelSequence.h"
#include "Sound/SoundWave.h"
#include "TTS/AudioAssetOperation.h"
#include "AudioAssetOperationSetting.generated.h"

// 前向声明
class UAudioAssetOperation;

UCLASS()
class CHARACTERGEN_API UAudioAssetOperationSetting : public UObject
{
    GENERATED_BODY()

public:
    // === 关卡序列设置 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "音频操作设置", meta = (AllowedClasses = "LevelSequence", DisplayName = "关卡序列"))
    TSoftObjectPtr<ULevelSequence> SelectedLevelSequence = nullptr;

    // === 音频轨道设置 ===
    // 轨道选项（下拉菜单）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "音频操作设置", meta = (GetOptions = "GetTrackOptions", DisplayName = "轨道"))
    FString SelectedTrack;

    // === 音频处理设置 ===
    // 注意：音频处理逻辑已简化为自动上采样到最高采样率

    //用于存储可选轨道名
    UPROPERTY(Transient)
    TArray<FString> TrackOptions;

    // 存储所有音频轨道信息
    UPROPERTY(Transient)
    TArray<FAudioTrackInfo> TrackInfos;

    // 合并后的音频
    UPROPERTY(Transient)
    USoundWave* MergedAudio = nullptr;
    
    /** Accessor and initializer*/
    static UAudioAssetOperationSetting* Get();

    // === 音频操作方法 ===
    
    // 合并音频片段
    UFUNCTION(BlueprintCallable, Category = "音频操作设置")
    bool MergeAudioSegments();

    // 应用合并后的音频到轨道
    UFUNCTION(BlueprintCallable, Category = "音频操作设置")
    bool ApplyMergedAudioToTrack();

    // 监听LevelSequence保存事件并刷新轨道列表
    UFUNCTION(BlueprintCallable, Category = "音频操作设置")
    void ListenForLevelSequenceSave();

    // 刷新轨道选项
    void RefreshTrackOptions();

    // 获取轨道层次结构信息
    UFUNCTION(BlueprintCallable, Category = "音频操作设置")
    FString GetTrackHierarchyInfo() const;



public:
    virtual void Serialize(class FArchive& Archive) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    UFUNCTION()
    TArray<FString> GetTrackOptions() const;
}; 