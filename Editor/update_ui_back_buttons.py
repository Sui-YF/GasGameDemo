import unreal

if not unreal.CVADEditorAssetBuilder.update_ui_back_buttons():
    raise RuntimeError('One or more UI back buttons could not be updated')

unreal.log('CVAD_UI_BACK_BUTTONS_UPDATED')
