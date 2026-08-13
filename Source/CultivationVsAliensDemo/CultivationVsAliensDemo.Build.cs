using UnrealBuildTool;

public class CultivationVsAliensDemo : ModuleRules
{
    public CultivationVsAliensDemo(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "NetCore",
            "UMG",
            "Slate",
            "SlateCore"
            ,"AIModule"
            ,"NavigationSystem"
            ,"HTTP"
            ,"ApplicationCore"
            ,"Sockets"
        });
    }
}
