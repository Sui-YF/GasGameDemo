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
]

configured = 0
for path in paths:
    sequence = unreal.EditorAssetLibrary.load_asset(path)
    if not sequence:
        unreal.log_warning("Missing root motion animation " + path)
        continue
    sequence.set_editor_property("enable_root_motion", True)
    sequence.set_editor_property("force_root_lock", False)
    sequence.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False)
    configured += 1
unreal.log("ROOT_MOTION_ENABLED animations={}".format(configured))
