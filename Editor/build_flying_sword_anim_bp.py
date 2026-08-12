import unreal

ok = unreal.CVADEditorAssetBuilder.build_flying_sword_animation_blueprint()
unreal.log('CVAD_BUILD_FLYING_SWORD_ANIM_BP={}'.format(ok))
if not ok:
    raise RuntimeError('Failed to build flying sword animation blueprint')
