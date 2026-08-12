import unreal

path = '/Game/CVAD/UI/WBP_SkillTree'
if not unreal.load_asset(path):
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/CultivationVsAliensDemo.CVADUserWidget'))
    unreal.AssetToolsHelpers.get_asset_tools().create_asset('WBP_SkillTree', '/Game/CVAD/UI', unreal.WidgetBlueprint, factory)

if not unreal.CVADEditorAssetBuilder.build_all_ui_control_skeletons():
    raise RuntimeError('One or more UI control skeletons failed')
unreal.log('CVAD_UI_CONTROL_SKELETONS_BUILT')
