// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CharacterGenEditor : ModuleRules
{
	public CharacterGenEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"CharacterGen",
				"Protocol",
				"LevelSequence",
				"MovieScene",
				"MovieSceneTracks",
				"CharAssetProcessor",
				"EasyCsv",
				"EasyFileDialog"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorFramework",
				"UnrealEd",
				"MainFrame",
				"PropertyEditor",
				"RenderCore",
				"RHI",
				"LevelEditor",
				"EditorStyle",
				"HTTP",
				"Json",
				"JsonUtilities",
				"Projects",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"ApplicationCore",
				"EditorSubsystem",
				"RuntimeAudioImporter"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}

