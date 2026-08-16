import unreal

ok = unreal.CVADEditorAssetBuilder.build_minion_animation_blueprint()
unreal.log_warning("CVAD_BUILD_MINION_ANIM=%s" % ok)
