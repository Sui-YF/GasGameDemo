import unreal

pc=unreal.load_asset('/Game/CVAD/Blueprints/Game/BP_CVADPlayerController')
gm=unreal.load_asset('/Game/CVAD/Blueprints/Game/BP_CVADGameMode')
for label,bp in [('PC',pc),('GM',gm)]:
    if not bp:
        unreal.log_error('CVAD_UI_CHECK missing '+label)
        continue
    cdo=unreal.get_default_object(bp.generated_class())
    unreal.log('CVAD_UI_CHECK {} class={}'.format(label,bp.generated_class().get_path_name()))
    if label=='PC':
        for prop in ('player_mapping_context','inventory_action','pause_action','inventory_widget_class','pause_widget_class','settings_widget_class','skill_tree_widget_class','save_slots_widget_class'):
            unreal.log('CVAD_UI_CHECK {}={}'.format(prop,cdo.get_editor_property(prop)))
    else:
        unreal.log('CVAD_UI_CHECK player_controller_class={}'.format(cdo.get_editor_property('player_controller_class')))

unreal.EditorLoadingAndSavingUtils.load_map('/Game/CVAD/Maps/L_CastleBattle')
ws=unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
unreal.log('CVAD_UI_CHECK castle_gamemode={}'.format(ws.get_editor_property('default_game_mode')))
