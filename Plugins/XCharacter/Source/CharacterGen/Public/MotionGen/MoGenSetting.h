// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MoGenStruct.h"
#include "UObject/NoExportTypes.h"
#include "MoGenSetting.generated.h"


/**
 * 
 */
UCLASS()
class CHARACTERGEN_API UMoGenSetting : public UObject
{
	GENERATED_BODY()

public:
	/** Reference SkeletonMesh */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReferenceSkeleton, meta = (ImportType = "SkeletalMesh|Animation"))
	TObjectPtr<class USkeleton> Skeleton;

	/** Prompt content*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionGen")
	FString Prompt = "a man is walking";

	/** AssetName of generated animation sequence*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionGen")
	FString AssetName = "walking_man";

	/** Animation sequence length (s) Maximum is 10 seconds, minimum 1s.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionGen", meta = (EnumCondition = 2, ClampMin = "1.0"))
	float AnimationLength = 10.0f;

	/** Root Offset Height of Animation Data, default is 15 cm*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionGen")
	float OffsetLength = 15.0f;
	
	/** Is Using MotionGen Recommended Animation Length*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionGen")
	bool bUseRecommendLength = false;

	/** Accessor and initializer*/
	static UMoGenSetting* Get();

public:
	virtual void Serialize(class FArchive& Archive) override;

};