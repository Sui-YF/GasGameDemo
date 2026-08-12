import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Blueprints/Characters/BP_TPPMinion")
if not bp:
    unreal.log_error("BP_TPPMinion missing")
else:
    cdo = unreal.get_default_object(bp.generated_class())
    mesh = cdo.get_editor_property("mesh")
    unreal.log("MINION_VISUAL mesh={} anim={} visible={} hidden_game={} owner_no_see={} scale={} location={} rotation={}".format(
        mesh.get_editor_property("skeletal_mesh_asset"),
        mesh.get_editor_property("anim_class"),
        mesh.get_editor_property("visible"),
        mesh.get_editor_property("hidden_in_game"),
        mesh.get_editor_property("owner_no_see"),
        mesh.get_editor_property("relative_scale3d"),
        mesh.get_editor_property("relative_location"),
        mesh.get_editor_property("relative_rotation")))
