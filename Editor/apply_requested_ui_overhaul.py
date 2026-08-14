import unreal

source = "/Game/CVAD/UI/WBP_Lobby"
multiplayer = "/Game/CVAD/UI/WBP_Multiplayer"
if not unreal.EditorAssetLibrary.does_asset_exist(multiplayer):
    if not unreal.EditorAssetLibrary.duplicate_asset(source, multiplayer):
        raise RuntimeError("Could not create WBP_Multiplayer")

builder = unreal.CVADEditorAssetBuilder
if not builder.build_all_ui_control_skeletons():
    raise RuntimeError("One or more UI pages could not be rebuilt")
if not builder.build_all_widget_layouts():
    raise RuntimeError("Main menu/HUD layout update failed")
builder.update_ui_back_buttons()
builder.update_settings_key_labels()
unreal.EditorAssetLibrary.save_directory("/Game/CVAD/UI", only_if_is_dirty=False, recursive=True)
unreal.log("Requested save, skill, outfit and multiplayer UI overhaul applied")
