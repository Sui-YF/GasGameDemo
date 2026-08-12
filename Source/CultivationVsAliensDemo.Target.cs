using UnrealBuildTool;

public class CultivationVsAliensDemoTarget : TargetRules
{
    public CultivationVsAliensDemoTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
        ExtraModuleNames.Add("CultivationVsAliensDemo");
    }
}
