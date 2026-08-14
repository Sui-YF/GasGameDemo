import unreal

if not unreal.CVADEditorAssetBuilder.update_settings_key_labels():
    raise RuntimeError("Failed to add one or more settings key labels")

unreal.log("Added current-key text labels to WBP_Settings")
