// Fill out your copyright notice in the Description page of Project Settings.

#include "BPLib/TransformAndKeyframeBPLib.h"

#include "XSequencerDefines.h"
#include "LevelSequence.h"

// MovieScene
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Materials/MaterialParameterCollection.h"

// Tracks
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Tracks/MovieSceneMaterialParameterCollectionTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneBoolTrack.h"

// Channels
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneBoolChannel.h"

// Sections
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"

#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "Curves/KeyHandle.h"

#include "Materials/MaterialInstanceConstant.h"

// Blends
#include "Evaluation/Blending/MovieSceneBlendType.h"

DEFINE_LOG_CATEGORY(SeqTransformBPLib);

#pragma region TransformTrack

UMovieScene3DTransformTrack* UTransformAndKeyframeBPLib::GetTransformTrackFromActor(
	FString LevelSeqPath, FGuid ActorGuid)
{
	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));

	// Check validity of Actor Guid
	if (LevelSequence == nullptr || !ActorGuid.IsValid())
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("GetTransformTrackFromActor - Actor GUID %s Or Sequence Invalid - %s"),
		       *ActorGuid.ToString(), *LevelSeqPath);
		return nullptr;
	}

	// Get the transform track
	UMovieScene3DTransformTrack* TransformTrack = LevelSequence->MovieScene->FindTrack<
		UMovieScene3DTransformTrack>(ActorGuid);

	return TransformTrack;
}

UMovieScene3DTransformTrack* UTransformAndKeyframeBPLib::AddTransformTrackToActor(FString LevelSeqPath, FGuid ActorGuid)
{
	// Check if there is a transform track in the level sequence
	UMovieScene3DTransformTrack* TransformTrack = GetTransformTrackFromActor(LevelSeqPath, ActorGuid);

	if (TransformTrack != nullptr)
	{
		return TransformTrack;
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr || !ActorGuid.IsValid())
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("AddTransformTrack - Actor GUID %s Or Sequence Invalid - %s"),
		       *ActorGuid.ToString(), *LevelSeqPath);
		return nullptr;
	}

	// Add transform track
	TransformTrack = LevelSequence->MovieScene->AddTrack<UMovieScene3DTransformTrack>(ActorGuid);
	return TransformTrack;
}

bool UTransformAndKeyframeBPLib::RemoveTransformTrackFromActor(FString LevelSeqPath, FGuid ActorGuid)
{
	// Check if there is a transform track in the level sequence
	UMovieScene3DTransformTrack* TransformTrack = GetTransformTrackFromActor(LevelSeqPath, ActorGuid);

	if (TransformTrack == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("GetTransformSection - Transform Track of Actor %s is invalid - '%s'"),
		       *ActorGuid.ToString(), *LevelSeqPath);
		return false;
	}

	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));

	// Remove transform track
	LevelSequence->MovieScene->RemoveTrack(*TransformTrack);

	return true;
}

#pragma endregion

#pragma region KeyFrame

UMovieScene3DTransformSection* UTransformAndKeyframeBPLib::GetTransformSectionInActor(FString LevelSeqPath,
	FGuid ActorGuid,
	int SectionIndex)
{
	// Check if there is a transform track in the level sequence
	UMovieScene3DTransformTrack* TransformTrack = GetTransformTrackFromActor(LevelSeqPath, ActorGuid);

	if (TransformTrack == nullptr)
	{
		return nullptr;
	}

	// Get all sections
	TArray<UMovieSceneSection*> AllSections = TransformTrack->GetAllSections();

	// Check validity of the index
	if (SectionIndex < 0 || SectionIndex >= AllSections.Num())
	{
		return nullptr;
	}

	return Cast<UMovieScene3DTransformSection>(AllSections[SectionIndex]);
}

UMovieScene3DTransformSection* UTransformAndKeyframeBPLib::AddTransformSectionToActor(FString LevelSeqPath,
	FGuid ActorGuid,
	int StartFrame,
	int EndFrame,
	EMovieSceneBlendType BlendType)
{
	// Check if there is a transform track in the level sequence
	UMovieScene3DTransformTrack* TransformTrack = GetTransformTrackFromActor(LevelSeqPath, ActorGuid);

	if (TransformTrack == nullptr)
	{
		return nullptr;
	}

	// Create a section
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(
		TransformTrack->CreateNewSection());

	if (TransformSection == nullptr)
	{
		return nullptr;
	}

	// Modify the transform section
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(),
	                                                                      nullptr, *LevelSeqPath));
	if (!LevelSequence)
	{
		UE_LOG(SeqTransformBPLib, Error,
		       TEXT("AddTransformSectionToActor - Level Sequence Invalid - %s"),
		       *LevelSeqPath);
		return nullptr;
	}

	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal() /
		LevelSequence->MovieScene->GetDisplayRate().AsDecimal();

	// Set frame range of the section
	TransformSection->SetRange(TRange<FFrameNumber>(FFrameNumber(StartFrame * TicksPerFrame),
	                                                FFrameNumber(EndFrame * TicksPerFrame)));

	// Set blend type
	TransformSection->SetBlendType(BlendType);

	// Place the section on a new row so it does not fight against one another
	int RowIndex = -1;
	for (UMovieSceneSection* ExistingSection : TransformTrack->GetAllSections())
	{
		RowIndex = FMath::Max(RowIndex, ExistingSection->GetRowIndex());
	}
	TransformSection->SetRowIndex(RowIndex + 1);

	// Add section to track
	TransformTrack->AddSection(*TransformSection);

	return TransformSection;
}

bool UTransformAndKeyframeBPLib::RemoveTransformSectionFromActor(FString LevelSeqPath,
                                                                 FGuid ActorGuid,
                                                                 int SectionIndex)
{
	// Get the transform section
	UMovieScene3DTransformSection* TransformSection = GetTransformSectionInActor(
		LevelSeqPath, ActorGuid, SectionIndex);

	if (TransformSection == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("AddTransformSection - Transform Section is invalid - '%s'"),
		       *LevelSeqPath);
		return false;
	}

	// Get transform track
	UMovieScene3DTransformTrack* TransformTrack = GetTransformTrackFromActor(LevelSeqPath, ActorGuid);

	// Remove section
	TransformTrack->RemoveSection(*TransformSection);

	// Refresh level sequence
	TransformTrack->Modify();

	return true;
}

bool UTransformAndKeyframeBPLib::AddTransformKeyframeForActor(FString LevelSeqPath,
                                                              FGuid ActorGuid,
                                                              int SectionIndex,
                                                              int Frame,
                                                              FTransform Transform,
                                                              int KeyInterpolation)
{
	// Get the transform section
	UMovieScene3DTransformSection* TransformSection = GetTransformSectionInActor(
		LevelSeqPath, ActorGuid, SectionIndex);

	if (TransformSection == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("AddTransformKeyframe - Transform section is invalid - '%s'"),
		       *LevelSeqPath);
		return false;
	}

	// Remove existing keyframe at the same place
	RemoveTransformKeyframeForActor(LevelSeqPath, ActorGuid, SectionIndex, Frame);

	// Add Location
	AddKeyframeToDoubleChannel(TransformSection, 0, Frame, Transform.GetLocation().X, KeyInterpolation);
	AddKeyframeToDoubleChannel(TransformSection, 1, Frame, Transform.GetLocation().Y, KeyInterpolation);
	AddKeyframeToDoubleChannel(TransformSection, 2, Frame, Transform.GetLocation().Z, KeyInterpolation);

	// Add Rotation
	AddKeyframeToDoubleChannel(TransformSection, 3, Frame, Transform.Rotator().Roll, KeyInterpolation);
	AddKeyframeToDoubleChannel(TransformSection, 4, Frame, Transform.Rotator().Pitch, KeyInterpolation);
	AddKeyframeToDoubleChannel(TransformSection, 5, Frame, Transform.Rotator().Yaw, KeyInterpolation);

	// Add Scale
	AddKeyframeToDoubleChannel(TransformSection, 6, Frame, Transform.GetScale3D().X, KeyInterpolation);
	AddKeyframeToDoubleChannel(TransformSection, 7, Frame, Transform.GetScale3D().Y, KeyInterpolation);
	AddKeyframeToDoubleChannel(TransformSection, 8, Frame, Transform.GetScale3D().Z, KeyInterpolation);

	return true;
}

bool UTransformAndKeyframeBPLib::RemoveTransformKeyframeForActor(FString LevelSeqPath,
                                                                 FGuid ActorGuid,
                                                                 int SectionIndex,
                                                                 int Frame)
{
	// Get the transform section
	UMovieScene3DTransformSection* TransformSection = GetTransformSectionInActor(
		LevelSeqPath, ActorGuid, SectionIndex);

	if (TransformSection == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("RemoveTransformKeyframe - Transform section is invalid - '%s'"),
		       *LevelSeqPath);
		return false;
	}

	// Remove Location
	RemoveKeyframeFromDoubleChannel(TransformSection, 0, Frame);
	RemoveKeyframeFromDoubleChannel(TransformSection, 1, Frame);
	RemoveKeyframeFromDoubleChannel(TransformSection, 2, Frame);

	// Remove Rotation
	RemoveKeyframeFromDoubleChannel(TransformSection, 3, Frame);
	RemoveKeyframeFromDoubleChannel(TransformSection, 4, Frame);
	RemoveKeyframeFromDoubleChannel(TransformSection, 5, Frame);

	// Remove Scale
	RemoveKeyframeFromDoubleChannel(TransformSection, 6, Frame);
	RemoveKeyframeFromDoubleChannel(TransformSection, 7, Frame);
	RemoveKeyframeFromDoubleChannel(TransformSection, 8, Frame);

	return true;
}

bool UTransformAndKeyframeBPLib::AddKeyframeToDoubleChannel(UMovieSceneSection* Section,
                                                            int ChannelIndex,
                                                            int Frame,
                                                            double Value,
                                                            int KeyInterpolation)
{
	// Check the validity of input section
	if (Section == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("AddKeyframeToDoubleChannel - Transform section is invalid."));
		return false;
	}

	// Get the channel
	FMovieSceneDoubleChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(ChannelIndex);
	if (Channel == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("AddKeyframeToDoubleChannel - Channel %d is invalid."), ChannelIndex);
		return false;
	}

	// Compute the actual frame number to add a key
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(Section->GetOutermostObject());
	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal() / LevelSequence->MovieScene->
		GetDisplayRate().AsDecimal();
	FFrameNumber FrameNumber = FFrameNumber(Frame * TicksPerFrame);

	// Add key to channel
	if (KeyInterpolation == 0)
	{
		Channel->AddCubicKey(FrameNumber, Value);
	}
	else if (KeyInterpolation == 1)
	{
		Channel->AddLinearKey(FrameNumber, Value);
	}
	else
	{
		Channel->AddConstantKey(FrameNumber, Value);
	}

	// Refresh the section
	Section->Modify();

	return true;
}

bool UTransformAndKeyframeBPLib::RemoveKeyframeFromDoubleChannel(UMovieSceneSection* Section,
                                                                 int ChannelIndex,
                                                                 int Frame)
{
	// Check the validity of input section
	if (Section == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("RemoveKeyframeFromDoubleChannel - Transform section is invalid."));
		return false;
	}

	// Get the channel
	FMovieSceneDoubleChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(ChannelIndex);
	if (Channel == nullptr)
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("RemoveKeyframeFromDoubleChannel - Channel %d is invalid."),
		       ChannelIndex);
		return false;
	}

	// Compute the actual frame number to remove a key
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(Section->GetOutermostObject());
	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal() / LevelSequence->MovieScene->
		GetDisplayRate().AsDecimal();
	FFrameNumber FrameNumber = FFrameNumber(Frame * TicksPerFrame);

	// Find the key to remove
	TArray<FFrameNumber> UnusedKeyTimes;
	TArray<FKeyHandle> KeyHandles;
	Channel->GetKeys(TRange<FFrameNumber>(FrameNumber, FrameNumber), &UnusedKeyTimes, &KeyHandles);

	// Remove key
	Channel->DeleteKeys(KeyHandles);

	// Refresh the section
	Section->Modify();
	return true;
}

#pragma endregion

#pragma region Interpolation

bool UTransformAndKeyframeBPLib::SetKeyFrameInterpInTransformTrack(const FString& LevelSeqPath,
                                                                   const FGuid& ActorGuid,
                                                                   int SectionIndex,
                                                                   int ChannelIndex,
                                                                   int Frame,
                                                                   float Value,
                                                                   ERichCurveInterpMode InterpMode,
                                                                   ERichCurveTangentMode TangentMode,
                                                                   float ArriveTangent,
                                                                   float LeaveTangent)
{
	auto Section = GetTransformSectionInActor(LevelSeqPath, ActorGuid, SectionIndex);
	if (!Section)
	{
		UE_LOG(SeqTransformBPLib, Error,
		       TEXT("SetKeyFrameInterpInTransformTrack - Section Index %d is invalid in Sequence - %s"),
		       SectionIndex, *LevelSeqPath);
		return false;
	}

	FMovieSceneDoubleChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(ChannelIndex);
	if (!Channel)
	{
		UE_LOG(SeqTransformBPLib, Error,
		       TEXT("SetKeyFrameInterpInTransformTrack - Channel Index %d is invalid in Sequence - %s"),
		       ChannelIndex, *LevelSeqPath);
		return false;
	}

	auto LevelSequence = Cast<ULevelSequence>(Section->GetOutermostObject());
	if (!LevelSequence)
	{
		UE_LOG(SeqTransformBPLib, Error,
		       TEXT("SetKeyFrameInterpInTransformTrack - Level Sequence Invalid - %s"),
		       *LevelSeqPath);
		return false;
	}
	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal()
		/ LevelSequence->MovieScene->GetDisplayRate().AsDecimal();

	FFrameNumber FrameNumber = FFrameNumber{Frame * TicksPerFrame};

	// 找到需要修改的 Key Frame
	TArray<FFrameNumber> UnusedKeyTimes{};
	TArray<FKeyHandle> KeyHandles;
	Channel->GetKeys(TRange<FFrameNumber>(FrameNumber, FrameNumber), &UnusedKeyTimes, &KeyHandles);

	for (const auto& Handle : KeyHandles)
	{
		int Index = Channel->GetData().GetIndex(Handle);
		Channel->GetData().GetValues()[Index].Value = Value;
		Channel->GetData().GetValues()[Index].InterpMode = InterpMode;
		Channel->GetData().GetValues()[Index].TangentMode = TangentMode;

		if (TangentMode == RCTM_Auto)
		{
			Channel->GetData().GetValues()[Index].Tangent.ArriveTangent = 0.0f;
			Channel->GetData().GetValues()[Index].Tangent.LeaveTangent = 0.0f;
		}
		else if (TangentMode == RCTM_User)
		{
			// user 态下，tangent 的初入值应该为一样的
			Channel->GetData().GetValues()[Index].Tangent.ArriveTangent = ArriveTangent;
			Channel->GetData().GetValues()[Index].Tangent.LeaveTangent = ArriveTangent;
		}
		else if (TangentMode == RCTM_Break)
		{
			Channel->GetData().GetValues()[Index].Tangent.ArriveTangent = ArriveTangent;
			Channel->GetData().GetValues()[Index].Tangent.LeaveTangent = LeaveTangent;
		}
	}

	Section->Modify();
	return true;
}

#pragma endregion

#pragma region Visibility

bool UTransformAndKeyframeBPLib::AddVisibilityKeyframeForActor(const FString& LevelSeqPath,
                                                               const FGuid& ActorGuid,
                                                               int Frame,
                                                               bool Visibility)
{
	// 如已有关键帧则删除
	RemoveVisibilityKeyframe(LevelSeqPath, ActorGuid, Frame);

	auto LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (!LevelSequence)
	{
		UE_LOG(SeqTransformBPLib, Error,
		       TEXT("AddVisibilityKeyframeForActor - Level Sequence Invalid - %s"),
		       *LevelSeqPath);
		return false;
	}

	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal()
		/ LevelSequence->MovieScene->GetDisplayRate().AsDecimal();

	FFrameNumber FrameNumber = FFrameNumber{Frame * TicksPerFrame};

	auto VisibilityTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneVisibilityTrack>(ActorGuid);
	if (!VisibilityTrack)
	{
		LevelSequence->MovieScene->AddTrack<UMovieSceneVisibilityTrack>(ActorGuid);
	}

	bool bSectionAdded{false};
	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>(
		VisibilityTrack->FindOrAddSection(FrameNumber, bSectionAdded));

	if (bSectionAdded)
	{
		BoolSection->SetRange(TRange<FFrameNumber>::All());
	}
	auto Channel = BoolSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	Channel->GetData().AddKey(FrameNumber, Visibility);
#else
	Channel->AddKeys({FrameNumber}, {Visibility});
#endif

	VisibilityTrack->Modify();
	BoolSection->Modify();
	return true;
}

bool UTransformAndKeyframeBPLib::RemoveVisibilityKeyframe(const FString& LevelSeqPath,
                                                          const FGuid& ActorGuid,
                                                          int Frame)
{
	if (!ActorGuid.IsValid())
	{
		UE_LOG(SeqTransformBPLib, Error,
		       TEXT("SetKeyFrameInterpInTransformTrack - Actor GUID %s is invalid in Sequence - %s"),
		       *ActorGuid.ToString(), *LevelSeqPath);
		return false;
	}

	auto LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (!LevelSequence)
	{
		UE_LOG(SeqTransformBPLib, Error,
		       TEXT("RemoveVisibilityKeyframe - Level Sequence Invalid - %s"),
		       *LevelSeqPath);
		return false;
	}
	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal()
		/ LevelSequence->MovieScene->GetDisplayRate().AsDecimal();

	FFrameNumber FrameNumber = FFrameNumber{Frame * TicksPerFrame};

	auto VisibilityTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneVisibilityTrack>(ActorGuid);
	if (!VisibilityTrack)
	{
		return false;
	}

	auto BoolSection = Cast<UMovieSceneBoolSection>(VisibilityTrack->FindSection(FrameNumber));
	if (!BoolSection)
	{
		return false;
	}

	auto Channel = BoolSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);

	// 找到需要删除的 Key Frame
	TArray<FFrameNumber> UnusedKeyTimes{};
	TArray<FKeyHandle> KeyHandles;
	Channel->GetKeys(TRange<FFrameNumber>(FrameNumber, FrameNumber), &UnusedKeyTimes, &KeyHandles);

	Channel->DeleteKeys(KeyHandles);

	BoolSection->Modify();
	return true;
}

#pragma endregion

#pragma region MatParameter

bool UTransformAndKeyframeBPLib::AddMPCKeyframe(const FString& LevelSeqPath,
                                                UMaterialParameterCollection* MaterialParameterCollection,
                                                const FString& Parameter,
                                                int Frame,
                                                bool bColor,
                                                float ScalarValue,
                                                FLinearColor ColorValue)
{
	auto LevelSequence = Cast<ULevelSequence>(StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (!LevelSequence || !MaterialParameterCollection)
	{
		return false;
	}

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
	auto Tracks = LevelSequence->MovieScene->GetMasterTracks();
#else
	auto Tracks = LevelSequence->MovieScene->GetTracks();
#endif

	UMovieSceneMaterialParameterCollectionTrack* MPCTrack{};
	for (auto Track : Tracks)
	{
		UMovieSceneMaterialParameterCollectionTrack* MPCTemp = Cast<UMovieSceneMaterialParameterCollectionTrack>(Track);
		if (MPCTemp != nullptr && MPCTemp->MPC == MaterialParameterCollection)
		{
			MPCTrack = MPCTemp;
			break;
		}
	}

	if (MPCTrack == nullptr)
	{
#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
		MPCTrack = LevelSequence->MovieScene->AddMasterTrack<UMovieSceneMaterialParameterCollectionTrack>();
#else
		MPCTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneMaterialParameterCollectionTrack>();
#endif

		MPCTrack->MPC = MaterialParameterCollection;
		MPCTrack->SetDisplayName(FText::FromString(MaterialParameterCollection->GetName()));
	}

	UMovieSceneSection* MPCSection{};
	if (MPCTrack->GetAllSections().Num() > 0)
	{
		MPCSection = MPCTrack->GetAllSections()[0];
	}
	else
	{
		MPCSection = MPCTrack->CreateNewSection();
		MPCSection->SetRange(TRange<FFrameNumber>::All());
		MPCTrack->AddSection(*MPCSection);
	}

	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal()
		/ LevelSequence->MovieScene->GetDisplayRate().AsDecimal();
	FFrameNumber FrameNumber = FFrameNumber{Frame * TicksPerFrame};

	if (bColor)
	{
		MPCTrack->AddColorParameterKey(*Parameter, FrameNumber, ColorValue);
	}
	else
	{
		MPCTrack->AddScalarParameterKey(*Parameter, FrameNumber, ScalarValue);
	}

	MPCTrack->Modify();
	MPCSection->Modify();
	return true;
}

UMovieSceneSection* UTransformAndKeyframeBPLib::AddMISwitcherSection(const FString& LevelSeqPath,
                                                                     FGuid ExistingMeshGuid,
                                                                     int32 MaterialIndex)
{
	// Load Level Sequence
	ULevelSequence* LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (LevelSequence == nullptr || !ExistingMeshGuid.IsValid())
	{
		UE_LOG(SeqTransformBPLib, Error, TEXT("AddMISwitcherSection - Actor GUID %s Or Sequence Invalid - %s"),
		       *ExistingMeshGuid.ToString(), *LevelSeqPath);
		return nullptr;
	}

	// Add transform track
	UMovieScenePrimitiveMaterialTrack* NewTrack = LevelSequence->MovieScene->AddTrack<
		UMovieScenePrimitiveMaterialTrack>(
		ExistingMeshGuid);

	NewTrack->SetMaterialIndex(MaterialIndex);
	NewTrack->SetDisplayName(FText::FromString("Material Element" + FString::FromInt(MaterialIndex)));

	auto Section = NewTrack->CreateNewSection();

	NewTrack->AddSection(*Section);

	return Section;
}

bool UTransformAndKeyframeBPLib::AddKeyframeToMISwitcherSection(UMovieSceneSection* Section,
                                                                int32 Frame,
                                                                UMaterialInstanceConstant* MaterialInstanceConstant)
{
	UMovieScenePrimitiveMaterialSection* MaterialSection = Cast<UMovieScenePrimitiveMaterialSection>(Section);
	FMovieSceneObjectPathChannel* Channel = MaterialSection->GetChannelProxy().GetChannel<
		FMovieSceneObjectPathChannel>(0);

	UObject* Material = Cast<UObject>(MaterialInstanceConstant);

	auto KeyValue = FMovieSceneObjectPathChannelKeyValue(Material);
	Channel->GetData().UpdateOrAddKey(FFrameNumber{Frame * 400}, KeyValue);

	return true;
}

#pragma endregion

#pragma region BPVariables

bool UTransformAndKeyframeBPLib::AddBPVariableKeyframe(const FString& LevelSeqPath,
                                                       const FGuid& ActorGuid,
                                                       int Frame,
                                                       const FString& VariableName,
                                                       int VariableType,
                                                       bool BoolPropertyValue,
                                                       float FloatPropertyValue)
{
	const auto LevelSequence = Cast<ULevelSequence>(
		StaticLoadObject(ULevelSequence::StaticClass(), nullptr, *LevelSeqPath));
	if (!LevelSequence || !ActorGuid.IsValid())
	{
		return false;
	}

	int TicksPerFrame = LevelSequence->MovieScene->GetTickResolution().AsDecimal()
		/ LevelSequence->MovieScene->GetDisplayRate().AsDecimal();
	FFrameNumber FrameNumber = FFrameNumber{Frame * TicksPerFrame};

	switch (VariableType)
	{
	case 0: // Variable Type == 0, Bool Type
		{
			UMovieSceneBoolTrack* BoolTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneBoolTrack>(
				ActorGuid, FName(*VariableName));
			if (!BoolTrack)
			{
				BoolTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneBoolTrack>(ActorGuid);
				BoolTrack->SetPropertyNameAndPath(FName(*VariableName), VariableName);
			}

			bool bBoolSectionAdded{false};
			UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>(
				BoolTrack->FindOrAddSection(FrameNumber, bBoolSectionAdded));
			if (bBoolSectionAdded)
			{
				BoolSection->SetRange(TRange<FFrameNumber>::All());
			}

			FMovieSceneBoolChannel* BoolChannel = BoolSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
			BoolChannel->GetData().AddKey(FrameNumber, BoolPropertyValue);
#else
			BoolChannel->AddKeys({FrameNumber}, {BoolPropertyValue});
#endif

			BoolTrack->Modify();
			BoolSection->Modify();
		}
		break;

	case 1:
		{
			UMovieSceneFloatTrack* FloatTrack = LevelSequence->MovieScene->FindTrack<UMovieSceneFloatTrack>(
				ActorGuid, FName(*VariableName));
			if (!FloatTrack)
			{
				FloatTrack = LevelSequence->MovieScene->AddTrack<UMovieSceneFloatTrack>(ActorGuid);
				FloatTrack->SetPropertyNameAndPath(FName(*VariableName), VariableName);
			}

			bool bFloatSectionAdded{false};
			UMovieSceneFloatSection* BoolSection = Cast<UMovieSceneFloatSection>(
				FloatTrack->FindOrAddSection(FrameNumber, bFloatSectionAdded));
			if (bFloatSectionAdded)
			{
				BoolSection->SetRange(TRange<FFrameNumber>::All());
			}

			FMovieSceneFloatChannel* BoolChannel = BoolSection->GetChannelProxy().GetChannel<
				FMovieSceneFloatChannel>(0);
			BoolChannel->AddLinearKey(FrameNumber, FloatPropertyValue);

			FloatTrack->Modify();
			BoolSection->Modify();
		}
		break;

	default:
		UE_LOG(SeqTransformBPLib, Error, TEXT("AddBPVariableKeyframe - Unsupported Vairable Type: %d"), VariableType);
		break;
	}

	return true;
}

#pragma endregion
