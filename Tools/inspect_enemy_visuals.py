import unreal

for path in (
    "/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion",
    "/Game/CVAD/Blueprints/Characters/BP_TPPMinion",
    "/Game/CVAD/Blueprints/Characters/BP_AngelBoss",
):
    bp = unreal.load_asset(path)
    if not bp:
        unreal.log_warning("CVAD_ENEMY missing %s" % path)
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    mesh = cdo.get_editor_property("mesh")
    unreal.log_warning(
        "CVAD_ENEMY %s mesh=%s anim=%s hidden=%s visible=%s loc=%s rot=%s scale=%s bounds=%s"
        % (
            path,
            mesh.get_editor_property("skeletal_mesh_asset"),
            mesh.get_editor_property("anim_class"),
            mesh.get_editor_property("hidden_in_game"),
            mesh.get_editor_property("visible"),
            mesh.get_editor_property("relative_location"),
            mesh.get_editor_property("relative_rotation"),
            mesh.get_editor_property("relative_scale3d"),
            mesh.get_editor_property("bounds_scale"),
        )
    )
