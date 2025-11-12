// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequenceUtilsBPLib.generated.h"

class UMovieSceneFolder;

/**
 * 
 */
UCLASS()
class XSEQUENCER_API USequenceUtilsBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region Folder

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Folder")
	static UMovieSceneFolder* GetFolderFromSequence(const FString& LevelSeqPath,
	                                                const FString& FolderName);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Folder")
	static UMovieSceneFolder* AddFolderToSequence(const FString& LevelSeqPath,
	                                              const FString& FolderName);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Folder")
	static bool RemoveFolderFromSequence(const FString& LevelSeqPath,
	                                     const FString& FolderName);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Folder")
	static bool MoveActorIntoFolderInSequence(const FString& LevelSeqPath,
	                                          const FString& FolderName,
	                                          const FGuid& ActorGuid);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Folder")
	static bool MoveActorOutOfFolderInSequence(const FString& LevelSeqPath,
	                                           const FString& FolderName,
	                                           const FGuid& ActorGuid);

#pragma endregion
};
