import unreal


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


blueprint_path = "/Game/CVAD/Blueprints/Characters/BP_AngelBoss"
blueprint = load(blueprint_path)
boss = unreal.get_default_object(blueprint.generated_class())

# All visual and animation resources live on BP_AngelBoss defaults. C++ only owns
# runtime behavior and replication, so designers can swap assets without recompiling.
boss.set_editor_property("boss_role_body_meshes", [
    load("/Game/GhostLady_S2/Meshes/Characters/Combines/SK_GhostLadyS2_A"),
    load("/Game/GhostLady_S2/Meshes/Characters/Combines/SK_GhostLadyS2_B"),
    load("/Game/GhostLady_S2/Meshes/Characters/Combines/SK_GhostLadyS2D_B"),
])
boss.set_editor_property("sword_boss_attack", load("/Game/GhostLady_S2/Animations/RootMotion/Knight/Anim_Knight_Attack2"))
boss.set_editor_property("wing_boss_attack", load("/Game/GhostLady_S2/Animations/RootMotion/FlyingAttack/Anim_Flying_FlashFwd"))
boss.set_editor_property("caster_boss_attack", load("/Game/GhostLady_S2/Animations/RootMotion/FlyingAttack/Anim_Flying_CallOfHeaven"))

wing_left = boss.get_editor_property("angel_wing_left")
wing_right = boss.get_editor_property("angel_wing_right")
sword = boss.get_editor_property("angel_sword")
wing_left.set_editor_property("skeletal_mesh_asset", load("/Game/GhostLady_S2/Meshes/Wings/SK_Wing_L"))
wing_right.set_editor_property("skeletal_mesh_asset", load("/Game/GhostLady_S2/Meshes/Wings/SK_Wing_R"))
sword.set_editor_property("skeletal_mesh_asset", load("/Game/GhostLady_S2/Meshes/Weapons/SK_LongSword"))

wing_anim = unreal.load_class(None, "/Game/GhostLady_S2/Animations/In-Place/Wings/Wings_AnimBP.Wings_AnimBP_C")
wing_left.set_editor_property("anim_class", wing_anim)
wing_right.set_editor_property("anim_class", wing_anim)

blueprint.modify()
unreal.EditorAssetLibrary.save_asset(blueprint_path, only_if_is_dirty=False)
unreal.log_warning("CVAD_CONFIGURED BP_AngelBoss visual and animation defaults")

# SkeletonArmy uses its own skeleton and cannot evaluate Manny's AnimBP. Keep the
# resource choice in the Blueprint while C++ owns only the runtime behavior.
minion_path = "/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion"
minion_bp = load(minion_path)
minion = unreal.get_default_object(minion_bp.generated_class())
minion.set_editor_property("minion_idle_animation", load("/Game/SkeletonArmy/Animations/Warlord/Skeleton_Idle"))
minion.get_editor_property("mesh").set_editor_property("anim_class",
    unreal.load_class(None, "/Game/CVAD/Animations/ABP_SkeletonMinion.ABP_SkeletonMinion_C"))
minion.modify()
unreal.EditorAssetLibrary.save_asset(minion_path, only_if_is_dirty=False)
unreal.log_warning("CVAD_CONFIGURED BP_SkeletonMinion compatible idle animation")
