// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ImportAssetsBPLib.generated.h"

class UAssetImportData;

/**
 * 
 */
UCLASS()
class XSEQUENCER_API UImportAssetsBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
     * 将指定素材导入引擎，导入网格请不要使用此方法，而是使用 Static Mesh 和 Skeletal Mesh 的专用方法
     * @param SourcePath			素材的原始路径，格式例如 C:/Temp/MyAsset.png
     * @param DestinationPath		导入后的路径，格式例如 /Game/Assets/MyAsset
     * @return 导入素材的指针
     */
	UFUNCTION(BlueprintCallable, Category = "Import Assets")
	static UObject* ImportAsset(const FString& SourcePath,
	                            const FString& DestinationPath);

	/**
	 * 将指定的 Static Mesh 导入引擎
	 * @param SourcePath			素材的原始路径，格式例如 C:/Temp/static_mesh.fbx
	 * @param DestinationPath		导入后的路径，格式例如 /Game/Assets/StaticMesh
	 * @return 导入素材的指针
	 */
	UFUNCTION(BlueprintCallable, Category = "Import Assets")
	static UStaticMesh* ImportStaticMesh(const FString& SourcePath,
	                                     const FString& DestinationPath);

	/**
	 * 将指定的 Skeletal Mesh 导入引擎
	 * @param SourcePath			素材的原始路径，格式例如 C:/Temp/skeletal_mesh.fbx
	 * @param DestinationPath		导入后的路径，格式例如 /Game/Assets/SkeletalMesh
	 * @return 导入素材的指针
	 */
	UFUNCTION(BlueprintCallable, Category = "Import Assets")
	static USkeletalMesh* ImportSkeletalMesh(const FString& SourcePath,
	                                         const FString& DestinationPath);

	/**
	 * 获取导入素材的原始路径
	 * @param AssetPath			素材路径，格式例如 /Game/Assets/SkeletalMesh
	 * @return 导入素材的原始路径
	 */
	UFUNCTION(BlueprintCallable, Category = "Import Assets")
	static FString GetAssetImportPath(const FString& AssetPath);

	/**
	 * 设置新的导入素材的原始路径
	 * @param AssetPath			素材路径，格式例如 /Game/Assets/SkeletalMesh
	 * @param NewImportPath		新的导入路径，格式例如 C:/Temp/skeletal_mesh.fbx
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Import Assets")
	static bool SetAssetImportPath(const FString& AssetPath,
	                               FString NewImportPath);

	static UAssetImportData* GetAssetImportData(UObject* Asset);

	static UAssetImportTask* CreateImportTask(const FString& SourcePath,
	                                          const FString& DestinationPath,
	                                          UFactory* ExtraFactory,
	                                          UObject* ExtraOptions);

	static UObject* ProcessImportTask(UAssetImportTask* ImportTask);
};
