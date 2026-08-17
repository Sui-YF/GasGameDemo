import unreal

paths = [
    "/Game/CVAD/Abilities/GA_FlyingSword",
    "/Game/CVAD/Abilities/Attacks/GA_FlyingSword1",
    "/Game/CVAD/Abilities/Attacks/GA_FlyingSword2",
    "/Game/CVAD/Abilities/Attacks/GA_FlyingSword3",
]

for path in paths:
    bp = unreal.load_asset(path)
    if not bp:
        unreal.log_warning("MISSING " + path)
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("auto_target_nearest", True)
    cdo.set_editor_property("spawn_homing_sword", True)
    cdo.set_editor_property("homing_sword_count", 1)
    bp.modify()
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log_warning("CONFIGURED " + path)
