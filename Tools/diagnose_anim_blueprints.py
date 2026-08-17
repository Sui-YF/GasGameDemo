import unreal

result = unreal.CVADEditorAssetBuilder.diagnose_animation_blueprints()
unreal.log_warning("CVAD_DIAGNOSE_ANIM_RESULT=%s" % result)
