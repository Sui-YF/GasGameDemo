import unreal

path = '/Game/CVAD/Blueprints/Characters/BP_TPPMinionBoss'
bp = unreal.load_asset(path)
if not bp:
    source = unreal.load_asset('/Game/CVAD/Blueprints/Characters/BP_TPPMinionCaptain')
    if source:
        bp = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            'BP_TPPMinionBoss', '/Game/CVAD/Blueprints/Characters', source)

if not bp:
    raise RuntimeError('Could not create demo boss blueprint')

cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('balance_row_name', 'Boss')
cdo.set_editor_property('visual_cull_distance', 60000.0)
cdo.set_editor_property('network_cull_distance', 80000.0)
mesh = cdo.get_editor_property('mesh')
mesh.set_editor_property('relative_scale3d', unreal.Vector(1.65, 1.65, 1.65))
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log('CVAD_DEMO_BOSS_CREATED {}'.format(bp.get_path_name()))
