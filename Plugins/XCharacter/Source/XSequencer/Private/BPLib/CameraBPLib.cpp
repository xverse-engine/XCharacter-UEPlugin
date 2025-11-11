// Fill out your copyright notice in the Description page of Project Settings.

#include "BPLib/CameraBPLib.h"

#include "XSequencerDefines.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "BPLib/SequenceBPLib.h"

#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

#include "Engine/World.h"
#include "Editor/EditorEngine.h"

DEFINE_LOG_CATEGORY(SeqCameraBPLib);

#pragma region CameraTrack

ACameraActor* UCameraBPLib::SpawnCameraInScene(bool bUseEditorWorld,
                                               const FString& WorldPath,
                                               const FVector& Location,
                                               const FRotator& Rotation,
                                               bool& bOutSuccess,
                                               float AspectRatio,
                                               float FieldOfView)
{
	UWorld* World{nullptr};
	if (bUseEditorWorld)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	else
	{
		World = Cast<UWorld>(StaticLoadObject(UWorld::StaticClass(), nullptr, *WorldPath));
	}

	if (World == nullptr)
	{
		bOutSuccess = false;
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("SpawnCameraInScene - World Path Invalid - %s"),
		       *WorldPath);
		return nullptr;
	}

	ACameraActor* SpawnedCamera = Cast<ACameraActor>(World->SpawnActor(ACameraActor::StaticClass()));
	if (SpawnedCamera == nullptr || !SpawnedCamera->SetActorLocationAndRotation(Location, Rotation))
	{
		bOutSuccess = false;
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("SpawnCameraInScene - Unable to Spawn Camera Actor in World - %s"),
		       *WorldPath);
		return nullptr;
	}

	SpawnedCamera->GetCameraComponent()->AspectRatio = AspectRatio;
	SpawnedCamera->GetCameraComponent()->FieldOfView = FieldOfView;

	bOutSuccess = true;
	return SpawnedCamera;
}

ACameraActor* UCameraBPLib::SpawnCinematicCameraInScene(bool bUseEditorWorld,
                                                        const FString& WorldPath,
                                                        const FVector& Location,
                                                        const FRotator& Rotation,
                                                        bool& bOutSuccess,
                                                        float FocusLength,
                                                        float Aperture)
{
	UWorld* World{nullptr};
	if (bUseEditorWorld)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	else
	{
		World = Cast<UWorld>(StaticLoadObject(UWorld::StaticClass(), nullptr, *WorldPath));
	}

	if (World == nullptr)
	{
		bOutSuccess = false;
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("SpawnCinematicCameraInScene - World Path Invalid - %s"),
		       *WorldPath);
		return nullptr;
	}

	ACineCameraActor* SpawnedCamera = Cast<ACineCameraActor>(World->SpawnActor(ACineCameraActor::StaticClass()));
	if (SpawnedCamera == nullptr || !SpawnedCamera->SetActorLocationAndRotation(Location, Rotation))
	{
		bOutSuccess = false;
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("SpawnCinematicCameraInScene - Unable to spawn Cine Camera Actor in world - %s"),
		       *WorldPath);
		return nullptr;
	}

	SpawnedCamera->GetCineCameraComponent()->AspectRatio = 0.5625;
	SpawnedCamera->GetCineCameraComponent()->Filmback.SensorWidth = 13.5;
	SpawnedCamera->GetCineCameraComponent()->Filmback.SensorHeight = 24;
	SpawnedCamera->GetCineCameraComponent()->CurrentFocalLength = FocusLength;
	SpawnedCamera->GetCineCameraComponent()->CurrentAperture = Aperture;

	SpawnedCamera->GetCineCameraComponent()->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;

	bOutSuccess = true;
	return SpawnedCamera;
}

UMovieSceneCameraCutTrack* UCameraBPLib::GetCameraCutTrackFromSeq(const FString& LevelSeqPath)
{
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error, TEXT("GetCameraCutTrackFromSeq - LevelSequence invalid - %s"), *LevelSeqPath);
		return nullptr;
	}

	UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(
		LevelSequence->MovieScene->GetCameraCutTrack());

	if (CameraCutTrack == nullptr)
	{
		return nullptr;
	}

	return CameraCutTrack;
}

UMovieSceneCameraCutTrack* UCameraBPLib::AddCameraCutTrackInSeq(const FString& LevelSeqPath)
{
	auto CameraCutTrack = GetCameraCutTrackFromSeq(LevelSeqPath);
	if (CameraCutTrack != nullptr)
	{
		return CameraCutTrack;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));

	CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(
		LevelSequence->MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass()));

	if (CameraCutTrack == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("AddCameraCutTrackInSeq - Unable to Add Camera Cut Track In Sequence - %s"),
		       *LevelSeqPath);
	}

	return CameraCutTrack;
}

bool UCameraBPLib::RemoveCameraCutTrackFromSeq(const FString& LevelSeqPath)
{
	auto CameraCutTrack = GetCameraCutTrackFromSeq(LevelSeqPath);
	if (CameraCutTrack == nullptr)
	{
		return true;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error, TEXT("RemoveCameraCutTrackFromSeq - LevelSequence invalid - %s"), *LevelSeqPath);
		return false;
	}

	LevelSequence->MovieScene->RemoveCameraCutTrack();

	UE_LOG(SeqCameraBPLib, Warning,
	       TEXT("RemoveCameraCutTrackFromSeq - Successfully Removed Camera Cut Track - %s"),
	       *LevelSeqPath);
	return true;
}

FGuid UCameraBPLib::LinkCameraToCameraCutTrack(const FString& LevelSeqPath, ACameraActor* CameraActor,
                                               int32 StartFrame, int32 EndFrame)
{
	if (!CameraActor)
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("LinkCameraToCameraCutTrack - Pass in Camera Actor invalid - %s"),
		       *LevelSeqPath);
		return FGuid();
	}

	auto CameraCutTrack = GetCameraCutTrackFromSeq(LevelSeqPath);
	if (CameraCutTrack == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("LinkCameraToCameraCutTrack - Unable to Find Camera Cut Track In Sequence - %s"),
		       *LevelSeqPath);
		return FGuid();
	}

	auto CameraGuid = USequenceBPLib::AddPossessableToSequence(LevelSeqPath, CameraActor);
	if (!CameraGuid.IsValid())
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("LinkCameraToCameraCutTrack - Unable to Get Guid For Passed in Camera Actor - %s"),
		       *LevelSeqPath);
		return FGuid();
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error, TEXT("LinkCameraToCameraCutTrack - LevelSequence invalid - %s"), *LevelSeqPath);
		return FGuid();
	}

	const int FrameTickValue = LevelSequence->MovieScene->GetTickResolution().AsDecimal() / LevelSequence->MovieScene->
		GetDisplayRate().AsDecimal();
	auto CameraSection = CameraCutTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(CameraGuid),
	                                                     FFrameNumber(StartFrame * FrameTickValue));
	if (CameraSection == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("LinkCameraToCameraCutTrack - Unable To Add New Camera Section In Camera Cut Track - %s"),
		       *LevelSeqPath);
		return FGuid();
	}

	if (EndFrame > StartFrame)
	{
		CameraSection->SetEndFrame(FFrameNumber(EndFrame * FrameTickValue));
	}

	return CameraGuid;
}

bool UCameraBPLib::LinkCameraToCameraCutTrackWithCameraGUID(const FString& LevelSeqPath,
                                                            const FGuid& CameraGuid,
                                                            int32 StartFrame,
                                                            int32 EndFrame)
{
	auto CameraCutTrack = GetCameraCutTrackFromSeq(LevelSeqPath);
	if (CameraCutTrack == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("LinkCameraToCameraCutTrackWithCameraGUID - Unable to Find Camera Cut Track In Sequence - %s"),
		       *LevelSeqPath);
		return false;
	}

	if (!CameraGuid.IsValid())
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("LinkCameraToCameraCutTrackWithCameraGUID - Invalid Camera GUID - %s"),
		       *CameraGuid.ToString());
		return false;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT("LinkCameraToCameraCutTrackWithCameraGUID - LevelSequence invalid - %s"),
		       *LevelSeqPath);
		return false;
	}

	const int FrameTickValue = LevelSequence->MovieScene->GetTickResolution().AsDecimal() / LevelSequence->MovieScene->
		GetDisplayRate().AsDecimal();
	auto CameraSection = CameraCutTrack->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(CameraGuid),
	                                                     FFrameNumber(StartFrame * FrameTickValue));
	if (CameraSection == nullptr)
	{
		UE_LOG(SeqCameraBPLib, Error,
		       TEXT(
			       "LinkCameraToCameraCutTrackWithCameraGUID - Unable To Add New Camera Section In Camera Cut Track - %s"
		       ),
		       *LevelSeqPath);
		return false;
	}

	if (EndFrame > StartFrame)
	{
		CameraSection->SetEndFrame(FFrameNumber(EndFrame * FrameTickValue));
	}

	return true;
}

#pragma endregion
