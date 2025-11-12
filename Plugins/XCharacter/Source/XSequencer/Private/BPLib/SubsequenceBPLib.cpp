// Fill out your copyright notice in the Description page of Project Settings.

#include "BPLib/SubsequenceBPLib.h"
#include "XSequencerDefines.h"

#include "LevelSequence.h"
#include "MovieScene.h"

#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

DEFINE_LOG_CATEGORY(SubSequenceBPLib);

#pragma region Shots

UMovieSceneCinematicShotTrack* USubsequenceBPLib::GetShotTrackFromSequence(const FString& LevelSeqPath)
{
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SubSequenceBPLib, Error, TEXT("GetShotTrackFromSequence - LevelSequence invalid - %s"), *LevelSeqPath);
		return nullptr;
	}

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	UMovieSceneCinematicShotTrack* ShotTrack = LevelSequence->MovieScene
	                                                        ->FindMasterTrack<UMovieSceneCinematicShotTrack>();
#else
	UMovieSceneCinematicShotTrack* ShotTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneCinematicShotTrack>();
#endif

	return ShotTrack;
}

UMovieSceneCinematicShotTrack* USubsequenceBPLib::AddShotTrackToSequence(const FString& LevelSeqPath)
{
	UMovieSceneCinematicShotTrack* ShotTrack = GetShotTrackFromSequence(LevelSeqPath);
	if (ShotTrack)
	{
		return ShotTrack;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SubSequenceBPLib, Error, TEXT("AddShotTrackToSequence - LevelSequence invalid - %s"), *LevelSeqPath);
		return nullptr;
	}

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	ShotTrack = LevelSequence->MovieScene->AddMasterTrack<UMovieSceneCinematicShotTrack>();
#else
	ShotTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneCinematicShotTrack>();
#endif

	if (!ShotTrack)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("AddShotTrackToSequence - Unable to Add Shot Track in Sequence - %s"),
		       *LevelSeqPath);
	}
	else
	{
		UE_LOG(SubSequenceBPLib, Warning,
		       TEXT("AddShotTrackToSequence - Successfully Added Shot Track to Sequence - %s"),
		       *LevelSeqPath);
	}

	return ShotTrack;
}

bool USubsequenceBPLib::RemoveShotTrackFromSequence(const FString& LevelSeqPath)
{
	UMovieSceneCinematicShotTrack* ShotTrack = GetShotTrackFromSequence(LevelSeqPath);
	if (!ShotTrack)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("RemoveShotTrackFromSequence - Unable To Find Shot Trackin Sequence - %s"),
		       *LevelSeqPath);
		return false;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	LevelSequence->MovieScene->RemoveTrack(*ShotTrack);

	UE_LOG(SubSequenceBPLib, Warning,
	       TEXT("RemoveShotTrackFromSequence - Successfully Removed Shot Track From Sequence - %s"),
	       *LevelSeqPath);
	return true;
}

bool USubsequenceBPLib::LinkShotToShotTrack(const FString& LevelSeqPath,
                                            const FString& ShotPath,
                                            int32 StartFrame,
                                            int32 EndFrame)
{
	ULevelSequence* Shot = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                             nullptr, *ShotPath));
	if (!Shot)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("LinkShotToShotTrack - Shot - %s - Invalid"), *ShotPath);
		return false;
	}

	UMovieSceneCinematicShotTrack* ShotTrack = GetShotTrackFromSequence(LevelSeqPath);
	if (!ShotTrack)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("LinkShotToShotTrack - Error During Get Shot Track - %s"),
		       *LevelSeqPath);
		return false;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	int32 TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal() /
		LevelSequence->MovieScene->GetDisplayRate().AsDecimal();

	UMovieSceneSubSection* SubSection = ShotTrack->AddSequence(Shot, FFrameNumber{StartFrame * TicksPerFrame},
	                                                           (EndFrame - StartFrame) * TicksPerFrame);

	if (!SubSection)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("LinkShotToShotTrack - Unable To Link  Shot To Shot Track in Sequence - %s"),
		       *LevelSeqPath);
		return false;
	}

	UE_LOG(SubSequenceBPLib, Warning,
	       TEXT("LinkShotToShotTrack - Successfully Linked Sequence Shot To Shot Track in Sequence - %s"),
	       *LevelSeqPath);
	return true;
}

#pragma endregion

#pragma region Subsequence

UMovieSceneSubTrack* USubsequenceBPLib::GetSubsequenceTrackFromSequence(const FString& LevelSeqPath)
{
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SubSequenceBPLib, Error, TEXT("GetSubsequenceTrackFromSequence - LevelSequence invalid - %s"),
		       *LevelSeqPath);
		return nullptr;
	}

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	UMovieSceneSubTrack* SubsequenceTrack = LevelSequence->MovieScene->FindMasterTrack<UMovieSceneSubTrack>();
#else
	UMovieSceneSubTrack* SubsequenceTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneSubTrack>();
#endif

	UE_LOG(SubSequenceBPLib, Warning,
	       TEXT("GetSubsequenceTrackFromSequence - Got Subsequence Track from Level Sequence - %s"), *LevelSeqPath);

	return SubsequenceTrack;
}

UMovieSceneSubTrack* USubsequenceBPLib::AddSubsequenceTrackToSequence(const FString& LevelSeqPath)
{
	UMovieSceneSubTrack* SubSequenceTrack = GetSubsequenceTrackFromSequence(LevelSeqPath);
	if (SubSequenceTrack)
	{
		return SubSequenceTrack;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr)
	{
		UE_LOG(SubSequenceBPLib, Error, TEXT("AddSubsequenceTrackToSequence - LevelSequence invalid - %s"),
		       *LevelSeqPath);
		return nullptr;
	}

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	SubSequenceTrack = LevelSequence->MovieScene->AddMasterTrack<UMovieSceneSubTrack>();
#else
	SubSequenceTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneSubTrack>();
#endif

	if (!SubSequenceTrack)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("AddSubsequenceTrackToSequence - Unable to Add Shot Track in Sequence - %s"),
		       *LevelSeqPath);
	}
	else
	{
		UE_LOG(SubSequenceBPLib, Warning,
		       TEXT("AddSubsequenceTrackToSequence - Successfully Added Shot Track to Sequence - %s"),
		       *LevelSeqPath);
	}

	return SubSequenceTrack;
}

bool USubsequenceBPLib::RemoveSubsequenceTrackFromSequence(const FString& LevelSeqPath)
{
	UMovieSceneSubTrack* SubsequenceTrack = GetSubsequenceTrackFromSequence(LevelSeqPath);
	if (!SubsequenceTrack)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("RemoveSubsequenceTrackFromSequence - Unable To Find Shot Trackin Sequence - %s"),
		       *LevelSeqPath);
		return false;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	LevelSequence->MovieScene->RemoveTrack(*SubsequenceTrack);

	UE_LOG(SubSequenceBPLib, Warning,
	       TEXT("RemoveSubsequenceTrackFromSequence - Successfully Removed Shot Track From Sequence - %s"),
	       *LevelSeqPath);
	return true;
}

bool USubsequenceBPLib::LinkSequenceToSubsequenceTrack(const FString& LevelSeqPath, const FString& SubSequencePath,
                                                       int32 StartFrame, int32 EndFrame)
{
	ULevelSequence* SubSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                    nullptr, *SubSequencePath));
	if (!SubSequence)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("LinkSequenceToSubsequenceTrack - SubSequence - %s - Invalid - %s"),
		       *SubSequence->GetName(), *LevelSeqPath);
		return false;
	}

	UMovieSceneSubTrack* SubsequenceTrack = GetSubsequenceTrackFromSequence(LevelSeqPath);
	if (!SubsequenceTrack)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT("LinkSequenceToSubsequenceTrack - Error During Get SubSequence Track - %s"),
		       *LevelSeqPath);
		return false;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	const int32 TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal() /
		LevelSequence->MovieScene->GetDisplayRate().AsDecimal();

	UMovieSceneSubSection* SubSection = SubsequenceTrack->AddSequence(SubSequence,
	                                                                  FFrameNumber{StartFrame * TicksPerFrame},
	                                                                  (EndFrame - StartFrame) * TicksPerFrame);

	if (!SubSection)
	{
		UE_LOG(SubSequenceBPLib, Error,
		       TEXT(
			       "LinkSequenceToSubsequenceTrack - Unable To Link SubSequence To SubSequence Track in Sequence - %s"
		       ),
		       *LevelSeqPath);
		return false;
	}

	UE_LOG(SubSequenceBPLib, Warning,
	       TEXT(
		       "LinkSequenceToSubsequenceTrack - Successfully Linked SubSequence To SubSequence Track in Sequence - %s"
	       ),
	       *LevelSeqPath);
	return true;
}

#pragma endregion
