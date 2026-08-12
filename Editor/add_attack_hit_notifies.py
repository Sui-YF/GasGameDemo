import unreal

paths = [
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack1",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack2",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack3",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack4",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack5",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_01",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_02",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_03",
]
library = getattr(unreal, "AnimationLibrary", None) or unreal.AnimationBlueprintLibrary
for path in paths:
    sequence = unreal.load_asset(path)
    if not sequence:
        continue
    library.remove_animation_notify_events_by_name(sequence, "Attack Hit")
    length = sequence.get_editor_property("sequence_length")
    # Demo tuning: impact occurs slightly before the middle of the authored attack.
    library.add_animation_notify_event(sequence, "Attack", length * 0.45, unreal.CVADAnimNotify_AttackHit)
    unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False)
    unreal.log("CVAD_ATTACK_NOTIFY {} time={:.3f}".format(sequence.get_name(), length * 0.45))
