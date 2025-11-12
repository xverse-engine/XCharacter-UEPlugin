// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XSequencer : ModuleRules
{
	public XSequencer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				// ... add other private include paths required here ...
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", "HTTP", "Protocol", "WebSockets"
				// ... add other public dependencies that you statically link with here ...
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"ApplicationCore",
				"Engine",

				// Slate Related
				"Slate",
				"SlateCore",
				"EditorStyle",
				"UMG",
				"EditorScriptingUtilities",
				"Blutility",

				// TTS Related
				"Json",
				"JsonUtilities",
				"AnimationModifiers",

				// Sequencer
				"LevelSequence",
				"MovieScene",
				"MovieSceneTracks",

				// Sequence Rendering
				"SequencerScripting",
				"SequencerScriptingEditor",
				"MovieSceneTools",
				"MovieSceneCapture",
				"AVIWriter",

				// Camera
				"CinematicCamera",
				"RuntimeAudioImporter",
				
				// File Dialog
				"EasyFileDialog",
				"EasyCsv",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
