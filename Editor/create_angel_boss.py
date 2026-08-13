import unreal

tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/CVAD/Blueprints/Characters'
name='BP_AngelBoss'
bp=unreal.load_asset(folder+'/'+name)
if not bp:
    source=unreal.load_asset(folder+'/BP_TPPMinionBoss')
    bp=tools.duplicate_asset(name,folder,source)
if not bp:
    raise RuntimeError('Unable to create BP_AngelBoss')
cdo=unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('balance_row_name','Boss')
cdo.set_editor_property('visual_cull_distance',80000.0)
cdo.set_editor_property('network_cull_distance',100000.0)
mesh_comp=cdo.get_editor_property('mesh')
mesh_comp.set_editor_property('skeletal_mesh_asset',unreal.load_asset('/Game/GhostLady_S2/Meshes/Characters/Combines/SK_GhostLadyS2_A'))
mesh_comp.set_editor_property('anim_class',unreal.load_class(None,'/Game/GhostLady_S2/Animations/In-Place/MoveBasic/Flying/FemaleFlying_AnimBP.FemaleFlying_AnimBP_C'))
mesh_comp.set_editor_property('relative_location',unreal.Vector(0,0,-90))
mesh_comp.set_editor_property('relative_rotation',unreal.Rotator(0,-90,0))
mesh_comp.set_editor_property('relative_scale3d',unreal.Vector(1.2,1.2,1.2))
unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)

unreal.EditorLoadingAndSavingUtils.load_map('/Game/CVAD/Maps/L_BattlePrototype')
boss_class=unreal.load_class(None,folder+'/'+name+'.'+name+'_C')
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_class().get_name()=='BP_MinionSpawner_C':
        actor.set_editor_property('boss_class',boss_class)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
unreal.log('CVAD_ANGEL_BOSS_CONFIGURED')
