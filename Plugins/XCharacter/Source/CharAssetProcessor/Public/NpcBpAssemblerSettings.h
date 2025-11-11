#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"
#include "Engine/SkeletalMesh.h"
#include "NpcBaseDup.h"
#include "NpcAnimBaseDup.h"
#include "IPythonScriptPlugin.h"

#include "NpcBpAssemblerSettings.generated.h"


UCLASS()
class CHARASSETPROCESSOR_API UNpcBpAssemblerSettings : public UObject
{
	GENERATED_BODY()
public:
	// NPC角色骨骼规范检查
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "选择需要检查和组装的骨骼网格体", meta = (AllowedClasses = "SkeletalMesh", DisplayName = "NPC角色骨骼网格体"))
	TSoftObjectPtr<USkeletalMesh> SelectedSkeletalMesh = nullptr;

	// NPC角色蓝图装配器
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "选择需要继承的蓝图基类（非特殊需要保持默认即可）", meta = (AllowedClasses = "NpcBaseDup", AllowAbstract = "false", DisplayName = "角色基类蓝图"))
	TSoftClassPtr<ANpcBaseDup> SelectedCharacterBlueprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "选择需要继承的蓝图基类（非特殊需要保持默认即可）", meta = (AllowedClasses = "NpcAnimBaseDup", AllowAbstract = "false", DisplayName = "角色基类动画蓝图"))
	TSoftClassPtr<UNpcAnimBaseDup> SelectedAnimBlueprint;

	UNpcBpAssemblerSettings();

	bool CheckSkmAsset(bool bJumpOutPassWindow);

	bool AssembleNpcBp();

	/** Accessor and initializer*/
	static UNpcBpAssemblerSettings* Get();

public:
	virtual void Serialize(class FArchive& Archive) override;

private:
	FString SelectedAssetPath;
	FString SelectedBpPath;
	FString SelectedAbpPath;
	IPythonScriptPlugin* Python;
};
