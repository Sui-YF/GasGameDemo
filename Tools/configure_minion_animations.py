import unreal


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


blueprint_path = "/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion"
blueprint = load(blueprint_path)
minion = unreal.get_default_object(blueprint.generated_class())

minion.set_editor_property("minion_idle_animation", load("/Game/SkeletonArmy/Animations/Footman/Skeleton_Idle"))
minion.set_editor_property("minion_attack_animation", load("/Game/SkeletonArmy/Animations/Footman/Skeleton_1H_swing_left"))
minion.set_editor_property("minion_hit_animation", load("/Game/SkeletonArmy/Animations/Footman/Skeleton_Hit_from_front"))
minion.set_editor_property("minion_death_animation", load("/Game/SkeletonArmy/Animations/Footman/Skeleton_Dying_A"))

blueprint.modify()
unreal.EditorAssetLibrary.save_asset(blueprint_path, only_if_is_dirty=False)
unreal.log_warning("CVAD_CONFIGURED BP_SkeletonMinion attack/hit/death animations")
