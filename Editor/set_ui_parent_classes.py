import unreal

menu_parent = unreal.load_class(None, '/Script/CultivationVsAliensDemo.CVADMenuWidget')
base_parent = unreal.load_class(None, '/Script/CultivationVsAliensDemo.CVADUserWidget')
for name in ('WBP_MainMenu', 'WBP_Lobby', 'WBP_Result'):
    bp = unreal.load_asset('/Game/CVAD/UI/' + name)
    if bp:
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, menu_parent)
        unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
for name in ('WBP_Pause','WBP_Settings','WBP_NameEntry','WBP_SkillTree'):
    bp = unreal.load_asset('/Game/CVAD/UI/' + name)
    if bp:
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, base_parent)
        unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log('CVAD_UI_PARENT_CLASSES_SET')
