// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionGen/BVHImportFactory.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Math/UnrealMathUtility.h"

#include "MotionGen/BVHImportOptions.h"
#include "MotionGen/BVHImporter.h"
#include "MotionGen/BVHImportSettings.h"
#include "MotionGen/BVHAssetImportData.h"
#include "MotionGen/BVHFile.h"

#include "Subsystems/AssetEditorSubsystem.h"

#include "AI/Navigation/NavCollisionBase.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "ImportUtils/StaticMeshImportUtils.h"
#include "ObjectTools.h"

#include "Factories/AnimSequenceFactory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BVHImportFactory)

#define LOCTEXT_NAMESPACE "BVHImportFactory"

DEFINE_LOG_CATEGORY_STATIC(LogBvhImporter, Verbose, All);

UBVHImportFactory::UBVHImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = nullptr;

	bEditorImport = true;
	bText = false;

	bShowOption = true;

	Formats.Add(TEXT("bvh;BVH"));
}

void UBVHImportFactory::PostInitProperties()
{
	Super::PostInitProperties();
	ImportSettings = UBVHImportSettings::Get();
}

FText UBVHImportFactory::GetDisplayName() const
{
	return LOCTEXT("BVHImportFactoryDescription", "BVH");
}

bool UBVHImportFactory::DoesSupportClass(UClass* Class)
{
	return (Class == UAnimSequence::StaticClass());
}

UClass* UBVHImportFactory::ResolveSupportedClass()
{
	return UAnimSequence::StaticClass();
}

bool UBVHImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);
	return FPaths::GetExtension(Filename) == TEXT("bvh");
}

UObject* UBVHImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
                                              const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
                                              bool& bOutOperationCanceled)
{
	const bool bIsUnattended = (IsAutomatedImport()
		|| FApp::IsUnattended()
		|| IsRunningCommandlet()
		|| GIsRunningUnattendedScript);
	bShowOption = !bIsUnattended;

	// Check if it's a re-import
	if (InParent != nullptr)
	{
		UObject* ExistingObject =
			StaticFindObject(UObject::StaticClass(), InParent, *InName.ToString());
		if (ExistingObject)
		{
			// Use this class as no other re-import handler exist for Alembics, yet
			FReimportHandler* ReimportHandler = this;
			TArray<FString> Filenames;
			Filenames.Add(UFactory::CurrentFilename);
			// Set the new source path before starting the re-import
			FReimportManager::Instance()->UpdateReimportPaths(ExistingObject, Filenames);
			// Do the re-import and exit
			const bool bIsAutomated = bIsUnattended;
			const bool bShowNotification = !bIsAutomated;
			const bool bAskForNewFileIfMissing = true;
			const FString PreferredReimportFile;
			const int32 SourceFileIndex = INDEX_NONE;
			const bool bForceNewFile = false;
			FReimportManager::Instance()->Reimport(
				ExistingObject,
				bAskForNewFileIfMissing,
				bShowNotification,
				PreferredReimportFile,
				ReimportHandler,
				SourceFileIndex,
				bForceNewFile,
				bIsAutomated);
			return ExistingObject;
		}
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(
		this, InClass, InParent, InName, TEXT("BVH"));

	// Use (and show) the settings from the script if provided
	UBVHImportSettings* ScriptedSettings = AssetImportTask
		                                       ? Cast<UBVHImportSettings>(AssetImportTask->Options)
		                                       : nullptr;
	if (ScriptedSettings)
	{
		ImportSettings = ScriptedSettings;
	}

	FBVHImporter Importer;
	Importer.SetImportSetting(ImportSettings);
	EBVHImportError ErrorCode = Importer.OpenBVHFileForImport(Filename);
	ImportSettings->bReimport = false;
	AdditionalImportedObjects.Empty();

	// Set up message log page name to separate different assets
	FText ImportingText = FText::Format(LOCTEXT("BVHFactoryImporting", "Importing {0}.bvh"), FText::FromName(InName));
	const FString& PageName = ImportingText.ToString();

	if (ErrorCode != BVHImportError_NoError)
	{
		return nullptr;
	}

	bOutOperationCanceled = false;

	if (bShowOption)
	{
		TSharedPtr<SBVHImportOptions> Options;
		ShowImportOptionsWindow(Options, UFactory::CurrentFilename, Importer);
		// Set whether or not the user canceled
		bOutOperationCanceled = !Options->ShouldImport();
	}

	TArray<UObject*> ResultAssets;
	if (!bOutOperationCanceled)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(
			this, InClass, InParent, InName, TEXT("BVH"));

		UObject* AnimSeq = ImportAnimation(ImportSettings->Skeleton.Get(), InParent, &Importer);
		ResultAssets.Add(AnimSeq);

		AdditionalImportedObjects.Reserve(ResultAssets.Num());
		for (UObject* Object : ResultAssets)
		{
			if (Object)
			{
				FAssetRegistryModule::AssetCreated(Object);
				GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);
				Object->MarkPackageDirty();
				Object->PostEditChange();
				AdditionalImportedObjects.Add(Object);
			}
		}
	}

	return (ResultAssets.Num() > 0) ? ResultAssets[0] : nullptr;
}


UAnimSequence* UBVHImportFactory::CreateAnimSequence(USkeleton* Skeleton, UObject* Outer, FBVHImporter* Importer)
{
	//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, TEXT("BVH"));

	UAnimSequence* AnimSeq = ImportAnimation(Skeleton, Outer, Importer);
	if (AnimSeq)
	{
		FAssetRegistryModule::AssetCreated(AnimSeq);
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, AnimSeq);
		AnimSeq->MarkPackageDirty();
		AnimSeq->PostEditChange();
		return AnimSeq;
	}
	else
	{
		return nullptr;
	}
}


UAnimSequence* UBVHImportFactory::ImportAnimation(USkeleton* Skeleton, UObject* Outer, FBVHImporter* Importer)
{
	// we need skeleton to create animsequence
	if (Skeleton == NULL)
	{
		FString Name = "Default_Skeleton";

		Skeleton = Importer->GetBvhFile()->FindDefaultSkeleton();
		if (Skeleton == NULL)
		{
			Skeleton = Importer->GetBvhFile()->CreateSkeletonByBvhInfo(Outer, Name);
		}
		//UE_LOG(LogBvhImporter, Warning, TEXT("Skeleton Not Selected, Use Default Skeleton."));
	}


	UAnimSequence* LastCreatedAnim = NULL;

	int32 ResampleRate = DEFAULT_SAMPLERATE;
	if (ImportSettings->ResampleRate > 0)
	{
		ResampleRate = ImportSettings->ResampleRate;
	}

	//redirect settings
	ImportSettings->ResampleRate = Importer->ImportSettings->ResampleRate;
	ImportSettings->TimeStep = Importer->ImportSettings->TimeStep;
	ImportSettings->FrameNum = Importer->ImportSettings->FrameNum;

	ImportSettings->FrameStart = Importer->ImportSettings->FrameStart;
	ImportSettings->FrameEnd = Importer->ImportSettings->FrameEnd;

	ImportSettings->MotionName = Importer->ImportSettings->MotionName;

	FString SequenceName = ImportSettings->MotionName;

	// See if this sequence already exists.
	SequenceName = ObjectTools::SanitizeObjectName(SequenceName);

	FString NewPackageName = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*Outer->GetName()),
	                                         *SequenceName);
	UPackage* Package = CreatePackage(*NewPackageName);

	UPackage* ExistingTypedObject = FindObject<UPackage>(Package, *SequenceName);
	UObject* ExistingObject = FindObject<UObject>(Package, *SequenceName);

	if (ExistingTypedObject != nullptr)
	{
		ExistingTypedObject->PreEditChange(nullptr);
	}
	else if (ExistingObject != nullptr)
	{
		// Replacing an object.  Here we go!
		// Delete the existing object
		const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);

		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Create a package for each mesh
			Package = CreatePackage(*NewPackageName);
		}
		else
		{
			// failed to delete
			return nullptr;
		}
	}


	UObject* Object = LoadObject<UObject>(Package, *SequenceName, NULL, (LOAD_Quiet | LOAD_NoWarn), NULL);
	UAnimSequence* DestSeq = Cast<UAnimSequence>(Object);
	// if object with same name exists, warn user
	if (Object && !DestSeq)
	{
		UE_LOG(LogBvhImporter, Error, TEXT("Asset already exists."));
		return NULL;
	}

	auto AnimSequenceFactory = NewObject<UAnimSequenceFactory>();
	AnimSequenceFactory->TargetSkeleton = Skeleton;

	// If not, create new one now.
	if (!DestSeq)
	{
		DestSeq = (UAnimSequence*)AnimSequenceFactory->FactoryCreateNew(
			UAnimSequence::StaticClass(), Package, *SequenceName, RF_Standalone | RF_Public, NULL, GWarn);

		CreatedObjects.Add(DestSeq);
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(DestSeq);
	}

	DestSeq->SetSkeleton(Skeleton);

	// float PreviousSequenceLength = DestSeq->GetPlayLength();
	IAnimationDataController& Controller = DestSeq->GetController();
	//Controller.InitializeModel();
	Controller.OpenBracket(LOCTEXT("ImportAnimation_Bracket", "Importing Animation"));

	//This destroy all previously imported animation raw data
	Controller.RemoveAllBoneTracks();

	// if you have one pose(thus 0.f duration), it still contains animation, so we'll need to consider that as MINIMUM_ANIMATION_LENGTH time length
	float PreviousSequenceLength = FMath::Max(ImportSettings->FrameNum - 1, MINIMUM_ANIMATION_LENGTH) * ImportSettings->
		TimeStep;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
	DestSeq->GetController().SetPlayLength(PreviousSequenceLength);
#else
	DestSeq->GetController().SetNumberOfFrames(
		DestSeq->GetController().ConvertSecondsToFrameNumber(PreviousSequenceLength));
#endif

	if (PreviousSequenceLength > MINIMUM_ANIMATION_LENGTH && DestSeq->GetDataModel()->GetNumberOfFloatCurves() > 0)
	{
		// The sequence already existed when we began the import. We need to scale the key times for all curves to match the new 
		// duration before importing over them. This is to catch any user-added curves
		float ScaleFactor = DestSeq->GetPlayLength() / PreviousSequenceLength;
		if (!FMath::IsNearlyEqual(ScaleFactor, 1.f))
		{
			for (const FFloatCurve& Curve : DestSeq->GetDataModel()->GetFloatCurves())
			{
#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION <= 2)
				const FAnimationCurveIdentifier CurveId(Curve.Name, ERawCurveTrackTypes::RCT_Float);
#else
				const FAnimationCurveIdentifier CurveId(Curve.GetName(), ERawCurveTrackTypes::RCT_Float);
#endif
				Controller.ScaleCurve(CurveId, 0.f, ScaleFactor);
			}
		}
	}


	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	Controller.SetFrameRate(FFrameRate(ResampleRate, 1));

	FBVHFile* BvhFile = Importer->GetBvhFile();

	for (int32 j = 0; j < BvhFile->GetNumJoint(); ++j)
	{
		int32 NumKeysForTrack = 0;

		// see if it's found in Skeleton
		const Joint* joint = BvhFile->GetJoint(j);
		int32 JointIdx = joint->index;


		FName BoneName(ANSI_TO_TCHAR(joint->name.c_str()));
		int32 BoneTreeIndex = RefSkeleton.FindBoneIndex(BoneName);
		//UE_LOG(LogBvhImporter, Error, TEXT("TrackName: %s"), *BoneName.ToString());
		const int32 NumSamplingFrame = ImportSettings->FrameEnd;
		const int32 NumTotalTracks = BvhFile->GetNumJoint();

		// update status
		FFormatNamedArguments Args;
		Args.Add(TEXT("TrackName"), FText::FromName(BoneName));
		Args.Add(TEXT("TotalKey"), FText::AsNumber(NumSamplingFrame + 1)); //Key number is Frame + 1
		Args.Add(TEXT("TrackIndex"), FText::AsNumber(JointIdx + 1));
		Args.Add(TEXT("TotalTracks"), FText::AsNumber(NumTotalTracks));
		const FText StatusUpate = FText::Format(LOCTEXT("ImportingAnimTrackDetail",
		                                                "Importing Animation Track [{TrackName}] ({TrackIndex}/{TotalTracks}) - TotalKey {TotalKey}"), Args);

		if (BoneTreeIndex != INDEX_NONE)
		{
			bool bSuccess = true;

			FRawAnimSequenceTrack RawTrack;
			RawTrack.PosKeys.Empty();
			RawTrack.RotKeys.Empty();
			RawTrack.ScaleKeys.Empty();

			TArray<float> TimeKeys;

			for (int32 FrameIdx = 0; FrameIdx < BvhFile->GetNumFrame(); ++FrameIdx)
			{
				double CurTime = ImportSettings->TimeStep * FrameIdx;
				FTransform LocalTransform = BvhFile->GetTransform(FrameIdx, JointIdx);
				if (LocalTransform.ContainsNaN())
				{
					bSuccess = false;
					//UE_LOG(LogBvhImporter, Error, TEXT("Bvh contain NaN."));
					break;
				}

				RawTrack.ScaleKeys.Add(FVector3f(LocalTransform.GetScale3D()));
				RawTrack.PosKeys.Add(FVector3f(LocalTransform.GetTranslation()));
				RawTrack.RotKeys.Add(FQuat4f(LocalTransform.GetRotation()));

				TimeKeys.Add(CurTime);

				++NumKeysForTrack;
			}

			if (bSuccess)
			{
				check(RawTrack.ScaleKeys.Num() == NumKeysForTrack);
				check(RawTrack.PosKeys.Num() == NumKeysForTrack);
				check(RawTrack.RotKeys.Num() == NumKeysForTrack);

				if (BoneTreeIndex != INDEX_NONE)
				{
					//add new track
					Controller.AddBoneTrack(BoneName);
					Controller.SetBoneTrackKeys(BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys);
				}
			}
		}
	}

	Controller.UpdateCurveNamesFromSkeleton(Skeleton, ERawCurveTrackTypes::RCT_Float);
	Controller.NotifyPopulated();

	Controller.CloseBracket();

	DestSeq->ImportFileFramerate = ImportSettings->ResampleRate;
	DestSeq->ImportResampleFramerate = 1 / ImportSettings->TimeStep;

	DestSeq->PostEditChange();
	DestSeq->ResetSkeleton(Skeleton);
	DestSeq->SetPreviewMesh(Skeleton->GetPreviewMesh());
	DestSeq->MarkPackageDirty();

	LastCreatedAnim = DestSeq;

	return LastCreatedAnim;
}

bool UBVHImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UAssetImportData* ImportData = nullptr;
	if (Obj->GetClass() == UAnimSequence::StaticClass())
	{
		UAnimSequence* Cache = Cast<UAnimSequence>(Obj);
		ImportData = Cache->AssetImportData;
	}

	if (ImportData)
	{
		if (FPaths::GetExtension(ImportData->GetFirstFilename()) == TEXT("bvh") || (Obj->GetClass() ==
			UAnimSequence::StaticClass() && ImportData->GetFirstFilename().IsEmpty()))
		{
			ImportData->ExtractFilenames(OutFilenames);
			return true;
		}
	}
	return false;
}

void UBVHImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UAnimSequence* Sequence = Cast<UAnimSequence>(Obj);
	if (Sequence && Sequence->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		Sequence->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UBVHImportFactory::Reimport(UObject* Obj)
{
	ImportSettings->bReimport = true;

	return EReimportResult::Failed;
}

void UBVHImportFactory::ShowImportOptionsWindow(TSharedPtr<SBVHImportOptions>& Options, FString FilePath,
                                                const FBVHImporter& Importer)
{
	// Window size computed from SAlembicImportOptions
	const float WindowHeight = 500.f;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "BVH Import Options"))
		.ClientSize(FVector2D(522.f, WindowHeight));

	Window->SetContent
	(
		SAssignNew(Options, SBVHImportOptions)
		.WidgetWindow(Window)
		.ImportSettings(ImportSettings)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(FilePath))
	);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

int32 UBVHImportFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
