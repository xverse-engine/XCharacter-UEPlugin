// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "XVCPluginSettings.generated.h"

/**
 * 
 */
UCLASS(config = XVCPluginSettings)
class PROTOCOL_API UXVCPluginSettings : public UObject
{
	GENERATED_BODY()
	
public:
	UXVCPluginSettings(const FObjectInitializer& obj);

	//UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "Server Address"))
	//FString ServerAddress;

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "Secret Key", Sensitive = true))
	FString SecretKey;

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "MotionGen Download Path"))
	FString MotionGenWorkSpace {"/Game/MotionGen/Download/"};

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "MultiModalChat Download Path"))
	FString MultiModalChatWorkSpace {"/Game/MultiModalChat/Download/"};

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "XSequencer Download Path"))
	FString XSequencerWorkSpace{ "/Game/XSequencer/Download/" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "TTS Storage Path"))
	FString TTSStoragePath{ "/Game/TTS/" };

    UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "A2F Storage Path"))
	FString A2FStoragePath{ "/Game/FacialAnimation/" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "TTS Server IP"))
	FString IP_TTS_Server{ "http://122.152.192.185:8008/" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "Zero-Shot Server IP"))
	FString IP_ZeroShot_Server{ "http://122.152.192.185:8009/" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "A2F Server IP"))
	FString IP_A2F_Server{ "http://122.152.192.185:8001/" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "每段文本合成音频数量", ClampMin = "1", ClampMax = "10"))
	int32 TTSReplicatesPerSegment = 4;

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "TTS分批处理大小", ClampMin = "1", ClampMax = "10"))
	int32 TTSBatchSize = 2;

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "Role Id AI"))
	FString RoleIdAI{ "qqdance001"};

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "Role Id User"))
	FString RoleIdUser{ "qqdance_user" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "Scenario Id"))
	FString ScenarioId{ "qqdance_scene_01" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "Chat Engine"))
	FString ChatEngine{ "LLM-X" };

	UPROPERTY(Config, EditAnywhere, Category = Settings, Meta = (DisplayName = "UserID"))
	FString UserID{ "UE5Plugin" };



	//Server IP
	const FString IP_TestServer = "http://10.0.39.80:7777/";
	const FString API_Test = "v1/test";

    // 平台服务器配置
	const FString IP_Platform = "https://sh-sit-online.xverse.cn/public/aifactory/";
	const FString API_CreateTask = "v1/createTask";
	const FString API_GetTaskInfo = "v1/getTaskInfo";
    
    // LLM服务器配置
	const FString IP_LLM = "https://prd-yuanx-llm-server.xverse.cn/release/prd_yuanxllm/api/";
	const FString API_ServerAuth = "v1/auth?service=XVCharacter";

    // MoGen服务器配置
	const FString AppID = "11174";
	const FString TaskType = "MoGenInfer";

    // TTS服务器配置
	const FString API_Synthesis = "api/inference/sft";
	const FString API_ZeroShot = "api/inference/zero_shot";
	const FString API_GetSpeakerList = "api/get_spkr_config";
	const FString API_EnhanceAudio = "api/enhance_audio";
	
	// A2F服务器配置
	const FString API_Audio2Face = "api/infer";
	
	//const FString AssetPath = "/XCharacter/Animation/";
	const float Timeout = 100.0f; //100s
	const int MaxRequestTime = 60; //60 ticks * heart beat
	const float HeartBeat = 1.0f; // 1s
};
