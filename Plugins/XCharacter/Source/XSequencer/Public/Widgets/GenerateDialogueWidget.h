// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "XSequencerStructs.h"
#include "EditorUtilityWidget.h"
#include "GenerateDialogueWidget.generated.h"

UCLASS()
class XSEQUENCER_API UGenerateDialogueWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Default")
	FString OpenFileDialogAndSelectFileName(bool bIsJson = false);

	/**
	 * 读取 Iphone Livelink Face 生成的面捕 CSV 数据
	 * @param InputCharacterSetup		角色设置
	 * @return 
	 */
	UFUNCTION(BlueprintCallable, Category = "Default")
	FString ReadCSVFacialCaptureData(const FCharacterSetupData& InputCharacterSetup);

	/**
 	 *  调用 AudioToFace 服务时用于暴露给蓝图的主函数，输入需要转换的音频，以及在外部写好的角色配置
 	 * @param InputSoundWave					蓝图中传入的 Json 文本
 	 * @param InputCharacterSetup			角色配置
 	 * @return 所创建的主 Sequence 路径
 	 */
	UFUNCTION(BlueprintCallable, Category = "Default")
	FString CallAudioToFaceServiceByInfo(USoundWave* InputSoundWave,
	                                     const FCharacterSetupData& InputCharacterSetup);

	/**
  	 *  调用 TTS （文字转音频+口型）的主函数，输入需要转换的音频，以及在外部写好的角色配置
  	 * @param InputText							蓝图中传入的需要生成的文本
  	 * @param InputCharacterSetup				角色配置
  	 * @param bGenerateAudioOnly				可以选择只生成音频不生成动画
  	 * @return 所创建的主 Sequence 路径
  	 */
	UFUNCTION(BlueprintCallable, Category = "Default")
	FString CallTTSServiceByInfo(const FString& InputText,
	                             const FCharacterSetupData& InputCharacterSetup,
	                             bool bGenerateAudioOnly = false);

	/**
 	 * Reads Metahuman Speech-to-Face (S2F) data with optional CSV and blink data.
 	 *
 	 * This function processes Metahuman S2F data and allows for additional CSV data
 	 * and blink animations to be incorporated. It's designed to be callable from Blueprints.
 	 *
 	 * @param InputCharacterSetup The setup data for the character, containing necessary information
 	 *                            for processing the S2F data.
 	 * @param bAddCSV             Whether to include additional CSV data in the processing.
 	 * @param bAddBlink           Whether to add blink animations to the facial expressions.
 	 *                            Defaults to false if not specified.
 	 * @param Extent              A scaling factor for the facial expressions. 
 	 *                            Defaults to 1.0f if not specified.
 	 *
 	 * @return FString
 	 */
	UFUNCTION(BlueprintCallable, Category = "Default")
	FString ReadMetahumanS2FDataWithExtraCSV(const FCharacterSetupData& InputCharacterSetup, bool bAddCSV,
	                                         bool bAddBlink = false,
	                                         float Extent = 1.0f);

private:
	/**
	 * 清理掉前一次制作 Sequence 产生的数据
	 * @return 是否成功
	 */
	bool CleanupData();

	/**
	 * 检测所有已完成，更新进度条和文字
	 */
	void OnEverythingFinish();

	////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Websocket 以及 动作生成、图片下载 回调
	////////////////////////////////////////////////////////////////////////////////////////////////////
	UFUNCTION()
	void OnTTSFinish_CallBack(FString CharacterName,
	                          bool bGenAnimSuccess,
	                          FString AnimPath,
	                          bool bGenAudioSuccess,
	                          FString AudioPath,
	                          float AudioTime,
	                          int32 CurrentIndex,
	                          int32 TotalIndex);
	UFUNCTION()
	void OnWsDisconnect_CallBack(bool bIsClean);

	UFUNCTION()
	void OnA2FFinish_CallBack(FString CharacterName,
	                          bool bGenAnimSuccess,
	                          FString AnimPath,
	                          bool bGenAudioSuccess,
	                          FString AudioPath,
	                          float AudioTime,
	                          int32 CurrentIndex,
	                          int32 TotalIndex);

	UFUNCTION()
	void OnCSVFinish_CallBack(FString CharacterName,
	                          bool bGenAnimSuccess,
	                          FString AnimPath,
	                          bool bGenAudioSuccess,
	                          FString AudioPath,
	                          float AudioTime,
	                          int32 CurrentIndex,
	                          int32 TotalIndex);


	UFUNCTION()
	void UpdateGenerateProgress(int32 CurrentChunk, int32 TotalChunk);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "TTS 地址", Category = "Default")
	FString TextToSoundServiceAddress = "wss://sit-online.xverse.cn/audio/multi_syn";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "S2F 地址", Category = "Default")
	FString SoundToFaceAddress = "ws://123.207.184.207:8001/ws";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "人物配置", Category = "Default")
	TMap<FString, FCharacterSetupData> CharacterSetup;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "两句之间的间隔时间", Category = "Default")
	float GapTimeBetweenSentences = 0.3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "渲染帧率", Category = "Default")
	int32 RenderFrameRate = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "音量大小", Category = "Default")
	float VoiceVolume = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "是否开启口型平滑", Category = "Default")
	bool bTtsAnimSmoothOut = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "口型平滑帧数", Category = "Default")
	int32 TtsAnimSmoothOutFrameNum = 5;

	UPROPERTY(BlueprintReadOnly, Category = "Default")
	float GenerateProgress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Default")
	FString DisplayMessage = TEXT("等待");

private:
	/// 储存主 Sequence 路径，用于跨函数修改渲染属性
	FString MainSequencePath{};

	/// 分句总数
	int32 TotalSentenceNumber{0};
	/// TTS 当前正在生成的句子序号
	int32 CurrentSentenceNumber{0};
	/// 总渲染帧数
	int32 TotalFrameNumber{0};

	/// 储存原始每一句话的文本和说话人信息
	TArray<FDialogueInfo> OriginInfos;
	/// 储存每一句话的文本信息、该句话的序号、TTS 返回的音频和口型动画路径、动作生成的动画路径
	TArray<FGeneratedDialogueAssetsInfo> ProcessedInfos;

	bool bIsWorking;

	UPROPERTY()
	TObjectPtr<USoundWave> SoundWaveStash;
};
