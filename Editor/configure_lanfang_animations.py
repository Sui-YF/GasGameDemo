import unreal

player = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Blueprints/Characters/BP_LanfangCharacter")
anim_class = unreal.load_class(None, "/Game/CVAD/Animations/ABP_LanFang_Normal.ABP_LanFang_Normal_C")
if player and anim_class:
    mesh = unreal.get_default_object(player.generated_class()).get_editor_property("mesh")
    mesh.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    mesh.set_editor_property("anim_class", anim_class)
    unreal.EditorAssetLibrary.save_loaded_asset(player)

assignments = {
    "GA_LightAttack": "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack1",
    "GA_HeavyAttack": "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack4",
    "GA_Dodge": "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Roll_Fwd",
    "GA_FlyingSword": "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_01",
    "GA_SwitchStance": "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Equip",
}
for ability_name, animation_path in assignments.items():
    ability = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Abilities/" + ability_name)
    animation = unreal.EditorAssetLibrary.load_asset(animation_path)
    if ability and animation:
        unreal.get_default_object(ability.generated_class()).set_editor_property("attack_animation", animation)
        unreal.EditorAssetLibrary.save_loaded_asset(ability)
        unreal.log("LANFANG_ANIM {}={}".format(ability_name, animation.get_name()))

switch_ability = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Abilities/GA_SwitchStance")
un_equip = unreal.EditorAssetLibrary.load_asset("/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_UnEquip")
if switch_ability and un_equip:
    unreal.get_default_object(switch_ability.generated_class()).set_editor_property("alternate_animation", un_equip)
    unreal.EditorAssetLibrary.save_loaded_asset(switch_ability)
