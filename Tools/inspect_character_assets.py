import unreal

for asset_path in (
    "/Game/CVAD/Blueprints/Characters/BP_TPPMinion",
    "/Game/CVAD/Blueprints/Characters/BP_TPPMinionCaptain",
    "/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion",
    "/Game/CVAD/Blueprints/Characters/BP_LanfangCharacter",
):
    blueprint = unreal.load_asset(asset_path)
    if not blueprint:
        unreal.log_error("CVAD_INSPECT missing " + asset_path)
        continue
    generated_class = blueprint.generated_class()
    default_actor = unreal.get_default_object(generated_class)
    mesh = default_actor.get_editor_property("mesh")
    unreal.log_warning(
        "CVAD_INSPECT %s mesh=%s anim=%s location=%s rotation=%s scale=%s"
        % (
            asset_path,
            mesh.get_editor_property("skeletal_mesh_asset"),
            mesh.get_editor_property("anim_class"),
            mesh.get_editor_property("relative_location"),
            mesh.get_editor_property("relative_rotation"),
            mesh.get_editor_property("relative_scale3d"),
        )
    )
