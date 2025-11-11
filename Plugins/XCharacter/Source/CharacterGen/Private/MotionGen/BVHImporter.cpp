// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionGen/BVHImporter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "RawIndexBuffer.h"
#include "Misc/ScopedSlowTask.h"

#include "PackageTools.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "ObjectTools.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshModel.h"

#include "Utils.h"

#include "MeshUtilities.h"
#include "Materials/Material.h"
#include "Modules/ModuleManager.h"

#include "Async/ParallelFor.h"


#include "MotionGen/BVHAssetImportData.h"
#include "MotionGen/BVHFile.h"

#include "AnimationUtils.h"
#include "ComponentReregisterContext.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "MotionGen/BVHFile.h"
#include "MotionGen/BVHImportSettings.h"

#define LOCTEXT_NAMESPACE "BVHImporter"

DEFINE_LOG_CATEGORY_STATIC(LogBVHImporter, Verbose, All);


FBVHImporter::FBVHImporter()
	: ImportSettings(nullptr), BvhFile(nullptr)
{

}

FBVHImporter::~FBVHImporter()
{
	delete BvhFile;
}


void FBVHImporter::RetrieveAssetImportData(UBVHAssetImportData* AssetImportData)
{
}

void FBVHImporter::SetImportSetting(UBVHImportSettings* InImportSetting)
{
	ImportSettings = InImportSetting;
}

void FBVHImporter::SetMoGenSetting(UMoGenSetting* InMoGenSetting)
{ 
	MoGenSetting = InMoGenSetting;
}

FBVHFile* FBVHImporter::GetBvhFile()
{
	return BvhFile;
}


const EBVHImportError FBVHImporter::CreateBVHFileByContent(const FString Content) {
	
	BvhFile = new FBVHFile();


	// 使用UTF-8字符串初始化std::string
	std::string StandardString = TCHAR_TO_ANSI(*Content);

	if (!BvhFile->Import(StandardString))
	{
		return EBVHImportError::BVHImportError_FailedToOpenFile;
	}

	ImportSettings->MotionName = FString(BvhFile->GetMotionName().c_str());
	ImportSettings->FrameNum = BvhFile->GetNumFrame();
	ImportSettings->FrameEnd = BvhFile->GetNumFrame() - 1;
	ImportSettings->TimeStep = BvhFile->GetInterval();
	ImportSettings->ResampleRate = 1 / ImportSettings->TimeStep;

	return EBVHImportError::BVHImportError_NoError;

}

const EBVHImportError FBVHImporter::OpenBVHFileForImport(const FString InFilePath)
{
	BvhFile = new FBVHFile(TCHAR_TO_ANSI ( * InFilePath));
	if (!BvhFile->Open())
	{
		return EBVHImportError::BVHImportError_FailedToOpenFile;
	}

	ImportSettings->MotionName = FString(BvhFile->GetMotionName().c_str());
	ImportSettings->FrameNum = BvhFile->GetNumFrame();
	ImportSettings->FrameEnd = BvhFile->GetNumFrame() - 1;
	ImportSettings->TimeStep = BvhFile->GetInterval();
	ImportSettings->ResampleRate = 1 / ImportSettings->TimeStep;

	return EBVHImportError::BVHImportError_NoError;
}

template<typename T>
T* FBVHImporter::CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags)
{
	// Parent package to place new mesh
	UPackage* Package = nullptr;
	FString NewPackageName;

	// Setup package name and create one accordingly
	NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName() + TEXT("/") + ObjectName);
	NewPackageName = UPackageTools::SanitizePackageName(NewPackageName);
	Package = CreatePackage(*NewPackageName);

	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

	T* ExistingTypedObject = FindObject<T>(Package, *SanitizedObjectName);
	UObject* ExistingObject = FindObject<UObject>(Package, *SanitizedObjectName);

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
			InParent = Package;
		}
		else
		{
			// failed to delete
			return nullptr;
		}
	}

	return NewObject<T>(Package, FName(*SanitizedObjectName), Flags | RF_Public);
}

void FBVHImporter::SetMetaData(const TArray<UObject*>& Objects)
{
}

const uint32 FBVHImporter::GetStartFrameIndex() const
{
	return ImportSettings->FrameStart;
}

const uint32 FBVHImporter::GetEndFrameIndex() const
{
	return ImportSettings->FrameEnd < ImportSettings->FrameNum ? ImportSettings->FrameEnd : ImportSettings->FrameNum - 1;
}

#undef LOCTEXT_NAMESPACE
