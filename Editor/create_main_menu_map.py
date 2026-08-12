import unreal

MAP_PATH = "/Game/CVAD/Maps/L_MainMenu"

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    unreal.log("CVAD_MAIN_MENU_MAP_ALREADY_EXISTS")
else:
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem.new_level(MAP_PATH):
        raise RuntimeError("Unable to create L_MainMenu")
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property("kill_z", -100000.0)
    subsystem.save_current_level()
    unreal.log("CVAD_MAIN_MENU_MAP_CREATED")

