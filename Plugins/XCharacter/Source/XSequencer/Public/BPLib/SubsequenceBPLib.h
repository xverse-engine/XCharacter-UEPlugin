// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SubsequenceBPLib.generated.h"

class UMovieSceneCinematicShotTrack;
class UMovieSceneSubTrack;

/**
 * 
 */
UCLASS()
class XSEQUENCER_API USubsequenceBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region Shots

	/**
	 * 获取 Sequence 中的 Shot Track，一个 Sequence 中只会有一个 Shot Track
	 *
	 * @param LevelSeqPath Sequence 路径
	 *
	 * @return 获取到的 Shot Track，如果获取失败则返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Shots")
	static UMovieSceneCinematicShotTrack* GetShotTrackFromSequence(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Shots")
	static UMovieSceneCinematicShotTrack* AddShotTrackToSequence(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Shots")
	static bool RemoveShotTrackFromSequence(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Shots")
	static bool LinkShotToShotTrack(const FString& LevelSeqPath,
	                                const FString& ShotPath,
	                                int32 StartFrame,
	                                int32 EndFrame);

#pragma endregion

#pragma region Subsequence

	/**
 	 * 获取 Sequence 中的 Subsequence Track，一个 Sequence 中只会有一个 Subsequence Track
 	 *
 	 * @param LevelSeqPath Sequence 路径
 	 *
 	 * @return 获取到的 Subsequence Track，如果获取失败则返回 nullptr
 	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Subsequence")
	static UMovieSceneSubTrack* GetSubsequenceTrackFromSequence(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Subsequence")
	static UMovieSceneSubTrack* AddSubsequenceTrackToSequence(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Subsequence")
	static bool RemoveSubsequenceTrackFromSequence(const FString& LevelSeqPath);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Subsequence")
	static bool LinkSequenceToSubsequenceTrack(const FString& LevelSeqPath,
	                                           const FString& SubSequencePath,
	                                           int32 StartFrame,
	                                           int32 EndFrame);

#pragma endregion
};
