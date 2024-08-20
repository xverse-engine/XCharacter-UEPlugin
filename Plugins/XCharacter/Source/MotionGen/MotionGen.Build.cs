// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MotionGen : ModuleRules
{
	public MotionGen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUsePrecompiled = true;
        PrecompileForTargets = PrecompileTargetsType.Any;
        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "AnimationCore",
                "Engine",
                "InputCore",
                "Slate",
                "SlateCore",
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
                "AdvancedPreviewScene",

                "Persona",
                "PropertyEditor",
                "BlueprintGraph",
                "AnimGraph",
                "AnimGraphRuntime",
				"IKRig",
				"IKRigEditor",
                "PropertyEditor"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "LevelEditor",
                "UnrealEd",
                "EditorStyle",
				// ... add private dependencies that you statically link with here ...	
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
