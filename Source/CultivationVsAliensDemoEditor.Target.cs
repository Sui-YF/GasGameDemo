using UnrealBuildTool;

public class CultivationVsAliensDemoEditorTarget : TargetRules
{
    public CultivationVsAliensDemoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
        ExtraModuleNames.Add("CultivationVsAliensDemo");
    }
}
