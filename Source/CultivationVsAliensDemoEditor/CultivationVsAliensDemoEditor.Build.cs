using UnrealBuildTool;

public class CultivationVsAliensDemoEditor : ModuleRules
{
    public CultivationVsAliensDemoEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "CultivationVsAliensDemo"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd",
            "UMG",
            "UMGEditor",
            "Slate",
            "SlateCore",
            "Kismet",
            "BlueprintGraph",
            "AnimGraph",
            "AnimGraphRuntime"
        });
    }
}
