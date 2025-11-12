// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BVHImportSettings.generated.h"

UENUM(BlueprintType)
enum class EEulerOrder : uint8
{
	None = 0 UMETA(Hidden),
	ZXY = 1,
};

UENUM(BlueprintType)
enum class EBVHSamplingType : uint8
{
	PerFrame,	
	PerXFrames UMETA(DisplayName = "Per X Frames"),
	PerTimeStep
};

UCLASS(Blueprintable)
class UBVHImportSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UBVHImportSettings()
	{
		SamplingType = EBVHSamplingType::PerFrame;
		TimeStep = 0.05f;
		FrameNum = FrameStart = FrameEnd = 0;
		ResampleRate = DEFAULT_SAMPLERATE;
	}

	/** Skeleton to use for imported asset. When importing a mesh, leaving this as "None" will create a new skeleton. When importing an animation this MUST be specified to import the asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mesh, meta = (ImportType = "SkeletalMesh|Animation"))
	TObjectPtr<class USkeleton> Skeleton;

	/** Type of sampling performed while importing the animation*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sampling)
	FString MotionName;

	/** Type of sampling performed while importing the animation*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sampling)
	EBVHSamplingType SamplingType;

	/** Time steps to take when sampling the animation*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sampling, meta = (EnumCondition = 2, ClampMin = "0.0001"))
	float TimeStep;

	/** Starting index to start sampling the animation from*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sampling)
	int32 FrameNum;
	
	/** Starting index to start sampling the animation from*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sampling)
	int32 FrameStart;

	/** Ending index to stop sampling the animation at*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sampling)
	int32 FrameEnd;

	/** Ending index to stop sampling the animation at*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sampling)
	int32 ResampleRate;	

	/** Accessor and initializer **/
	static UBVHImportSettings* Get();
	bool bReimport;

public:
	virtual void Serialize(class FArchive& Archive) override;
};
