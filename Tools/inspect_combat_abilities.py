import unreal

paths = [
    "/Game/CVAD/Abilities/GA_SwitchStance",
    "/Game/CVAD/Abilities/GA_FlyingSword",
    "/Game/CVAD/Abilities/Attacks/GA_FlyingSword1",
    "/Game/CVAD/Abilities/GA_HeavyAttack",
]

for path in paths:
    bp = unreal.load_asset(path)
    if not bp:
        unreal.log_warning("MISSING " + path)
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    unreal.log_warning("ABILITY %s" % path)
    for prop in [
        "ability_input",
        "damage",
        "attack_distance",
        "attack_radius",
        "auto_target_nearest",
        "spawn_homing_sword",
        "homing_sword_count",
        "resource",
        "resource_cost",
    ]:
        try:
            unreal.log_warning("  %s=%s" % (prop, cdo.get_editor_property(prop)))
        except Exception as exc:
            unreal.log_warning("  %s ERROR %s" % (prop, exc))

print("CVAD_COMBAT_ABILITY_INSPECT_DONE")
