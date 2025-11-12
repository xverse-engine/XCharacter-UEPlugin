// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "XSequencerStructs.h"
#include "ProtocolStruct.h"

#include "MultiModalService.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_EightParams(FOnMultiModalDataProcessed,
                                               FString, CharacterName,
                                               bool, bGenAnimSuccess,
                                               FString, AnimPath,
                                               bool, bGenAudioSuccess,
                                               FString, AudioPath,
                                               float, AudioTime,
                                               int32, CurrentIndex,
                                               int32, TotalIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnServiceDisconnect, bool, bClean);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnA2FOneChunkOver, int32, CurrentChunk, int32, TotalChunk);

/**
 * 
 */
UCLASS()
class XSEQUENCER_API UMultiModalService : public UObject
{
	GENERATED_BODY()

public:
	FOnMultiModalDataProcessed OnMultiModalDataProcessed;
	FOnServiceDisconnect OnServiceDisconnect;
	FOnA2FOneChunkOver OnA2FOneChunkOver;

	/**
    * 发起 TTS （文字转音频+口型）的服务
    * @param NewTTSAddress						TTS 服务地址
    * @param bGenerateAudioOnly					是否只生成音频
    * @param CurrentSentence					当前句子，可以忽略，填为 0
    * @param TotalSentenceNumber				总句子数，可以忽略，填为 1
    * @param CharacterName						角色名字，会存在生成的动画文件名中作为识别
    * @param SpeakContent						所发文字
    * @param CharacterSetup						角色设置，包含角色的模型、音色、均值和方差控制等
    * @param GlobalSettings						音量、是否起始和结束、平滑帧数
    * @return 生成的服务指针
    */
	static UMultiModalService* MultiModalTextToSoundService(
		const FString& NewTTSAddress,
		const bool bGenerateAudioOnly,
		int32 CurrentSentence,
		int32 TotalSentenceNumber,
		const FString& SpeakContent,
		const FString& CharacterName,
		const FCharacterSetupData& CharacterSetup,
		const FTTSGlobalSettings& GlobalSettings
	);

	/**
	 * 读取 Iphone 面捕数据
	 * @param CharacterName				角色名
	 * @param ExpressionFrames			读取的表情序列，在蓝图空间中读取 CSV 获得
	 * @param CharacterSetup			角色设置，包含角色的模型、音色、均值和方差控制等
	 * @param GlobalSettings			音量、是否起始和结束、平滑帧数
	 * @return 生成的服务指针
	 */
	static UMultiModalService* CSVService(
		const FString& CharacterName,
		const TArray<TArray<float>>& ExpressionFrames,
		const FCharacterSetupData& CharacterSetup,
		const FTTSGlobalSettings& GlobalSettings
	);

	/**
	 * 调用我们的音频转口型服务
	 * @param NewS2FAddress					音频转口型服务地址
	 * @param InputSoundWave				输入的音频
	 * @param CharacterName					角色名
	 * @param CharacterSetup				角色设置，包含角色的模型等
	 * @param GlobalSettings				音量、是否起始和结束、平滑帧数
	 * @param ExtraExpressionFrames			是否输入额外的 Iphone 面部数据
	 * @return 生成的服务指针
	 */
	static UMultiModalService* MultiModalSoundToFaceWithCSVService(
		const FString& NewS2FAddress,
		const TObjectPtr<USoundWave>& InputSoundWave,
		const FString& CharacterName,
		const FCharacterSetupData& CharacterSetup,
		const FTTSGlobalSettings& GlobalSettings,
		const TArray<TArray<float>>& ExtraExpressionFrames
	);

	/**
 	 * 根据插件 Resource 目录下储存的表情系数序列 Json 文件中的数据，生成对应角色的表情动画
 	 * @param CharacterName						角色名字，会存在生成的动画文件名中作为识别
 	 * @param MeshAsset							角色模型引用
 	 * @param ExpressionName					表情文件名字，需要查看插件 Resource 目录下储存的表情系数文件，传入的文件名必须在其中
 	 * @param ExpressionExtent					控制表情程度
 	 * @return 生成的表情动画的路径
 	 */
	UFUNCTION(BlueprintCallable, Category = "Default")
	static FString GenerateExpressionAnim(const FString& CharacterName,
	                                      USkeletalMesh* MeshAsset,
	                                      const FString& ExpressionName,
	                                      float ExpressionExtent);

	void StartCSVReading();

private:
	void ConnectToServer(const EModalType Type);

	// ----------------------------------------- Websocket -----------------------------------------
	FWebSocketsModule* WebSocketModule{&FWebSocketsModule::Get()};
	TSharedPtr<IWebSocket> WebSocket;
	FString TextToSoundAddress;
	FString SoundToFaceAddress;

	// ----------------------------------------- Callbacks -----------------------------------------
	void OnWebSocketConnectionSuccess();
	void OnWebSocketConnectionError(const FString& ErrorMsg) const;
	void OnWebSocketDisconnectionSuccess(int32 StatusCode, const FString& Reason, bool bIsClean);
	void OnWebSocketMessageReceived(const FString& Message);
	void OnWebSocketMessageSent(const FString& Message);

	// ----------------------------------------- Universal Data -----------------------------------------
	EModalType ModalType{EModalType::TextToSound};
	FString CharacterName{"Vox"};
	FCharacterSetupData CharacterSetup{};
	FTTSGlobalSettings GlobalSettings{};

	bool bIsGenerating{false};
	TArray<TArray<float>> ExpressionFrames{};
	float AudioLength{0.0f};

	// ----------------------------------------- TTS Data -----------------------------------------
	bool bGenerateAnimation{true};
	TArray<uint8> AudioBuffer{};
	uint32 AudioSampleRate{44100};

	// ----------------------------------------- SoundToFace Data -----------------------------------------
	FSoundToFaceInitJson Request{};
	TArray<FString> Chunks;
	int32 ChunkAmount{0};
	int32 CurrentChunk{0};
	FString AudioName{};

	float OriginAnimationFPS{25.f};

	TArray<TArray<float>> ExtraExpressionFrames{};

	FString AudioBase64{};
	bool AudioToFaceFirstFrame = false;

	// ----------------------------------------- Internal Functions -----------------------------------------

	void ConnectToService();
	void StreamSendAudio();

	/**
     * 开始生成音频以及动画文件的内部函数
     * @param bGenerateAudioFile
     * @param bGenerateAnimFile 是否生成动画文件，如果设定为 False 则不生成动画文件
     * @param bGenerateSkelAnim
     * @param AnimDataFPS
     */
	void StartGeneratingAudioAndAnimFile(const bool bGenerateAudioFile,
	                                     const bool bGenerateAnimFile,
	                                     const bool bGenerateSkelAnim,
	                                     int32 AnimDataFPS = 25);

	bool GenerateAudioFile(const FString& AudioBaseName, float& OutAudioTime) const;
	bool GenerateAnimationFile(
		const FString& AnimBaseName,
		bool bSkeletalAnim, int32 AnimDataFPS);

	void PreprocessExpressionData();
	void PostProcessExpressionData();

	/**
	 * 由于 TTS 返回表情帧数为 25 FPS，而表情输出动画帧数为 30 FPS，因此需要对表情帧进行重采样
	 * @param CurrentFrame			当前帧数
	 * @param TargetFrameLength		目标总帧数
	 * @param OriginFPS
	 */
	TArray<float> GetResampledExpressionFrame(int32 CurrentFrame, int32 TargetFrameLength, int32 OriginFPS);

	/**
     * 给角色添加眨眼动画，通过修改 Expression Frames 中眨眼相关曲线数据实现
     */
	void AddBlinkForCharacter();

	// ----------------------------------------- UtilityFunctions -----------------------------------------
	static FTransform CalcBoneCurrentFrame(const TArray<FTransform>& ArkitBoneTransforms,
	                                       const TArray<float>& CurrentExpressionFrame,
	                                       const FTransform& OriginTransform);

	static FString GetAndRemoveSubstring(FString& OriginalString, int32 Length);
	static TArray<float> Base64ToFloatArray(const FString& Base64String);
	static FString ConvertPCMBufferToBase64String(const TArray<uint8>& PcmBuffer);

	static TArray<uint8> ConvertPCMBufferToWavBuffer(const TArray<uint8>& PcmBuffer, uint32 AudioSampleRate);
	static TArray<uint8> FStringToBytes(const FString& String);
	static TArray<uint8> Uint32ToBytes(const uint32 Value, bool UseLittleEndian = true);
	static TArray<uint8> Uint16ToBytes(const uint16 Value, bool UseLittleEndian = true);
};
