import unreal

asset_path = "/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion"
blueprint = unreal.load_asset(asset_path)
generated_class = blueprint.generated_class()
default_actor = unreal.get_default_object(generated_class)
mesh = default_actor.get_editor_property("mesh")
idle = unreal.load_asset("/Game/SkeletonArmy/Animations/Footman/Skeleton_Idle")
mesh.set_editor_property("relative_rotation", unreal.Rotator(0.0, -90.0, 0.0))
mesh.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
mesh.set_editor_property("animation_data", unreal.SingleAnimationPlayData(anim_to_play=idle, saved_looping=True, saved_play_rate=1.0))
blueprint.modify()
unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
unreal.log_warning("CVAD_FIXED skeleton minion rotation and compatible idle animation")
