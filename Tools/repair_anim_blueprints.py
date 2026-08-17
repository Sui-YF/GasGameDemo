import unreal

result = unreal.CVADEditorAssetBuilder.repair_animation_blueprints()
unreal.log_warning("CVAD_REPAIR_ANIM_RESULT=%s" % result)
