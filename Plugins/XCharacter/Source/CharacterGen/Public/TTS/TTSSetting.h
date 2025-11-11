#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"
#include "Sound/SoundWave.h"
#include "TTSSetting.generated.h"

// 前向声明
class UTTSServer;

// TTS情感枚举（独立定义）
UENUM(BlueprintType)
enum class ETTS_Emotion : uint8
{
    Adaptive UMETA(DisplayName = "自适应"),
    Happy UMETA(DisplayName = "开心"),
    Sad UMETA(DisplayName = "伤心"),
    Angry UMETA(DisplayName = "愤怒"),
    Fearful UMETA(DisplayName = "恐惧"),
    Disgusted UMETA(DisplayName = "厌恶"),
    Surprised UMETA(DisplayName = "惊讶"),
    Neutral UMETA(DisplayName = "中性")
};

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

    /** TTS情感参数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS设置", meta = (GetOptions = "GetEmotionOptions", DisplayName = "情感"))
    FString EmotionName = TEXT("自适应");

    /** 自定义情感名称（当使用情感音频时） */
    UPROPERTY()
    FString CustomEmotionName;


    /** 情感音频 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS设置", meta = (AllowedClasses = "SoundWave", AllowedExtensions = ".wav,.mp3,.ogg,.flac", DisplayName = "选择情感参考音频 （3 - 10 s）"))
    TSoftObjectPtr<USoundWave> EmotionAudio = nullptr;
    
    // === 音频选择框 ===
    /** 音色音频 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTS设置", meta = (AllowedClasses = "SoundWave", AllowedExtensions = ".wav,.mp3,.ogg,.flac", DisplayName = "选择音色参考音频 （3 - 10 s）"))
    TSoftObjectPtr<USoundWave> VoiceAudio = nullptr;


    // === 音频字节流存储 ===
    /** 音色音频字节流 */
    UPROPERTY()
    TArray<uint8> VoiceAudioBytes;

    /** 情感音频字节流 */
    UPROPERTY()
    TArray<uint8> EmotionAudioBytes;

    /** 音色音频文件名（用于生成新音色名称） */
    UPROPERTY()
    FString VoiceAudioFileName;

    /** 情感音频文件名（用于生成新音色名称） */
    UPROPERTY()
    FString EmotionAudioFileName;

    /** 是否使用自定义音色音频 */
    UPROPERTY()
    bool bUseCustomVoiceAudio = false;

    /** 是否使用自定义情感音频 */
    UPROPERTY()
    bool bUseCustomEmotionAudio = false;
    
    // === 静态缓存机制 ===
    /** 缓存的音色数据 - 键为音色名称，值为音频字节流 */
    static TMap<FString, TArray<uint8>> CachedVoiceData;
    
    /** 缓存的情感数据 - 键为情感名称，值为音频字节流 */
    static TMap<FString, TArray<uint8>> CachedEmotionData;
    
    /** 清除所有缓存的音色和情感数据 */
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    static void ClearAllCachedData();
    

    
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
    /** 处理音色音频选择 */
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    void ProcessVoiceAudioSelection();

    /** 处理情感音频选择 */
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    void ProcessEmotionAudioSelection();

    /** 获取当前选中的音色数据（可能是字符串或字节流） */
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    bool GetCurrentSpeakerData(FString& OutSpeakerName, TArray<uint8>& OutAudioBytes);

    /** 获取当前选中的情感数据（可能是字符串或字节流） */
    UFUNCTION(BlueprintCallable, Category = "TTS设置")
    bool GetCurrentEmotionData(FString& OutEmotionName, TArray<uint8>& OutAudioBytes);

    /** 打印音频字节流信息到控制台 */
    void PrintAudioBytesInfo(const FString& AudioType, const TArray<uint8>& AudioBytes);

    /** 获取音频文件的原始数据 */
    static bool GetAudioFileData(USoundWave* SoundWave, TArray<uint8>& OutAudioData);
    
    /** 将PCM数据转换为WAV格式 */
    static bool ConvertPCMToWAV(const uint8* PCMData, int32 PCMDataSize, int32 SampleRate, int32 NumChannels, TArray<uint8>& OutWAVData);

public:
    virtual void Serialize(class FArchive& Archive) override;

    /** 获取音色下拉选项 */
    UFUNCTION()
    static TArray<FString> GetSpeakerOptions();

    /** 获取情感下拉选项 */
    UFUNCTION()
    static TArray<FString> GetEmotionOptions();
}; 