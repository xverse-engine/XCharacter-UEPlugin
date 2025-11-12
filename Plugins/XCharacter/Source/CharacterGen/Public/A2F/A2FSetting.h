#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"
#include "Sound/SoundWave.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "LevelSequence.h"
#include "A2FSetting.generated.h"

// 情感类型枚举
UENUM(BlueprintType)
enum class EA2FEmotionType : uint8
{
    Neutral        UMETA(DisplayName = "Neutral"),
    Happy          UMETA(DisplayName = "Happy"),
    Sad            UMETA(DisplayName = "Sad"),
    Disgust        UMETA(DisplayName = "Disgust"),
    Anger          UMETA(DisplayName = "Anger"),
    Surprise       UMETA(DisplayName = "Surprise"),
    Fear           UMETA(DisplayName = "Fear")
};

UCLASS()
class CHARACTERGEN_API UA2FSetting : public UObject
{
    GENERATED_BODY()
public:
    // 步骤1：关卡序列选择
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤1: 请选择要操作的关卡序列", meta = (AllowedClasses = "LevelSequence", DisplayName = "关卡序列"))
    TSoftObjectPtr<ULevelSequence> SelectedLevelSequence = nullptr;

    // 步骤2：A2F设置
    // 音频资产选择栏
    UPROPERTY(EditAnywhere, Category = "步骤2: A2F设置", meta = (AllowedClasses = "SoundWave", DisplayName = "输入音频"))
    TSoftObjectPtr<USoundWave> InputAudio = nullptr;

    // Mesh模型选择栏
    UPROPERTY(EditAnywhere, Category = "步骤2: A2F设置", meta = (AllowedClasses = "SkeletalMesh", DisplayName = "输入Mesh模型"))
    TSoftObjectPtr<USkeletalMesh> InputMeshModel = nullptr;

    // 动画资产选择栏
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "骨骼动画驱动"))
    bool bSkeletalAnim = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "输入表情基骨骼动画", EditCondition = "bSkeletalAnim"))
    TSoftObjectPtr<UAnimSequence> InputSkeleAnim;

    // 眨眼控制参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "启用眨眼控制"))
    bool addBlink = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "眨眼间隔最低值", EditCondition = "addBlink", ClampMin = "0.1", ClampMax = "10.0"))
    float BlinkRandomBottom = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "眨眼间隔最高值", EditCondition = "addBlink", ClampMin = "0.1", ClampMax = "10.0"))
    float BlinkRandomTop = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "单次眨眼时长", EditCondition = "addBlink", ClampMin = "0.05", ClampMax = "1.0"))
    float BlinkTime = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "眨眼幅度", EditCondition = "addBlink", ClampMin = "0.0", ClampMax = "1.0"))
    float BlinkAmplitude = 1.0f;

    // 张嘴幅度控制参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "启用张嘴幅度控制"))
    bool addMouthScale = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "张嘴幅度", EditCondition = "addMouthScale", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float MouthAmplitudeScale = 0.8f;

    // 情感控制参数
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "情感类型"))
    EA2FEmotionType EmotionType = EA2FEmotionType::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "步骤2: A2F设置", Meta = (DisplayName = "情感强度", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "EmotionType != EA2FEmotionType::Neutral"))
    float EmotionIntensity = 1.0f;

    /** Accessor and initializer*/
    static UA2FSetting* Get();
    
public:
    virtual void Serialize(class FArchive& Archive) override;
}; 