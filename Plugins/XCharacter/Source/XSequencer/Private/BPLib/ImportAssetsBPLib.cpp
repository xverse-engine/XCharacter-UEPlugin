// Fill out your copyright notice in the Description page of Project Settings.

#include "BPLib/ImportAssetsBPLib.h"

#include "XSequencerDefines.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"

#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"

#include "EditorReimportHandler.h"
#include "EditorFramework/AssetImportData.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Animation/AnimSequence.h"
#include "Engine/DataTable.h"
#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY(AssetImportExport);

UObject* UImportAssetsBPLib::ImportAsset(const FString& SourcePath,
                                         const FString& DestinationPath)
{
	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::ImportAsset - Importing Asset, Source Path: %s"),
	       *SourcePath);

	UAssetImportTask* ImportTask = CreateImportTask(SourcePath, DestinationPath, nullptr, nullptr);
	if (!ImportTask)
	{
		return nullptr;
	}

	UObject* RetAsset = ProcessImportTask(ImportTask);
	if (!RetAsset)
	{
		return nullptr;
	}

	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::ImportAsset - Import Asset Success, Dest Path: %s"),
	       *DestinationPath);
	return RetAsset;
}

UStaticMesh* UImportAssetsBPLib::ImportStaticMesh(const FString& SourcePath,
                                                  const FString& DestinationPath)
{
	UFbxImportUI* Options = NewObject<UFbxImportUI>();

	// Options for static mesh
	Options->bAutomatedImportShouldDetectType = false;
	Options->MeshTypeToImport = EFBXImportType::FBXIT_StaticMesh;
	Options->bImportMesh = true;
	// No Skeletal Mesh or Animation
	Options->bImportAsSkeletal = false;
	Options->bCreatePhysicsAsset = false;

	Options->bImportAnimations = false;
	Options->bImportTextures = true;
	Options->bImportMaterials = true;
	Options->bResetToFbxOnMaterialConflict = true;
	Options->LodNumber = 0;

	Options->StaticMeshImportData->ImportTranslation = FVector(0.0f);
	Options->StaticMeshImportData->ImportRotation = FRotator(0.0f);
	Options->StaticMeshImportData->ImportUniformScale = 1.0f;
	Options->StaticMeshImportData->bConvertScene = true;
	Options->StaticMeshImportData->bForceFrontXAxis = true;
	Options->StaticMeshImportData->bConvertSceneUnit = true;

	Options->StaticMeshImportData->bTransformVertexToAbsolute = false;
	Options->StaticMeshImportData->bBakePivotInVertex = false;
	Options->StaticMeshImportData->bImportMeshLODs = true;
	Options->StaticMeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ComputeNormals;
	Options->StaticMeshImportData->NormalGenerationMethod = EFBXNormalGenerationMethod::BuiltIn;
	Options->StaticMeshImportData->bComputeWeightedNormals = true;
	Options->StaticMeshImportData->bReorderMaterialToFbxOrder = false;

	Options->StaticMeshImportData->StaticMeshLODGroup = FName();
	Options->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
	Options->StaticMeshImportData->bRemoveDegenerates = true;
	Options->StaticMeshImportData->bBuildReversedIndexBuffer = true;
	Options->StaticMeshImportData->bBuildNanite = false;
	Options->StaticMeshImportData->bGenerateLightmapUVs = true;
	Options->StaticMeshImportData->bOneConvexHullPerUCX = true;
	Options->StaticMeshImportData->bAutoGenerateCollision = true;
	Options->StaticMeshImportData->bCombineMeshes = true;
	Options->StaticMeshImportData->DistanceFieldResolutionScale = 0.0f;

	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::ImportStaticMesh - Importing Static Mesh Asset, Source Path: %s"),
	       *SourcePath);

	UAssetImportTask* ImportTask = CreateImportTask(SourcePath, DestinationPath, nullptr, Options);
	if (!ImportTask)
	{
		return nullptr;
	}

	UStaticMesh* RetAsset = Cast<UStaticMesh>(ProcessImportTask(ImportTask));
	if (!RetAsset)
	{
		return nullptr;
	}

	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::ImportAsset - Import Static Mesh Success, Dest Path: %s"),
	       *DestinationPath);
	return RetAsset;
}

USkeletalMesh* UImportAssetsBPLib::ImportSkeletalMesh(const FString& SourcePath,
                                                      const FString& DestinationPath)
{
	UFbxImportUI* Options = NewObject<UFbxImportUI>();

	// Options for Skeletal mesh
	Options->bAutomatedImportShouldDetectType = false;
	Options->MeshTypeToImport = EFBXImportType::FBXIT_SkeletalMesh;
	Options->bImportMesh = true;
	// No Skeletal Mesh or Animation
	Options->bImportAsSkeletal = true;
	Options->bCreatePhysicsAsset = false;

	Options->bImportAnimations = false;
	Options->bImportTextures = true;
	Options->bImportMaterials = true;
	Options->bResetToFbxOnMaterialConflict = true;
	Options->LodNumber = 0;

	Options->SkeletalMeshImportData->ImportTranslation = FVector(0.0f);
	Options->SkeletalMeshImportData->ImportRotation = FRotator(0.0f);
	Options->SkeletalMeshImportData->ImportUniformScale = 1.0f;
	Options->SkeletalMeshImportData->bConvertScene = true;
	Options->SkeletalMeshImportData->bForceFrontXAxis = true;
	Options->SkeletalMeshImportData->bConvertSceneUnit = true;

	Options->SkeletalMeshImportData->bTransformVertexToAbsolute = false;
	Options->SkeletalMeshImportData->bBakePivotInVertex = false;
	Options->SkeletalMeshImportData->bImportMeshLODs = true;
	Options->SkeletalMeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ComputeNormals;
	Options->SkeletalMeshImportData->NormalGenerationMethod = EFBXNormalGenerationMethod::BuiltIn;
	Options->SkeletalMeshImportData->bComputeWeightedNormals = true;
	Options->SkeletalMeshImportData->bReorderMaterialToFbxOrder = false;

	Options->SkeletalMeshImportData->ImportContentType = EFBXImportContentType::FBXICT_All;
	Options->SkeletalMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
	Options->SkeletalMeshImportData->bUpdateSkeletonReferencePose = true;
	Options->SkeletalMeshImportData->bUseT0AsRefPose = true;
	Options->SkeletalMeshImportData->bPreserveSmoothingGroups = true;
	Options->SkeletalMeshImportData->bImportMeshesInBoneHierarchy = true;
	Options->SkeletalMeshImportData->bImportMorphTargets = true;
	Options->SkeletalMeshImportData->ThresholdPosition = 0.0f;
	Options->SkeletalMeshImportData->ThresholdTangentNormal = 0.0f;
	Options->SkeletalMeshImportData->ThresholdUV = 0.0f;
	Options->SkeletalMeshImportData->MorphThresholdPosition = 0.0f;

	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::ImportStaticMesh - Importing Skeletal Mesh Asset, Source Path: %s"),
	       *SourcePath);

	UAssetImportTask* ImportTask = CreateImportTask(SourcePath, DestinationPath, nullptr, Options);
	if (!ImportTask)
	{
		return nullptr;
	}

	USkeletalMesh* RetAsset = Cast<USkeletalMesh>(ProcessImportTask(ImportTask));
	if (!RetAsset)
	{
		return nullptr;
	}

	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::ImportAsset - Import Skeletal Asset Success, Dest Path: %s"),
	       *DestinationPath);
	return RetAsset;
}

FString UImportAssetsBPLib::GetAssetImportPath(const FString& AssetPath)
{
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		UE_LOG(AssetImportExport, Error,
		       TEXT("UImportAssetsBPLib::GetAssetImportPath - Asset Invalid - %s"),
		       *AssetPath);
		return "";
	}

	TArray<FString> Paths{};
	if (FReimportManager::Instance()->CanReimport(Asset, &Paths) && Paths.Num() > 0)
	{
		FString Path = Paths[0];
		return Path;
	}

	if (UAssetImportData* ImportData = GetAssetImportData(Asset))
	{
		return ImportData->GetFirstFilename();
	}

	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::GetAssetImportPath - Unable To Get Import Path For Asset: %s"),
	       *AssetPath);
	return "";
}

bool UImportAssetsBPLib::SetAssetImportPath(const FString& AssetPath, FString NewImportPath)
{
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		UE_LOG(AssetImportExport, Error,
		       TEXT("UImportAssetsBPLib::SetAssetImportPath - Asset Invalid - %s"),
		       *AssetPath);
		return false;
	}

	if (FReimportManager::Instance()->CanReimport(Asset, nullptr))
	{
		FReimportManager::Instance()->UpdateReimportPath(Asset, NewImportPath, 0);
		UE_LOG(AssetImportExport, Warning,
		       TEXT("UImportAssetsBPLib::SetAssetImportPath - Updated Import Path for - %s"),
		       *AssetPath);
		UE_LOG(AssetImportExport, Warning,
		       TEXT("UImportAssetsBPLib::SetAssetImportPath - New Import Path - %s"),
		       *NewImportPath);
		return true;
	}

	UE_LOG(AssetImportExport, Error,
	       TEXT("UImportAssetsBPLib::SetAssetImportPath - Unable To Set New Import Path For - %s"),
	       *AssetPath);
	return false;
}

UAssetImportData* UImportAssetsBPLib::GetAssetImportData(UObject* Asset)
{
	if (Cast<UStaticMesh>(Asset))
	{
		return Cast<UStaticMesh>(Asset)->AssetImportData;
	}
	if (Cast<USkeletalMesh>(Asset))
	{
		return Cast<USkeletalMesh>(Asset)->GetAssetImportData();
	}
	if (Cast<UTexture>(Asset))
	{
		return Cast<UTexture>(Asset)->AssetImportData;
	}
	if (Cast<UAnimSequence>(Asset))
	{
		return Cast<UAnimSequence>(Asset)->AssetImportData;
	}
	if (Cast<UDataTable>(Asset))
	{
		return Cast<UDataTable>(Asset)->AssetImportData;
	}
	if (Cast<USoundWave>(Asset))
	{
		return Cast<USoundWave>(Asset)->AssetImportData;
	}
	return nullptr;
}

UAssetImportTask* UImportAssetsBPLib::CreateImportTask(const FString& SourcePath,
                                                       const FString& DestinationPath,
                                                       UFactory* ExtraFactory,
                                                       UObject* ExtraOptions)
{
	UAssetImportTask* RetTask = NewObject<UAssetImportTask>();
	if (!RetTask)
	{
		UE_LOG(AssetImportExport, Error, TEXT("UImportAssetsBPLib::CreateImportTask - Create Import Task Error"));
		return nullptr;
	}

	// Set Path Information
	RetTask->Filename = SourcePath;
	RetTask->DestinationPath = FPaths::GetPath(DestinationPath);
	RetTask->DestinationName = FPaths::GetCleanFilename(DestinationPath);

	// Set Basic Options
	RetTask->bSave = false;
	RetTask->bAutomated = true;
	RetTask->bAsync = false;
	RetTask->bReplaceExisting = true;
	RetTask->bReplaceExistingSettings = false;

	if (ExtraFactory)
	{
		RetTask->Factory = ExtraFactory;
	}
	if (ExtraOptions)
	{
		RetTask->Options = ExtraOptions;
	}

	UE_LOG(AssetImportExport, Warning,
	       TEXT("UImportAssetsBPLib::CreateImportTask - Created Import Task, Source Path: %s"),
	       *SourcePath);
	return RetTask;
}

UObject* UImportAssetsBPLib::ProcessImportTask(UAssetImportTask* ImportTask)
{
	if (!ImportTask)
	{
		UE_LOG(AssetImportExport, Error, TEXT("UImportAssetsBPLib::ProcessImportTask - Invalid Import Task"));
		return nullptr;
	}

	FAssetToolsModule* AssetToolsModule = FModuleManager::LoadModulePtr<FAssetToolsModule>("AssetTools");
	if (!AssetToolsModule)
	{
		UE_LOG(AssetImportExport, Error, TEXT("UImportAssetsBPLib::ProcessImportTask - Load Asset Tools Module Error"));
		return nullptr;
	}

	AssetToolsModule->Get().ImportAssetTasks({ImportTask});
	if (ImportTask->GetObjects().Num() == 0)
	{
		UE_LOG(AssetImportExport, Error,
		       TEXT(
			       "UImportAssetsBPLib::ProcessImportTask - Nothing was Imported, Is the file valid? Is the asset type supported?"
		       ));
		return nullptr;
	}

	UObject* ImportedAsset = StaticLoadObject(UObject::StaticClass(),
	                                          nullptr,
	                                          *FPaths::Combine(ImportTask->DestinationPath,
	                                                           ImportTask->DestinationName));
	return ImportedAsset;
}
