import unreal

builder = unreal.CVADEditorAssetBuilder
results = [
    ("main", builder.build_all_widget_layouts()),
    ("skeletons", builder.build_all_ui_control_skeletons()),
    ("back_buttons", builder.update_ui_back_buttons()),
    ("settings_key_labels", builder.update_settings_key_labels()),
]
for name, ok in results:
    unreal.log_warning("CVAD_UI_REBUILD %s=%s" % (name, ok))
unreal.EditorAssetLibrary.save_directory("/Game/CVAD/UI", only_if_is_dirty=True, recursive=True)
