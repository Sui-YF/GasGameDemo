import unreal

builder = unreal.CVADEditorAssetBuilder
unreal.log_warning("CVAD_REBUILD UI=%s" % builder.build_all_ui_control_skeletons())
unreal.log_warning("CVAD_REBUILD ANIM=%s" % builder.build_flying_sword_animation_blueprint())
unreal.EditorAssetLibrary.save_directory("/Game/CVAD", only_if_is_dirty=True, recursive=True)
