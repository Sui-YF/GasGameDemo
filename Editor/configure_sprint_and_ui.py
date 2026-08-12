import unreal

root = '/Game/CVAD'
tools = unreal.AssetToolsHelpers.get_asset_tools()
sprint = unreal.load_asset(root + '/Input/IA_Sprint')
if not sprint:
    factory = unreal.InputAction_Factory()
    sprint = tools.create_asset('IA_Sprint', root + '/Input', unreal.InputAction, factory)
    sprint.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_loaded_asset(sprint)
interact = unreal.load_asset(root + '/Input/IA_Interact')
if not interact:
    factory = unreal.InputAction_Factory()
    interact = tools.create_asset('IA_Interact', root + '/Input', unreal.InputAction, factory)
    interact.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_loaded_asset(interact)

context = unreal.load_asset(root + '/Input/IMC_Player')
if context:
    mappings = list(context.get_editor_property('mappings'))
    if not any(m.get_editor_property('action') == sprint for m in mappings):
        mapping = unreal.EnhancedActionKeyMapping()
        mapping.set_editor_property('action', sprint)
        key = unreal.Key()
        key.set_editor_property('key_name', 'LeftControl')
        mapping.set_editor_property('key', key)
        mappings.append(mapping)
    if not any(m.get_editor_property('action') == interact for m in mappings):
        mapping = unreal.EnhancedActionKeyMapping()
        mapping.set_editor_property('action', interact)
        key = unreal.Key(); key.set_editor_property('key_name', 'E')
        mapping.set_editor_property('key', key)
        mappings.append(mapping)
    context.set_editor_property('mappings', mappings)
    unreal.EditorAssetLibrary.save_loaded_asset(context, only_if_is_dirty=False)

pc_bp = unreal.load_asset(root + '/Blueprints/Player/BP_CVADPlayerController')
if not pc_bp:
    pc_bp = unreal.load_asset(root + '/Blueprints/Game/BP_CVADPlayerController')
if pc_bp:
    cdo = unreal.get_default_object(pc_bp.generated_class())
    cdo.set_editor_property('sprint_action', sprint)
    cdo.set_editor_property('interact_action', interact)
    pause_class = unreal.load_class(None, root + '/UI/WBP_Pause.WBP_Pause_C')
    cdo.set_editor_property('pause_widget_class', pause_class)
    cdo.set_editor_property('result_widget_class', unreal.load_class(None, root + '/UI/WBP_Result.WBP_Result_C'))
    unreal.EditorAssetLibrary.save_loaded_asset(pc_bp, only_if_is_dirty=False)
unreal.log('CVAD_SPRINT_AND_PAUSE_CONFIGURED')
