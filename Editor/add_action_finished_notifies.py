import unreal

paths = [
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack1",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack2",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack3",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack4",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack5",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Roll_Fwd",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Roll_Bwd",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Roll_Lt",
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Roll_Rt",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_01",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_02",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_03",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Equip",
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_UnEquip",
]
notify_class = unreal.CVADAnimNotify_ActionFinished
library = getattr(unreal, "AnimationLibrary", None) or getattr(unreal, "AnimationBlueprintLibrary", None)
added = 0
for path in paths:
    sequence = unreal.EditorAssetLibrary.load_asset(path)
    if not sequence:
        continue
    # Remove prior copies so this script remains idempotent.
    if hasattr(library, "remove_animation_notify_events_by_name"):
        library.remove_animation_notify_events_by_name(sequence, "Action Finished")
    time = max(0.0, sequence.get_editor_property("sequence_length") - 0.02)
    library.add_animation_notify_event(sequence, "1", time, notify_class)
    unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False)
    added += 1
unreal.log("ACTION_FINISHED_NOTIFIES_ADDED animations={}".format(added))
