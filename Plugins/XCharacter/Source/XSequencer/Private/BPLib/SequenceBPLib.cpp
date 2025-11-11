// Fill out your copyright notice in the Description page of Project Settings.

#include "BPLib/SequenceBPLib.h"

#include "XSequencer.h"
#include "XSequencerDefines.h"

#include "Editor/EditorEngine.h"

#include "IAssetTools.h"
#include "AssetToolsModule.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "LevelSequenceActor.h"

#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
#include "Sound/SoundWave.h"
#else
#endif


#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 2
#include "Sound/SoundWave.h"
#endif

DEFINE_LOG_CATEGORY(AutoSequencerBPLib);

const FString SequenceFolder = "/Game/YDAutomation/Sequences/";

ULevelSequence* USequenceBPLib::GetLevelSequenceByPath(FString& SequencePath)
{
	const auto LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *SequencePath));
	if (!LevelSequence)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("SetSequenceEndTime: Level Sequence Not Valid - %s"),
		       *SequencePath);
		return nullptr;
	}
	return LevelSequence;
}

bool USequenceBPLib::CreateLevelSequence(FString& CreatedSequencePath, bool OpenInEditor, const FString& Prefix)
{
	// Create a new level sequence
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = nullptr;

	const FString CurrentTime = FXSequencerModule::GetDateTimeSnakeCase();
	const FString AssetName = Prefix + "-" + "Seq_" + CurrentTime;

	// Attempt to create a new asset
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* CurrentClass = *It;
		if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !CurrentClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
			if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 &&
				Factory->SupportedClass == ULevelSequence::StaticClass())
			{
				NewAsset = AssetTools.CreateAsset(AssetName,
				                                  SequenceFolder + AssetName,
				                                  ULevelSequence::StaticClass(),
				                                  Factory);
				break;
			}
		}
	}

	if (!NewAsset)
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("CreateLevelSequence: Error Creating File For Level Sequence"));
		CreatedSequencePath = "";
		return false;
	}

	// Spawn an actor at the origin, and either move infront of the camera or focus camera on it (depending on the viewport) and open for edit
	UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ALevelSequenceActor::StaticClass());
	if (!ensure(ActorFactory))
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("CreateLevelSequence: Error Creating Level Sequence Actor Factory"));
		CreatedSequencePath = "";
		return false;
	}

	AActor* Actor = GEditor->UseActorFactory(ActorFactory, FAssetData(NewAsset), &FTransform::Identity);
	if (Actor == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("CreateLevelSequence: Error Creating Level Sequence Actor"));
		CreatedSequencePath = "";
		return false;
	}

	if (OpenInEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewAsset);
	}

	CreatedSequencePath = SequenceFolder + AssetName + "/" + AssetName;
	UE_LOG(AutoSequencerBPLib, Warning,
	       TEXT("CreateLevelSequence: Successfully created level sequence - %s"),
	       *CreatedSequencePath);

	return true;
}

bool USequenceBPLib::SetSequenceRenderParameters(const FString& LevelSeqPath,
                                                 float StartTime,
                                                 float EndTime,
                                                 int32 FrameRate)
{
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr || EndTime < StartTime)
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("SetSequenceEndTime: LevelSequence invalid Or Time Not Valid - %s"),
		       *LevelSeqPath);
		return false;
	}

	LevelSequence->MovieScene->SetDisplayRate(FFrameRate{static_cast<uint32>(FrameRate), 1});

	LevelSequence->MovieScene->SetWorkingRange(StartTime, EndTime);
	LevelSequence->MovieScene->SetViewRange(StartTime, EndTime);

	const int TickResolution = LevelSequence->MovieScene->GetTickResolution().AsDecimal();
	LevelSequence->MovieScene->SetPlaybackRange(FFrameNumber{static_cast<int32>(StartTime)}, EndTime * TickResolution);
	LevelSequence->MovieScene->SetSelectionRange(TRange<FFrameNumber>{
		FFrameNumber{static_cast<int32>(StartTime)}, FFrameNumber{static_cast<int32>(EndTime * TickResolution)}
	});

	return true;
}

FGuid USequenceBPLib::AddCharacterToSequenceByPath(const FString& LevelSeqPath,
                                                   const FString& CharName,
                                                   const FString& MeshPath)
{
	return AddSpawnableToSequence(LevelSeqPath, MeshPath, CharName);
}

FGuid USequenceBPLib::AddCharacterToSequenceByObject(const FString& LevelSeqPath,
                                                     const FString& CharName,
                                                     USkeletalMesh* Mesh)
{
	return AddSpawnableToSequenceByObject(LevelSeqPath, Mesh, CharName);
}

#pragma region GetGuid|Add|Remove

TArray<FGuid> USequenceBPLib::GetAllGuidsFromSequence(const FString& LevelSeqPath)
{
	const auto LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (!LevelSequence || !LevelSequence->GetMovieScene())
	{
		UE_LOG(AutoSequencerBPLib, Warning,
		       TEXT("SetSequenceEndTime: Level Sequence Not Valid - %s"),
		       *LevelSeqPath);
		return TArray<FGuid>{};
	}

	TArray<FGuid> OutGuids{};
	for (const auto& Binding : LevelSequence->GetMovieScene()->GetBindings())
	{
		OutGuids.Add(Binding.GetObjectGuid());
	}

	return OutGuids;
}

AActor* USequenceBPLib::GetPossessableActorUsingGuidFromSequence(const FString& LevelSeqPath,
                                                                 const FGuid& ActorGuid)
{
	const auto LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (!LevelSequence)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("SetSequenceEndTime: Level Sequence Not Valid - %s"),
		       *LevelSeqPath);
		return nullptr;
	}

	// Actor 必须存在于当前场景中才能获取到
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("SetSequenceEndTime: Unable To Find Current World"));
		return nullptr;
	}

	TArray<UObject*, TInlineAllocator<1>> OutObjects{};
#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION <= 3)
	LevelSequence->LocateBoundObjects(ActorGuid, World, OutObjects);
#else
	UE::UniversalObjectLocator::FResolveParams Params(World);
	LevelSequence->LocateBoundObjects(ActorGuid, Params, OutObjects);
#endif

	if (OutObjects.Num() <= 0)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("SetSequenceEndTime: Unable To Find Actor For GUID %s in Sequence - %s"),
		       *ActorGuid.ToString(), *LevelSeqPath);
		return nullptr;
	}

	return Cast<AActor>(OutObjects[0]);
}

FGuid USequenceBPLib::GetSpawnableGuidFromSequence(const FString& LevelSeqPath,
                                                   const FString& SpawnableName)
{
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return FGuid();
	}

	FGuid Guid = FGuid();
	for (size_t x = 0; x < LevelSequence->MovieScene->GetSpawnableCount(); x++)
	{
		FMovieSceneSpawnable Spawnable = LevelSequence->MovieScene->GetSpawnable(x);
		FString Name = LevelSequence->MovieScene->GetObjectDisplayName(Spawnable.GetGuid()).ToString();
		if (Name == SpawnableName)
		{
			Guid = Spawnable.GetGuid();
			break;
		}
	}

	if (!Guid.IsValid())
	{
		return FGuid();
	}

	return Guid;
}

FGuid USequenceBPLib::AddSpawnableToSequence(const FString& LevelSeqPath,
                                             const FString& AssetPath,
                                             const FString& SpawnableName)
{
	FGuid Guid = GetSpawnableGuidFromSequence(LevelSeqPath, SpawnableName);
	if (Guid.IsValid())
	{
		return Guid;
	}

	UObject* SpawnableTemplate = Cast<UObject>(StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath));
	if (SpawnableTemplate == nullptr)
	{
		return FGuid();
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return FGuid();
	}

	Guid = Cast<UMovieSceneSequence>(LevelSequence)->CreateSpawnable(SpawnableTemplate);
	if (!Guid.IsValid())
	{
		return FGuid();
	}

	LevelSequence->MovieScene->SetObjectDisplayName(Guid, FText::FromString(SpawnableName));

	return Guid;
}

FGuid USequenceBPLib::AddSpawnableToSequenceByObject(const FString& LevelSeqPath,
                                                     UObject* Object,
                                                     const FString& SpawnableName)
{
	FGuid Guid = GetSpawnableGuidFromSequence(LevelSeqPath, SpawnableName);
	if (Guid.IsValid())
	{
		return Guid;
	}

	if (Object == nullptr)
	{
		return FGuid();
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return FGuid();
	}

	Guid = Cast<UMovieSceneSequence>(LevelSequence)->CreateSpawnable(Object);
	if (!Guid.IsValid())
	{
		return FGuid();
	}

	LevelSequence->MovieScene->SetObjectDisplayName(Guid, FText::FromString(SpawnableName));

	return Guid;
}

bool USequenceBPLib::RemoveSpawnableFromSequence(const FString& LevelSeqPath, const FString& SpawnableName)
{
	// 检查 Actor 是否在 Sequence 中
	FGuid Guid = GetSpawnableGuidFromSequence(LevelSeqPath, SpawnableName);
	if (!Guid.IsValid())
	{
		return false;
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return false;
	}

	if (!LevelSequence->MovieScene->RemoveSpawnable(Guid))
	{
		return false;
	}
	return true;
}

FGuid USequenceBPLib::GetPossessableGuidFromSeq(const FString& LevelSeqPath, AActor* Actor)
{
	// validate actor
	if (Actor == nullptr)
	{
		return FGuid();
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return FGuid();
	}

	FGuid Guid = LevelSequence->FindBindingFromObject(Actor, Actor->GetWorld());
	if (!Guid.IsValid())
	{
		return FGuid();
	}

	return Guid;
}

FGuid USequenceBPLib::GetPossessableGuidFromSeqUObject(const FString& LevelSeqPath, UObject* Object)
{
	// validate actor
	if (Object == nullptr)
	{
		return FGuid();
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return FGuid();
	}

	FGuid Guid = LevelSequence->FindBindingFromObject(Object, Object->GetWorld());
	if (!Guid.IsValid())
	{
		return FGuid();
	}

	return Guid;
}

FGuid USequenceBPLib::AddPossessableToSequence(const FString& LevelSeqPath, AActor* Actor)
{
	FGuid Guid = GetPossessableGuidFromSeq(LevelSeqPath, Actor);
	if (Guid.IsValid())
	{
		return Guid;
	}

	// validate actor
	if (Actor == nullptr)
	{
		return FGuid();
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return FGuid();
	}

	Guid = Cast<UMovieSceneSequence>(LevelSequence)->CreatePossessable(Actor);
	if (!Guid.IsValid())
	{
		return FGuid();
	}

	return Guid;
}

FGuid USequenceBPLib::AddPossessableToSequenceUObject(const FString& LevelSeqPath, UObject* Object)
{
	FGuid Guid = GetPossessableGuidFromSeqUObject(LevelSeqPath, Object);
	if (Guid.IsValid())
	{
		return Guid;
	}

	// validate actor
	if (Object == nullptr)
	{
		return FGuid();
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return FGuid();
	}

	Guid = Cast<UMovieSceneSequence>(LevelSequence)->CreatePossessable(Object);
	if (!Guid.IsValid())
	{
		return FGuid();
	}

	return Guid;
}

bool USequenceBPLib::RemovePossessableFromSequence(const FString& LevelSeqPath, AActor* Actor)
{
	// 检查 Actor 是否在 Sequence 中
	FGuid Guid = GetPossessableGuidFromSeq(LevelSeqPath, Actor);
	if (!Guid.IsValid())
	{
		return false;
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		return false;
	}

	// Unbind and Delete Track
	LevelSequence->UnbindPossessableObjects(Guid);
	if (!LevelSequence->MovieScene->RemovePossessable(Guid))
	{
		return false;
	}

	return true;
}

#pragma endregion

#pragma region AddAnimAudio

bool USequenceBPLib::AddAnimationToLevelSequence(const FString& LevelSeqPath,
                                                 const FString& AnimPath,
                                                 const FGuid& ActorGuid,
                                                 int32 StartFrame,
                                                 int32 EndFrame,
                                                 bool bUseAutoEase,
                                                 float EaseInTime,
                                                 float EaseOutTime)
{
	if (!ActorGuid.IsValid() || AnimPath.IsEmpty())
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("AddAnimationToLevelSequence: Guid Invalid - %s"), *LevelSeqPath);
		return false;
	}

	// Load Anim
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(),
	                                                                   nullptr, *AnimPath));
	if (AnimSequence == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("AddAnimationToLevelSequence: Anim Sequence File invalid - %s"),
		       *AnimPath);
		return false;
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("AddAnimationToLevelSequence: LevelSequence invalid - %s"), *LevelSeqPath);
		return false;
	}

	// 如果添加动画的时间范围与已有动画冲突，UE 会自动新建动画 Section，因此不用担心动画重叠
	auto AnimTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneSkeletalAnimationTrack>(ActorGuid);
	if (AnimTrack == nullptr)
	{
		AnimTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(ActorGuid);
		if (AnimTrack == nullptr)
		{
			UE_LOG(AutoSequencerBPLib, Error,
			       TEXT("AddAnimationToLevelSequence: Failed To Add Anim Track - %s"), *LevelSeqPath);
			return false;
		}
	}

	// Add Anim Section
	int FrameTickValue = LevelSequence->MovieScene->GetTickResolution().AsDecimal() / LevelSequence->MovieScene->
		GetDisplayRate().AsDecimal();
	auto Section = AnimTrack->AddNewAnimation(FFrameNumber(StartFrame * FrameTickValue), AnimSequence);
	if (Section == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("AddAnimationToLevelSequence: Failed To Add Anim Section - %s"), *LevelSeqPath);
		return false;
	}

	// Add end frame
	if (EndFrame > StartFrame)
	{
		Section->SetEndFrame(FFrameNumber(EndFrame * FrameTickValue));
	}

	if (bUseAutoEase)
	{
		FMovieSceneEasingSettings EasingSettings;
		EasingSettings.AutoEaseInDuration = EaseInTime;
		EasingSettings.AutoEaseOutDuration = EaseOutTime;
		Section->Easing = EasingSettings;
	}

	AnimTrack->Modify();
	return true;
}

float USequenceBPLib::AddAudioToLevelSequence(const FString& LevelSeqPath,
                                              const FString& AudioPath,
                                              int32 StartFrame,
                                              int32 EndFrame)
{
	if (AudioPath.IsEmpty())
	{
		return 0;
	}

	// Load Anim
	USoundWave* SoundWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(),
	                                                          nullptr, *AudioPath));
	if (SoundWave == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("AddAudioToLevelSequence: Sound Wave File invalid - %s"), *AudioPath);
		return 0;
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("AddAudioToLevelSequence: LevelSequence invalid - %s"), *LevelSeqPath);
		return 0;
	}

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	auto SoundTrack = LevelSequence->MovieScene->FindMasterTrack<UMovieSceneAudioTrack>();
#else
	auto SoundTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneAudioTrack>();
#endif

	if (SoundTrack == nullptr)
	{
#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
		SoundTrack = LevelSequence->MovieScene->AddMasterTrack<UMovieSceneAudioTrack>();
#else
		SoundTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneAudioTrack>();
#endif

		if (SoundTrack == nullptr)
		{
			UE_LOG(AutoSequencerBPLib, Error,
			       TEXT("AddAudioToLevelSequence: Failed To Add Audio Track - %s"), *LevelSeqPath);
			return 0;
		}
	}

	// Add Anim Section
	int FrameTickValue = LevelSequence->MovieScene->GetTickResolution().AsDecimal() / LevelSequence->MovieScene->
		GetDisplayRate().AsDecimal();
	auto Section = SoundTrack->AddNewSound(SoundWave, FFrameNumber(StartFrame * FrameTickValue));
	if (Section == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("AddAudioToLevelSequence: Failed To Add Audio Section - %s"), *LevelSeqPath);
		return 0;
	}

	// Add end frame
	if (EndFrame > StartFrame)
	{
		Section->SetEndFrame(FFrameNumber(EndFrame * FrameTickValue));
	}

	SoundTrack->Modify();
	return SoundWave->Duration;
}

float USequenceBPLib::AddSoundWaveToLevelSequence(const FString& LevelSeqPath, USoundWave* SoundWave, int32 StartFrame,
                                                  int32 EndFrame)
{
	if (SoundWave == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("AddAudioToLevelSequence: Sound Wave invalid"));
		return 0;
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("AddAudioToLevelSequence: LevelSequence invalid - %s"), *LevelSeqPath);
		return 0;
	}

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	auto SoundTrack = LevelSequence->MovieScene->FindMasterTrack<UMovieSceneAudioTrack>();
#else
	auto SoundTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneAudioTrack>();
#endif

	if (SoundTrack == nullptr)
	{
#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
		SoundTrack = LevelSequence->MovieScene->AddMasterTrack<UMovieSceneAudioTrack>();
#else
		SoundTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneAudioTrack>();
#endif

		if (SoundTrack == nullptr)
		{
			UE_LOG(AutoSequencerBPLib, Error,
			       TEXT("AddAudioToLevelSequence: Failed To Add Audio Track - %s"), *LevelSeqPath);
			return 0;
		}
	}

	// Add Anim Section
	int FrameTickValue = LevelSequence->MovieScene->GetTickResolution().AsDecimal() / LevelSequence->MovieScene->
		GetDisplayRate().AsDecimal();
	auto Section = SoundTrack->AddNewSound(SoundWave, FFrameNumber(StartFrame * FrameTickValue));
	if (Section == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("AddAudioToLevelSequence: Failed To Add Audio Section - %s"), *LevelSeqPath);
		return 0;
	}

	// Add end frame
	if (EndFrame > StartFrame)
	{
		Section->SetEndFrame(FFrameNumber(EndFrame * FrameTickValue));
	}

	SoundTrack->Modify();
	return SoundWave->Duration;
}

#pragma endregion
