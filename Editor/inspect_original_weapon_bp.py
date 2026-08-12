import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/LanFang/Blueprints/Combines/BP_CB_LanFang_Weapons")
if not bp:
    unreal.log_error("Original weapon blueprint missing")
else:
    cdo = unreal.get_default_object(bp.generated_class())
    for component in cdo.get_components_by_class(unreal.SkeletalMeshComponent):
        mesh = component.get_editor_property("skeletal_mesh_asset")
        parent = component.get_attach_parent()
        unreal.log("ORIGINAL_WEAPON_COMPONENT name={} mesh={} parent={} socket={} loc={} rot={} scale={} visible={}".format(
            component.get_name(), mesh.get_path_name() if mesh else "None",
            parent.get_name() if parent else "None", component.get_attach_socket_name(),
            component.get_editor_property("relative_location"), component.get_editor_property("relative_rotation"),
            component.get_editor_property("relative_scale3d"), component.get_editor_property("visible")))
