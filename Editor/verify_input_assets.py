import unreal

imc = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Input/IMC_Player")
controller_bp = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Blueprints/Game/BP_CVADPlayerController")
if not imc or not controller_bp:
    unreal.log_error("Missing IMC_Player or BP_CVADPlayerController")
else:
    mappings = imc.get_editor_property("mappings")
    look = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Input/Actions/IA_Look")
    has_mouse_look = any(m.action == look and str(m.key.get_editor_property("key_name")) == "Mouse2D" for m in mappings)
    cdo = unreal.get_default_object(controller_bp.generated_class())
    cdo.set_editor_property("player_mapping_context", imc)
    cdo.set_editor_property("look_action", look)
    unreal.EditorAssetLibrary.save_loaded_asset(controller_bp)
    unreal.log("INPUT_VERIFY mappings={} Mouse2DLook={} Context={} LookAction={}".format(
        len(mappings), has_mouse_look,
        cdo.get_editor_property("player_mapping_context").get_name(),
        cdo.get_editor_property("look_action").get_name()))
