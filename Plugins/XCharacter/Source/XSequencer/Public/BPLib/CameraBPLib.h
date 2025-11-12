// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraBPLib.generated.h"

class ACameraActor;
class UMovieSceneCameraCutTrack;

/**
 * 
 */
UCLASS()
class XSEQUENCER_API UCameraBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region Camera

	/**
	 * 在场景中生成普通相机
	 * @param bUseEditorWorld		是否使用当前编辑器中打开的关卡
	 * @param WorldPath				如果不使用当前编辑器中的关卡，则传入需要生成相机的 Level 路径
	 * @param Location				生成位置
	 * @param Rotation				生成旋转
	 * @param bOutSuccess			返回是否成功
	 * @param AspectRatio			所生成相机的纵横比，默认 9:16 即 0.5625
	 * @param FieldOfView			所生成相机的 FOV，默认 30
	 * @return 所生成的 Camera Actor
	 */
	UE_DEPRECATED(5.3, "不再在场景中生成相机，而是直接在 Sequence 中生成相机")
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Camera")
	static ACameraActor* SpawnCameraInScene(bool bUseEditorWorld,
	                                        const FString& WorldPath,
	                                        const FVector& Location,
	                                        const FRotator& Rotation,
	                                        bool& bOutSuccess,
	                                        float AspectRatio = 0.5625f,
	                                        float FieldOfView = 30.f);

	/**
 	 * 在场景中生成电影相机，默认生成纵横比 9:16
 	 * @param bUseEditorWorld		是否使用当前编辑器中打开的关卡
 	 * @param WorldPath				如果不使用当前编辑器中的关卡，则传入需要生成相机的 Level 路径
 	 * @param Location				生成位置
 	 * @param Rotation				生成旋转
 	 * @param bOutSuccess			返回是否成功
 	 * @param FocusLength			所生成相机的纵焦距，默认 35
 	 * @param Aperture				所生成相机的光圈，默认 2.8
 	 * @return 所生成的 Camera Actor
 	 */
	UE_DEPRECATED(5.3, "不再在场景中生成相机，而是直接在 Sequence 中生成相机")
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Camera")
	static ACameraActor* SpawnCinematicCameraInScene(bool bUseEditorWorld,
	                                                 const FString& WorldPath,
	                                                 const FVector& Location,
	                                                 const FRotator& Rotation,
	                                                 bool& bOutSuccess,
	                                                 float FocusLength = 35.f,
	                                                 float Aperture = 2.8f);

#pragma endregion

#pragma region CameraTrack

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | CameraCutTrack")
	static UMovieSceneCameraCutTrack* GetCameraCutTrackFromSeq(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | CameraCutTrack")
	static UMovieSceneCameraCutTrack* AddCameraCutTrackInSeq(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | CameraCutTrack")
	static bool RemoveCameraCutTrackFromSeq(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | CameraCutTrack")
	static FGuid LinkCameraToCameraCutTrack(const FString& LevelSeqPath,
	                                        ACameraActor* CameraActor,
	                                        int32 StartFrame = 0,
	                                        int32 EndFrame = -1);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | CameraCutTrack")
	static bool LinkCameraToCameraCutTrackWithCameraGUID(const FString& LevelSeqPath,
	                                                     const FGuid& CameraGuid,
	                                                     int32 StartFrame = 0,
	                                                     int32 EndFrame = -1);
#pragma endregion
};
