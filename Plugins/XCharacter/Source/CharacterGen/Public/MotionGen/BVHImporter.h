// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimSequence.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "BVHFile.h"

class FSkeletalMeshLODModel;
class UMaterial;
class UStaticMesh;
class USkeletalMesh;

class UBVHImportSettings;
class UMoGenSetting;
class FSkeletalMeshImportData;
class UBVHAssetImportData;

struct FBVHImportData;

enum EBVHImportError : uint32
{
	BVHImportError_NoError,
	BVHImportError_FailedToOpenFile,
	BVHImportError_FailedToImportData
};

class FBVHImporter
{
public:
	FBVHImporter();
	~FBVHImporter();
public:

	const EBVHImportError CreateBVHFileByContent(const FString Content);

	const EBVHImportError OpenBVHFileForImport(const FString InFilePath);

	/** Returns the lowest frame index containing data for the imported Alembic file */
	const uint32 GetStartFrameIndex() const;

	/** Returns the highest frame index containing data for the imported Alembic file */
	const uint32 GetEndFrameIndex() const;

	void RetrieveAssetImportData(UBVHAssetImportData* ImportData);

	void SetImportSetting(UBVHImportSettings* InImportSetting);

	void SetMoGenSetting(UMoGenSetting* InMoGenSetting);

	FBVHFile* GetBvhFile();

public:
	/** Cached ptr for the import settings */
	UBVHImportSettings* ImportSettings;

	UMoGenSetting* MoGenSetting;

private:
	/**
	* Creates an template object instance taking into account existing Instances and Objects (on reimporting)
	*
	* @param InParent - ParentObject for the geometry cache, this can be changed when parent is deleted/re-created
	* @param ObjectName - Name to be used for the created object
	* @param Flags - Object creation flags
	*/
	template<typename T> T* CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags);
	
	/** Set the Alembic archive metadata on the given objects */
	void SetMetaData(const TArray<UObject*>& Objects);

private:
	

	/** Offset for each sample, used as the root bone translation */
	TOptional<TArray<FVector>> SamplesOffsets;

	/** ABC file representation for currently opened filed */
	FBVHFile* BvhFile;
};
