// Fill out your copyright notice in the Description page of Project Settings.

#include "BPLib/SequenceUtilsBPLib.h"
#include "XSequencerDefines.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"

#pragma region Folder

UMovieSceneFolder* USequenceUtilsBPLib::GetFolderFromSequence(const FString& LevelSeqPath, const FString& FolderName)
{
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error, TEXT("GetFolderFromSequence - LevelSequence invalid - %s"), *LevelSeqPath);
		return nullptr;
	}

	UMovieSceneFolder* ExistingFolder{};

	for (const auto Folder : LevelSequence->MovieScene->GetRootFolders())
	{
		if (Folder->GetFolderName().ToString() == FolderName)
		{
			ExistingFolder = Folder;
			break;
		}
	}

	UE_LOG(AutoSequencerBPLib, Warning,
	       TEXT("GetFolderFromSequence - Result: Got Folder - %s - from Level Sequence - %s"),
	       *FolderName, *LevelSeqPath);
	return ExistingFolder;
}

UMovieSceneFolder* USequenceUtilsBPLib::AddFolderToSequence(const FString& LevelSeqPath, const FString& FolderName)
{
	auto Folder = GetFolderFromSequence(LevelSeqPath, FolderName);
	if (Folder != nullptr)
	{
		return Folder;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));

	Folder = NewObject<UMovieSceneFolder>(LevelSequence->MovieScene, NAME_None, RF_Transactional);
	Folder->SetFolderName(*FolderName);

	LevelSequence->MovieScene->AddRootFolder(Folder);

	UE_LOG(AutoSequencerBPLib, Warning,
	       TEXT("AddFolderToSequence - Successfully Added Folder - %s - from Level Sequence - %s"),
	       *FolderName, *LevelSeqPath);
	return Folder;
}

bool USequenceUtilsBPLib::RemoveFolderFromSequence(const FString& LevelSeqPath, const FString& FolderName)
{
	auto Folder = GetFolderFromSequence(LevelSeqPath, FolderName);
	if (Folder == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("RemoveFolderFromSequence - Unable to Find Folder - %s - in - %s"),
		       *FolderName, *LevelSeqPath);
		return false;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	LevelSequence->MovieScene->RemoveRootFolder(Folder);

	UE_LOG(AutoSequencerBPLib, Warning,
	       TEXT("RemoveFolderFromSequence - Successfully Removed Folder - %s - from Level Sequence - %s"),
	       *FolderName, *LevelSeqPath);
	return true;
}

bool USequenceUtilsBPLib::MoveActorIntoFolderInSequence(const FString& LevelSeqPath, const FString& FolderName,
                                                        const FGuid& ActorGuid)
{
	auto Folder = GetFolderFromSequence(LevelSeqPath, FolderName);
	if (Folder == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("MoveActorIntoFolderInSequence - Unable to Find Folder - %s - in - %s"),
		       *FolderName, *LevelSeqPath);
		return false;
	}

	if (!ActorGuid.IsValid())
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("MoveActorIntoFolderInSequence - Actor Guid Not Valid - %s"),
		       *LevelSeqPath);
		return false;
	}

	if (Folder->GetChildObjectBindings().Contains(ActorGuid))
	{
		return true;
	}

	Folder->AddChildObjectBinding(ActorGuid);
	UE_LOG(AutoSequencerBPLib, Warning,
	       TEXT("MoveActorIntoFolderInSequence - Successfully Moved Actor %s into Folder %s from Level Sequence - %s"),
	       *ActorGuid.ToString(), *FolderName, *LevelSeqPath);
	return true;
}

bool USequenceUtilsBPLib::MoveActorOutOfFolderInSequence(const FString& LevelSeqPath, const FString& FolderName,
                                                         const FGuid& ActorGuid)
{
	auto Folder = GetFolderFromSequence(LevelSeqPath, FolderName);
	if (Folder == nullptr)
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("MoveActorOutOfFolderInSequence - Unable to Find Folder - %s - in - %s"),
		       *FolderName, *LevelSeqPath);
		return false;
	}

	if (!ActorGuid.IsValid())
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("MoveActorOutOfFolderInSequence - Actor Guid Not Valid - %s"),
		       *LevelSeqPath);
		return false;
	}

	if (!Folder->GetChildObjectBindings().Contains(ActorGuid))
	{
		UE_LOG(AutoSequencerBPLib, Error,
		       TEXT("MoveActorOutOfFolderInSequence - Unable to find Actor in Folder - %s - %s"),
		       *FolderName, *LevelSeqPath);
		return false;
	}

	Folder->RemoveChildObjectBinding(ActorGuid);
	UE_LOG(AutoSequencerBPLib, Warning,
	       TEXT("MoveActorOutOfFolderInSequence - Successfully Moved Actor %s Out of Folder %s from Level Sequence - %s"
	       ),
	       *ActorGuid.ToString(), *FolderName, *LevelSeqPath);
	return true;
}

#pragma endregion
