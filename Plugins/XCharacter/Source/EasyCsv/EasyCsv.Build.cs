// Copyright Jared Therriault 2019, 2022

using UnrealBuildTool;
using System.IO;

public class EasyCsv : ModuleRules
{
    public EasyCsv(ReadOnlyTargetRules target) : base(target)
    {
        PrivateIncludePaths.AddRange(new string[] { Path.Combine(ModuleDirectory, "Private") });
        PublicIncludePaths.AddRange(new string[] { Path.Combine(ModuleDirectory, "Public") });
        
        PublicDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "Slate", "SlateCore", "UMG"});

        PrivateDependencyModuleNames.AddRange(new string[] { "Engine", "Core", "CoreUObject", "InputCore" });

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
    }
}
