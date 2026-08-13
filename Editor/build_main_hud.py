import unreal

result=unreal.CVADEditorAssetBuilder.build_all_widget_layouts()
if not result:
    raise RuntimeError('Main HUD layout build failed')
unreal.log('CVAD_MAIN_HUD_V2_BUILT')
