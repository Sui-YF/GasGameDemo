import unreal

root='/Game/CVAD/Input'
context=unreal.load_asset(root+'/IMC_Player')
if not context: raise RuntimeError('Missing IMC_Player')

expected={
    '/Game/CVAD/Input/Actions/IA_Inventory':'Tab',
    '/Game/CVAD/Input/Actions/IA_Pause':'P',
}
mappings=list(context.get_editor_property('mappings'))
for action_path,key_name in expected.items():
    action=unreal.load_asset(action_path)
    if not action: raise RuntimeError('Missing '+action_path)
    existing=[m for m in mappings if m.get_editor_property('action')==action]
    if action.get_name() == 'IA_Pause':
        mappings=[m for m in mappings if m.get_editor_property('action')!=action]
        existing=[]
    if not any(m.get_editor_property('key').get_editor_property('key_name')==key_name for m in existing):
        mapping=unreal.EnhancedActionKeyMapping()
        mapping.set_editor_property('action',action)
        key=unreal.Key(); key.set_editor_property('key_name',key_name)
        mapping.set_editor_property('key',key)
        mappings.append(mapping)
        unreal.log('CVAD_UI_REPAIR added {} -> {}'.format(action.get_name(),key_name))
context.set_editor_property('mappings',mappings)
unreal.EditorAssetLibrary.save_loaded_asset(context,only_if_is_dirty=False)

pc=unreal.load_asset('/Game/CVAD/Blueprints/Game/BP_CVADPlayerController')
cdo=unreal.get_default_object(pc.generated_class())
cdo.set_editor_property('settings_widget_class',unreal.load_class(None,'/Game/CVAD/UI/WBP_Settings.WBP_Settings_C'))
cdo.set_editor_property('skill_tree_widget_class',unreal.load_class(None,'/Game/CVAD/UI/WBP_SkillTree.WBP_SkillTree_C'))
cdo.set_editor_property('save_slots_widget_class',unreal.load_class(None,'/Game/CVAD/UI/WBP_SaveSlots.WBP_SaveSlots_C'))
unreal.EditorAssetLibrary.save_loaded_asset(pc,only_if_is_dirty=False)
for m in mappings:
    a=m.get_editor_property('action')
    if a and a.get_name() in ('IA_Inventory','IA_Pause'):
        unreal.log('CVAD_UI_REPAIR mapping {} -> {}'.format(a.get_name(),m.get_editor_property('key').get_editor_property('key_name')))

if not unreal.CVADEditorAssetBuilder.build_all_widget_layouts():
    raise RuntimeError('Failed to update HUD pause-key hint')
