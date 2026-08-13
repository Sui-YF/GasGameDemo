import unreal

SOURCE_BP = '/Game/CVAD/Blueprints/Characters/BP_TPPMinion'
TARGET_FOLDER = '/Game/CVAD/Blueprints/Characters'
TARGET_NAME = 'BP_SkeletonMinion'
FOOTMAN_BP = '/Game/SkeletonArmy/Blueprints/BP_SkeletonFootman'
FOOTMAN_MESH = '/Game/SkeletonArmy/Characters/Footman/SK_SkeletonFootman'
LEVEL = '/Game/CVAD/Maps/L_BattlePrototype'

tools = unreal.AssetToolsHelpers.get_asset_tools()
target = unreal.load_asset(TARGET_FOLDER + '/' + TARGET_NAME)
if not target:
    target = tools.duplicate_asset(TARGET_NAME, TARGET_FOLDER, unreal.load_asset(SOURCE_BP))
if not target:
    raise RuntimeError('Could not create BP_SkeletonMinion')

cdo = unreal.get_default_object(target.generated_class())
mesh = cdo.get_editor_property('mesh')
footman_mesh = unreal.load_asset(FOOTMAN_MESH)
if not footman_mesh:
    raise RuntimeError('Skeleton footman mesh was not found')
mesh.set_editor_property('skeletal_mesh_asset', footman_mesh)

# Reuse the imported pack's tested animation class when it has one.
source_pack_bp = unreal.load_asset(FOOTMAN_BP)
if source_pack_bp:
    source_cdo = unreal.get_default_object(source_pack_bp.generated_class())
    source_mesh = source_cdo.get_component_by_class(unreal.SkeletalMeshComponent)
    anim_class = source_mesh.get_editor_property('anim_class') if source_mesh else None
    if anim_class:
        mesh.set_editor_property('anim_class', anim_class)
        unreal.log('CVAD_SKELETON_ANIM_CLASS {}'.format(anim_class.get_path_name()))

mesh.set_editor_property('relative_location', unreal.Vector(0, 0, -90))
mesh.set_editor_property('relative_rotation', unreal.Rotator(0, -90, 0))
mesh.set_editor_property('relative_scale3d', unreal.Vector(1, 1, 1))
cdo.set_editor_property('balance_row_name', 'Minion')
cdo.set_editor_property('visual_cull_distance', 30000.0)
cdo.set_editor_property('network_cull_distance', 50000.0)
unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False)

unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
minion_class = unreal.load_class(None, TARGET_FOLDER + '/' + TARGET_NAME + '.' + TARGET_NAME + '_C')
changed = 0
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_class().get_name() == 'BP_MinionSpawner_C':
        actor.set_editor_property('minion_class', minion_class)
        changed += 1
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('CVAD_SKELETON_MINION_CONFIGURED spawners={}'.format(changed))
