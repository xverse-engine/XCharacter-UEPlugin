// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "A2FSetting.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "A2FService.generated.h"

// 常量定义
constexpr int32 ARKIT_BLENDSHAPE_COUNT = 51;
constexpr int32 DEFAULT_ANIMATION_FPS = 30;
constexpr int32 MAX_FRAME_COUNT = 1000000; // 最大帧数限制

// ---- USTRUCT定义 ----

// 眨眼参数结构体
USTRUCT(BlueprintType)
struct CHARACTERGEN_API FBlinkParams
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "眨眼参数")
    bool add_blink = true;

    UPROPERTY(BlueprintReadWrite, Category = "眨眼参数")
    float blink_random_bottom = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "眨眼参数")
    float blink_random_top = 7.0f;

    UPROPERTY(BlueprintReadWrite, Category = "眨眼参数")
    float blink_time = 0.3f;

    UPROPERTY(BlueprintReadWrite, Category = "眨眼参数")
    float blink_amplitude = 1.0f;
};

// 张嘴参数结构体
USTRUCT(BlueprintType)
struct CHARACTERGEN_API FMouthParams
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "张嘴参数")
    bool add_mouth_scale = false;

    UPROPERTY(BlueprintReadWrite, Category = "张嘴参数")
    float mouth_amplitude_scale = 1.0f;
};

USTRUCT(BlueprintType)
struct CHARACTERGEN_API FA2FRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    FString trace_id;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    FString audio;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    FBlinkParams blink_params;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    FMouthParams mouth_params;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    FString emotion;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    float intensity;
};

USTRUCT(BlueprintType)
struct CHARACTERGEN_API FA2FResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    FString trace_id;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    TArray<float> data;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    int32 code = 0;

    UPROPERTY(BlueprintReadWrite, Category = "HTTP")
    FString msg;
};

// ---- UCLASS ----
UCLASS()
class CHARACTERGEN_API UA2FService : public UObject
{
    GENERATED_BODY()

public:
    UA2FService();

    // 表情帧数据
    TArray<TArray<float>> ExpressionFrames;

    // 生成动画文件
    bool GenerateAnimationFile(UA2FSetting* A2FSettings,
                                const FString& FacialAnimationFolder,
                                const FString& AnimBaseName,
                                bool bSkeletalAnim, int32 AnimDataFPS);

    // A2F功能执行
    void ExecuteA2F(UA2FSetting* A2FSettings);

    // 获取重采样的表情帧数据
    TArray<float> GetResampledExpressionFrame(const int32 CurrentFrame, const int32 TargetFrameLength, int32 OriginFPS);

    // 音频转换工具函数
    static TArray<uint8> ConvertPCMBufferToWavBuffer(const TArray<uint8>& PcmBuffer, uint32 AudioSampleRate);
    static FString ConvertWavBufferToBase64String(const TArray<uint8>& WavBuffer);

private:
    // 工具函数
    static FTransform CalcBoneCurrentFrame(const TArray<FTransform>& ArkitBoneTransforms,
                                           const TArray<float>& CurrentExpressionFrame,
                                           const FTransform& OriginTransform);
    static TArray<float> Base64ToFloatArray(const FString& Base64String);
    static FString ConvertPCMBufferToBase64String(const TArray<uint8>& PcmBuffer);
    static TArray<uint8> FStringToBytes(const FString& String);
    static TArray<uint8> Uint32ToBytes(const uint32 Value, bool UseLittleEndian = true);
    static TArray<uint8> Uint16ToBytes(const uint16 Value, bool UseLittleEndian = true);
};
