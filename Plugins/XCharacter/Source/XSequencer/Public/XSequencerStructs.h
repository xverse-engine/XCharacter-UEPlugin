// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "XSequencerStructs.generated.h"

UENUM(BlueprintType)
enum class EModalType : uint8
{
	TextToSound,
	SoundToFace,
	CsvService,
	SoundToFaceWithCSV
};

USTRUCT()
struct FMetahumanS2FSingleFrame
{
	GENERATED_BODY()

	UPROPERTY()
	float time{};

	UPROPERTY()
	float value{};
};

USTRUCT()
struct FGeneratedDialogueAssetsInfo
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SentenceNumber{0};

	UPROPERTY()
	FString Line{};

	UPROPERTY()
	FGuid LineSource{};

	UPROPERTY()
	FString AnimPath{};

	UPROPERTY()
	FString AudioPath{};

	UPROPERTY()
	float AudioLength{};
};

USTRUCT(BlueprintType)
struct FCharacterBlinkSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "是否开启眨眼", Category = "CharacterBlinkSettings")
	bool bShouldBlink{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "眨眼时间间隔下限", Category = "CharacterBlinkSettings")
	float BlinkRandomBottom{1.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "眨眼时间间隔上限", Category = "CharacterBlinkSettings")
	float BlinkRandomTop{7.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "一次眨眼的时间", Category = "CharacterBlinkSettings")
	float BlinkTime{0.3f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "眨眼程度", Category = "CharacterBlinkSettings", 
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BlinkExtent{1.f};
};

USTRUCT(BlueprintType)
struct FCharacterSetupData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "模型", Category = "CharacterSetupData")
	TObjectPtr<USkeletalMesh> Mesh{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "使用表情基骨骼动画（不勾选则使用 MorphTarget）", Category = "CharacterSetupData")
	bool UseArkitSkeletonAnim{false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "表情基骨骼动画", Category = "CharacterSetupData")
	TObjectPtr<UAnimSequence> ArkitAnim{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "音色", Category = "CharacterSetupData")
	FString Speaker{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "表情均值", Category = "CharacterSetupData", 
		meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float FaceScale{1.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "表情方差", Category = "CharacterSetupData", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float FaceVarScale{1.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Smile 程度", Category = "CharacterSetupData", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SmileExtent{1.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Smile Base", Category = "CharacterSetupData", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SmileBase{0.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Up Lips 程度", Category = "CharacterSetupData", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float UpLipsExtent{1.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Down Lips 程度", Category = "CharacterSetupData", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DownLipsExtent{1.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Nose 程度", Category = "CharacterSetupData", 
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float NoseExtent{1.0};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "眨眼设置", Category = "CharacterSetupData")
	FCharacterBlinkSettings BlinkSettings;
};

USTRUCT()
struct FDialogueInfo
{
	GENERATED_BODY()

	int32 SentenceNumber{0};
	FString Line{};
	FString CharacterName{};
	FCharacterSetupData CharacterSetup{};
};


USTRUCT()
struct FTTSGlobalSettings
{
	GENERATED_BODY()
	float SoundVolume = 1.0;
	bool bEaseOutExpressionAnim = true;
	int32 EaseOutFrameNumber = 5;
	bool csv = false;
};

USTRUCT(BlueprintType)
struct FEditorMetaInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString app_id{"demo"};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString user_id{"demo"};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString utt_id{"demo"};
};

USTRUCT(BlueprintType)
struct FEditorTalkingFaceJson
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FEditorMetaInfo meta_info{};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString text{"Hello World!"};

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString spkr{"g010"};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString style{"default"};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	int styledegree{100};

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString role{"default"};

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	bool stream{true};

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	int spd{100};

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	bool face{true};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	float face_scale{0.5f};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	float face_var_scale{1.0f};
};

USTRUCT(BlueprintType)
struct FSoundToFaceInitJson
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FEditorMetaInfo meta_info{};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString request{"create_engine"};

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	int sample_rate{16000};

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	int chunk_size{2};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	int num_left_chunks{32};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	float face_scale{0.5f};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	float face_var_scale{1.0f};
};

USTRUCT(BlueprintType)
struct FSoundToFaceRequestJson
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString request{"process"};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	FString audio_bytes{};
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	int is_end = false;
	UPROPERTY(BlueprintReadWrite, Category = "Avatar")
	int need_sync{1};
};


inline TArray<FName> ArkitBlendshapeNameList{
	"browDownLeft",
	"browDownRight",
	"browInnerUp",
	"browOuterUpLeft",
	"browOuterUpRight",
	"cheekPuff",
	"cheekSquintLeft",
	"cheekSquintRight",
	"eyeBlinkLeft",
	"eyeBlinkRight",
	"eyeLookDownLeft",
	"eyeLookDownRight",
	"eyeLookInLeft",
	"eyeLookInRight",
	"eyeLookOutLeft",
	"eyeLookOutRight",
	"eyeLookUpLeft",
	"eyeLookUpRight",
	"eyeSquintLeft",
	"eyeSquintRight",
	"eyeWideLeft",
	"eyeWideRight",
	"jawForward",
	"jawLeft",
	"jawOpen",
	"jawRight",
	"mouthClose",
	"mouthDimpleLeft",
	"mouthDimpleRight",
	"mouthFrownLeft",
	"mouthFrownRight",
	"mouthFunnel",
	"mouthLeft",
	"mouthLowerDownLeft",
	"mouthLowerDownRight",
	"mouthPressLeft",
	"mouthPressRight",
	"mouthPucker",
	"mouthRight",
	"mouthRollLower",
	"mouthRollUpper",
	"mouthShrugLower",
	"mouthShrugUpper",
	"mouthSmileLeft",
	"mouthSmileRight",
	"mouthStretchLeft",
	"mouthStretchRight",
	"mouthUpperUpLeft",
	"mouthUpperUpRight",
	"noseSneerLeft",
	"noseSneerRight"
};

inline TArray<FString> ArkitBlendshapeNameStringListWithoutMouthClose{
	"browDownLeft",
	"browDownRight",
	"browInnerUp",
	"browOuterUpLeft",
	"browOuterUpRight",
	"cheekPuff",
	"cheekSquintLeft",
	"cheekSquintRight",
	"eyeBlinkLeft",
	"eyeBlinkRight",
	"eyeLookDownLeft",
	"eyeLookDownRight",
	"eyeLookInLeft",
	"eyeLookInRight",
	"eyeLookOutLeft",
	"eyeLookOutRight",
	"eyeLookUpLeft",
	"eyeLookUpRight",
	"eyeSquintLeft",
	"eyeSquintRight",
	"eyeWideLeft",
	"eyeWideRight",
	"jawForward",
	"jawLeft",
	"jawOpen",
	"jawRight",
	"mouthClose",
	"mouthDimpleLeft",
	"mouthDimpleRight",
	"mouthFrownLeft",
	"mouthFrownRight",
	"mouthFunnel",
	"mouthLeft",
	"mouthLowerDownLeft",
	"mouthLowerDownRight",
	"mouthPressLeft",
	"mouthPressRight",
	"mouthPucker",
	"mouthRight",
	"mouthRollLower",
	"mouthRollUpper",
	"mouthShrugLower",
	"mouthShrugUpper",
	"mouthSmileLeft",
	"mouthSmileRight",
	"mouthStretchLeft",
	"mouthStretchRight",
	"mouthUpperUpLeft",
	"mouthUpperUpRight",
	"noseSneerLeft",
	"noseSneerRight"
};


inline TArray<FString> ArkitBlendshapeNameStringList{
	"BrowDownLeft",
	"BrowDownRight",
	"BrowInnerUp",
	"BrowOuterUpLeft",
	"BrowOuterUpRight",
	"CheekPuff",
	"CheekSquintLeft",
	"CheekSquintRight",
	"EyeBlinkLeft",
	"EyeBlinkRight",
	"EyeLookDownLeft",
	"EyeLookDownRight",
	"EyeLookInLeft",
	"EyeLookInRight",
	"EyeLookOutLeft",
	"EyeLookOutRight",
	"EyeLookUpLeft",
	"EyeLookUpRight",
	"EyeSquintLeft",
	"EyeSquintRight",
	"EyeWideLeft",
	"EyeWideRight",
	"JawForward",
	"JawLeft",
	"JawOpen",
	"JawRight",
	"MouthClose",
	"MouthDimpleLeft",
	"MouthDimpleRight",
	"MouthFrownLeft",
	"MouthFrownRight",
	"MouthFunnel",
	"MouthLeft",
	"MouthLowerDownLeft",
	"MouthLowerDownRight",
	"MouthPressLeft",
	"MouthPressRight",
	"MouthPucker",
	"MouthRight",
	"MouthRollLower",
	"MouthRollUpper",
	"MouthShrugLower",
	"MouthShrugUpper",
	"MouthSmileLeft",
	"MouthSmileRight",
	"MouthStretchLeft",
	"MouthStretchRight",
	"MouthUpperUpLeft",
	"MouthUpperUpRight",
	"NoseSneerLeft",
	"NoseSneerRight"
};

inline TArray<FString> MetahumanS2FBlendshapeList{
	"eyeBlinkLeft",
	"eyeLookDownLeft",
	"eyeLookInLeft",
	"eyeLookOutLeft",
	"eyeLookUpLeft",
	"eyeSquintLeft",
	"eyeWideLeft",
	"eyeBlinkRight",
	"eyeLookDownRight",
	"eyeLookInRight",
	"eyeLookOutRight",
	"eyeLookUpRight",
	"eyeSquintRight",
	"eyeWideRight",
	"jawForward",
	"jawLeft",
	"jawRight",
	"jawOpen",
	"mouthFunnel",
	"mouthPucker",
	"mouthLeft",
	"mouthRight",
	"mouthSmileLeft",
	"mouthSmileRight",
	"mouthFrownLeft",
	"mouthFrownRight",
	"mouthDimpleLeft",
	"mouthDimpleRight",
	"mouthStretchLeft",
	"mouthStretchRight",
	"mouthRollLower",
	"mouthRollUpper",
	"mouthShrugLower",
	"mouthShrugUpper",
	"mouthPressLeft",
	"mouthPressRight",
	"mouthLowerDownLeft",
	"mouthLowerDownRight",
	"mouthUpperUpLeft",
	"mouthUpperUpRight",
	"browDownLeft",
	"browDownRight",
	"browInnerUp",
	"browOuterUpLeft",
	"browOuterUpRight",
	"cheekPuff",
	"cheekSquintLeft",
	"cheekSquintRight",
	"noseSneerLeft",
	"noseSneerRight",
	"tongueOut",
};
