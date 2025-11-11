// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ProtocolStruct.generated.h"

//USTRUCT(BlueprintType)
//struct FMetaInfo
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString app_id = "11007";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString user_id = "xiaoming";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString utt_id = "d38c4367-fca9-4b72-81a2-03a1837db1aa";
//};
//
//USTRUCT(BlueprintType)
//struct FAsrConfig
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool use_vad = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool add_itn = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool add_punctuation = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool use_lm = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    int32 chunk_size = 12;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    int32 num_left_chunks = 10;
//};
//
//USTRUCT(BlueprintType)
//struct FChatInfo
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString roleIdAI = "qqdance001";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString roleIdUser = "qqdance_user";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString scenarioId = "qqdance_scene_01";
//};
//
//USTRUCT(BlueprintType)
//struct FMultiTtsConfig
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString spkr = "m015";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString style = "default";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    int32 styledegree = 100;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString role = "default";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    int32 spd = 100;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool stream = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool face = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    float face_scale = 1.100001f;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    float face_var_scale = 1.300001f;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool gesture = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString gesturestyle = "default";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    float arm_scale = 0.500001f;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    float body_scale = 0.500001f;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    bool chat = true;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FString chat_engine = "LLM-C";
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FChatInfo chat_info;
//};
//
//USTRUCT(BlueprintType)
//struct FConfig
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FMetaInfo meta_info;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FAsrConfig asr_config;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite)
//    FMultiTtsConfig multi_tts_config;
//};
//
//
//
///***********response************/

//
//USTRUCT(BlueprintType)
//struct FGestureData
//{
//    GENERATED_BODY()
//
//    UPROPERTY(BlueprintReadWrite, Category = "GestureData")
//    TArray<FString> bone_name;
//
//    UPROPERTY(BlueprintReadWrite, Category = "GestureData")
//    FString bone_bytes;
//};
//
//USTRUCT(BlueprintType)
//struct FRespondInfo
//{
//    GENERATED_BODY()
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatResponse")
//    bool chat;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatResponse")
//    FString text;
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatResponse")
//    FString emotion;  // Assuming emotion could be a string describing the emotion
//
//    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatResponse")
//    FString action;  // Assuming action could be a string describing an action
//};
//
//
//USTRUCT(BlueprintType)
//struct FChatResponse
//{
//    GENERATED_BODY()
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString utt_id;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString err_no;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString status;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString audio;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FAudioEnc audio_enc;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString face_bytes;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString face_rows;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString face_cols;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FGestureData gesture_data;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString chat_respond;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FRespondInfo respond_info;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    bool is_end;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString user_state;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString agent_state;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString trace_id;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString asr_result;
//
//    UPROPERTY(BlueprintReadWrite, Category = "ChatResponse")
//    FString asr_type;
//};
//
//
//USTRUCT(BlueprintType)
//struct FTalkingFaceJson
//{
//    GENERATED_BODY()
//
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    FMetaInfo meta_info{};
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    FString text{ "Hello World!" };
//
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    FString spkr{ "g010" };
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    FString style{ "default" };
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    int styledegree{ 100 };
//
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    FString role{ "default" };
//
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    bool stream{ true };
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    int sample_rate{ 24000 };
//
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    int spd{ 100 };
//
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    bool face{ true };
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    float face_scale{ 0.5f };
//    UPROPERTY(BlueprintReadWrite, Category = "Avatar")
//    float face_var_scale{ 1.0f };
//};

/** V2 Response **/



USTRUCT(BlueprintType)
struct FMetaInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaInfo")
    FString app_id{"11007"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaInfo")
    FString user_id{"xiaoming"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaInfo")
    FString utt_id{"d38c4367-fca9-4b72-81a2-03a1837db1aa"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaInfo")
    FString api_key{"XoPGHOXtDcNcmyBuJQlg4gYmpM3b0Fv5"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaInfo")
    FString input_type{"audio"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaInfo")
    FString task_type{"s2f"};
};

USTRUCT(BlueprintType)
struct FTTSConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    FString text{"Hello World!"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    FString speaker{"g002"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    FString style{"default"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    int32 style_degree{100};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    int32 volume{1};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    FString role{"default"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    float speed{1.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    bool stream{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    FString encoding{"pcm"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    int32 max_sen_words{100};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSConfig")
    FString response_format{"pcm"};
};

USTRUCT(BlueprintType)
struct FFaceChannel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceChannel")
    float face_scale{1.00001f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceChannel")
    float face_var_scale{1.00001f};
};

USTRUCT(BlueprintType)
struct FGestureChannel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GestureChannel")
    FString gesture_style{"default"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GestureChannel")
    float arm_scale{0.500001f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GestureChannel")
    float body_scale{0.500001f};
};

USTRUCT(BlueprintType)
struct FFaceResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceResponse")
    FString face_bytes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceResponse")
    int32 face_rows;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceResponse")
    int32 face_cols;

};


USTRUCT(BlueprintType)
struct FChatContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatContext")
    FString role{"user"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatContext")
    FString content{"（高兴）讲个笑话"};
};

USTRUCT(BlueprintType)
struct FGestureData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "GestureData")
    TArray<FString> bone_names;

    UPROPERTY(BlueprintReadWrite, Category = "GestureData")
    FString bone_bytes;
};

USTRUCT(BlueprintType)
struct FGestureResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GestureResponse")
    FGestureData gesture_data;

};

USTRUCT(BlueprintType)
struct FAnimResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimResponse")
    FFaceResponse face_channel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimResponse")
    FGestureResponse gesture_channel;
};

USTRUCT(BlueprintType)
struct FChatConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatConfig")
    FString role_id_ai{"qqdance001"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatConfig")
    FString role_id_user{"qqdance_user"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatConfig")
    FString scenario_id{"qqdance_scene_01"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatConfig")
    FString chat_engine{"LLM-X"};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatConfig")
    TArray<FChatContext> context;
};

USTRUCT(BlueprintType)
struct FAudioEnc
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "AudioEnc")
    FString enc{"pcm"};

    UPROPERTY(BlueprintReadWrite, Category = "AudioEnc")
    int32 sr{44100};
};

USTRUCT(BlueprintType)
struct FAnimConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimConfig")
    FFaceChannel face_channel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimConfig")
    FGestureChannel gesture_channel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimConfig")
    FString audio;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimConfig")
    FAudioEnc audio_enc;
};


USTRUCT(BlueprintType)
struct FChatResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatResponse")
    FString token_type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChatResponse")
    FString token_content;

};

USTRUCT(BlueprintType)
struct FTTSResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSResponse")
    FString text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSResponse")
    FString audio;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSResponse")
    FAudioEnc audio_enc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TTSResponse")
    TMap<FString, FString> respond_info;
};

USTRUCT(BlueprintType)
struct FResponseContent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResponseContent")
    FChatResponse chat_channel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResponseContent")
    FTTSResponse tts_channel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ResponseContent")
    FAnimResponse anim_channel;
};

USTRUCT(BlueprintType)
struct FMultiModalConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiModalConfig")
    FTTSConfig tts_config;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiModalConfig")
    FChatConfig chat_config;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiModalConfig")
    FAnimConfig anim_config;

    //UPROPERTY(EditAnywhere, BlueprintReadWrite)
    //FASRConfig asr_config;

};

USTRUCT(BlueprintType)
struct FRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
    FMetaInfo meta_info;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
    FMultiModalConfig multimodal_config;
};

//Response
USTRUCT(BlueprintType)
struct FResponse
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
    FMetaInfo meta_info;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
    int32 err_no;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
    FString err_msg;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
    bool is_end;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
    FResponseContent respond_content;
};






