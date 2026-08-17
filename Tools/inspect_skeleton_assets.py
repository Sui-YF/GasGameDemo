import unreal

for path in [
    "/Game/SkeletonArmy/Characters/Footman/SkeletonFootman_Skeleton",
    "/Game/SkeletonArmy/Characters/Footman/SK_SkeletonFootman",
]:
    obj = unreal.load_asset(path)
    if not obj:
        unreal.log_warning("MISSING " + path)
        continue
    unreal.log_warning("ASSET %s class=%s" % (path, obj.get_class().get_name()))
    for prop in [
        "preview_skeletal_mesh",
        "compatible_skeletons",
        "bone_tree",
        "skeleton",
        "additional_preview_skeletal_meshes",
    ]:
        try:
            value = obj.get_editor_property(prop)
            if hasattr(value, "get_path_name"):
                value = value.get_path_name()
            elif isinstance(value, unreal.Array):
                value = [getattr(v, "get_path_name", lambda: str(v))() for v in value]
            unreal.log_warning("  %s=%s" % (prop, value))
        except Exception as exc:
            unreal.log_warning("  %s ERROR %s" % (prop, exc))

print("CVAD_SKELETON_INSPECT_DONE")
