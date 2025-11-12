#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"
#include "Sound/SoundWave.h"
#include "TTSSetting.generated.h"

// 前向声明
class UTTSServer;

UCLASS()
class CHARACTERGEN_API UTTSSetting : public UObject
{
    GENERATED_BODY()

public:
    // === TTS设置 ===
    /** TTS文本输入 */
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS设置", DisplayName = "合成文本")
    FString TTSInputText;

    /** 音色选项（下拉菜单） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS设置", meta = (GetOptions = "GetSpeakerOptions", DisplayName = "音色"))
    FString SpeakerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS设置", DisplayName = "语速", meta = (ClampMin = "0.8", ClampMax = "1.2"))
    float AudioSpeed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS设置", DisplayName = "语调", meta = (ClampMin = "0.8", ClampMax = "1.2"))
    float AudioPitch = 1.0f;


    

    
    /** Accessor and initializer*/
    static UTTSSetting* Get();

    // === TTS方法 ===
    
    // 合成TTS音频
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    bool SynthesizeTTSAudio();
    
    // 合成TTS音频（分批处理）
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    bool SynthesizeTTSAudioBatched(int32 BatchSize = 2);

    // === 音频处理方法 ===
    /** 获取当前选中的音色数据（用于兼容性） */
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    bool GetCurrentSpeakerData(FString& OutSpeakerName, TArray<uint8>& OutAudioBytes);

public:
    virtual void Serialize(class FArchive& Archive) override;

    /** 获取音色下拉选项 */
    UFUNCTION()
    static TArray<FString> GetSpeakerOptions();
}; 