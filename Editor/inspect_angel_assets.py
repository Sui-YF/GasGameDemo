import unreal

roots = ['/Game/GhostLady_S2/Blueprints', '/Game/GhostLady_S2/Meshes/Characters/Combines']
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for root in roots:
    for data in registry.get_assets_by_path(root, recursive=True):
        unreal.log('ANGEL_ASSET {} CLASS={}'.format(data.package_name, data.asset_class_path.asset_name))

for path in ['/Game/GhostLady_S2/Blueprints/Separates/BP_SP_GhostLadyS2_F',
             '/Game/GhostLady_S2/Blueprints/Separates/BP_SP_GhostLadyS2_E']:
    bp = unreal.load_asset(path)
    if not bp:
        continue
    unreal.log('ANGEL_BP {} PARENT={}'.format(path, bp.parent_class.get_name() if bp.parent_class else 'None'))
    cdo = unreal.get_default_object(bp.generated_class())
    for comp in cdo.get_components_by_class(unreal.SkeletalMeshComponent):
        mesh = comp.get_editor_property('skeletal_mesh_asset')
        unreal.log('ANGEL_COMPONENT {} MESH={}'.format(comp.get_name(), mesh.get_path_name() if mesh else 'None'))
