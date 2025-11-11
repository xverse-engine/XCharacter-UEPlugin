// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "BVHImportFactory.generated.h"

class UBVHImportSettings;
class USkeletalMesh;
class UBVHAssetImportData;
class SBVHImportOptions;

class FBVHImporter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreateAnimSequenceDelegate, FString, Path);

UCLASS(hidecategories = Object)
class CHARACTERGEN_API UBVHImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()
public:
	FOnCreateAnimSequenceDelegate OnCreateAnimSequenceDelegate;

	/** Object used to show import options for Alembic */
	UPROPERTY()
	TObjectPtr<UBVHImportSettings> ImportSettings;

	UPROPERTY()
	bool bShowOption;

	TArray<TWeakObjectPtr<UObject>> CreatedObjects;

	//~ Begin UObject Interface
	void PostInitProperties();
	//~ End UObject Interface

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual bool DoesSupportClass(UClass* Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	                                   const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	                                   bool& bOutOperationCanceled) override;
	//~ End UFactory Interface

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;

	void ShowImportOptionsWindow(TSharedPtr<SBVHImportOptions>& Options, FString FilePath,
	                             const FBVHImporter& Importer);

	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface

	UAnimSequence* ImportAnimation(USkeleton* Skeleton, UObject* Outer, FBVHImporter* importer);
	UAnimSequence* CreateAnimSequence(USkeleton* Skeleton, UObject* Outer, FBVHImporter* Importer);
};