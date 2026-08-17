import unreal

paths = [
    "/Game/CVAD/Animations/ABP_LanFang_Normal",
    "/Game/CVAD/Animations/ABP_LanFang_FlyingSword",
    "/Game/CVAD/Animations/ABP_LanFang_FlyingSwordV2",
    "/Game/CVAD/Animations/ABP_SkeletonMinion",
    "/Game/CVAD/Animations/BS_LanFang_FlyingSword",
    "/Game/CVAD/Animations/BS_SkeletonMinion",
]

for path in paths:
    obj = unreal.load_asset(path)
    if not obj:
        unreal.log_error("MISSING " + path)
        continue
    unreal.log_warning("ASSET %s class=%s name=%s" % (path, obj.get_class().get_name(), obj.get_name()))
    try:
        unreal.log_warning("  target_skeleton=%s" % obj.get_editor_property("target_skeleton"))
    except Exception as exc:
        unreal.log_warning("  target_skeleton ERROR %s" % exc)
    try:
        unreal.log_warning("  skeleton=%s" % obj.get_editor_property("skeleton"))
    except Exception as exc:
        unreal.log_warning("  skeleton ERROR %s" % exc)

    cls = None
    try:
        cls = obj.generated_class()
        unreal.log_warning("  generated_class=%s parent=%s" % (cls.get_name(), cls.get_super_class().get_name() if cls.get_super_class() else "None"))
    except Exception as exc:
        unreal.log_warning("  generated_class ERROR %s" % exc)

    if cls:
        try:
            cdo = unreal.get_default_object(cls)
            unreal.log_warning("  cdo_class=%s" % cdo.get_class().get_name())
        except Exception as exc:
            unreal.log_warning("  cdo ERROR %s" % exc)

    try:
        samples = obj.get_editor_property("sample_data")
        unreal.log_warning("  sample_data_count=%d" % len(samples))
        for i, sample in enumerate(samples):
            anim = sample.get_editor_property("animation")
            unreal.log_warning("    sample[%d] anim=%s" % (i, anim.get_path_name() if anim else "None"))
    except Exception:
        pass

    try:
        graphs = obj.get_editor_property("graphs")
        unreal.log_warning("  graphs_count=%d" % len(graphs))
    except Exception:
        pass

print("CVAD_ANIM_INSPECT_DONE")
