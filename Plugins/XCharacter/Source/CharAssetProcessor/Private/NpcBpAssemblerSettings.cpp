#include "NpcBpAssemblerSettings.h"
#include "Misc/MessageDialog.h"

UNpcBpAssemblerSettings::UNpcBpAssemblerSettings()
{
	SelectedCharacterBlueprint = TSoftClassPtr<ANpcBaseDup>(FSoftObjectPath(TEXT("/XCharacter/BP/NpcBpAssembler/BP_NPC.BP_NPC_C")));
	SelectedAnimBlueprint = TSoftClassPtr<UNpcAnimBaseDup>(FSoftObjectPath(TEXT("/XCharacter/BP/NpcBpAssembler/ABP_NPC.ABP_NPC_C")));
	SelectedAssetPath = "";
	SelectedBpPath = "";
	SelectedAbpPath = "";
	Python = IPythonScriptPlugin::Get();
}

bool UNpcBpAssemblerSettings::CheckSkmAsset(bool bJumpOutPassWindow = true)
{
	if (SelectedSkeletalMesh.IsNull())
	{
		FText ErrorMessage = FText::FromString(TEXT("'NPC角色骨骼网格体' 一栏不能为空"));
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);

		return false;
	}

	SelectedAssetPath = SelectedSkeletalMesh.ToSoftObjectPath().ToString();
	UE_LOG(LogTemp, Log, TEXT("Selected Skeletal Mesh Path: %s"), *SelectedAssetPath);

	FString PyCommand;
	if (bJumpOutPassWindow)
		PyCommand = FString::Printf(TEXT("import npc_bp_assembler.skm_asset_checker as sac\nimport importlib as i\ni.reload(sac)\nsac.check_skm_asset_cpp('%s', jump_out_pass_window=True)"), *SelectedAssetPath);
	else
		PyCommand = FString::Printf(TEXT("import npc_bp_assembler.skm_asset_checker as sac\nimport importlib as i\ni.reload(sac)\nsac.check_skm_asset_cpp('%s', jump_out_pass_window=False)"), *SelectedAssetPath);

	Python->ExecPythonCommand(*PyCommand);

	return true;
}

static FString GetBlueprintAssetPathFromClass(const TSoftClassPtr<UObject>& SoftClassPtr)
{
	UClass* Class = SoftClassPtr.Get();
	if (Class && Class->GetOuter())
	{
		return Class->GetOuter()->GetPathName();
	}
	return FString();
}

bool UNpcBpAssemblerSettings::AssembleNpcBp()
{
	SelectedAssetPath = SelectedSkeletalMesh.ToSoftObjectPath().ToString();
	SelectedBpPath = SelectedCharacterBlueprint.ToSoftObjectPath().GetAssetPathString();
	SelectedAbpPath = SelectedAnimBlueprint.ToSoftObjectPath().GetAssetPathString();
	UE_LOG(LogTemp, Log, TEXT("Selected Skeletal Mesh Path: %s"), *SelectedAssetPath);
	UE_LOG(LogTemp, Log, TEXT("Selected Character Blueprint Path: %s"), *SelectedBpPath);
	UE_LOG(LogTemp, Log, TEXT("Selected Anim Blueprint Path: %s"), *SelectedAbpPath);

	if (SelectedSkeletalMesh.IsNull())
	{
		FText ErrorMessage = FText::FromString(TEXT("'NPC角色骨骼网格体' 一栏不能为空"));
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);

		return false;
	}

	if (SelectedCharacterBlueprint.IsNull() || SelectedAnimBlueprint.IsNull())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("'角色基类蓝图' 或 '角色基类动画蓝图' 一栏不能为空")));
		return false;
	}

	FString PyCommand = FString::Printf(
		TEXT("import npc_bp_assembler.skm_asset_checker as sac\nimport npc_bp_assembler.npc_bp_assembler as nba\n\nimport importlib as i\ni.reload(sac)\ni.reload(nba)\n\nif sac.check_skm_asset_cpp('%s', jump_out_pass_window=False):\n    nba.assemble_npc_bp_cpp('%s', '%s', '%s')"),
		*SelectedAssetPath,
		*SelectedAssetPath,
		*SelectedBpPath,
		*SelectedAbpPath);
	 Python->ExecPythonCommand(*PyCommand);

	return true;
}

void UNpcBpAssemblerSettings::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
}

UNpcBpAssemblerSettings* UNpcBpAssemblerSettings::Get()
{
	return GetMutableDefault<UNpcBpAssemblerSettings>();
}