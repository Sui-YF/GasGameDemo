import unreal

captain = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Blueprints/Characters/BP_TPPMinionCaptain")
if captain:
    cdo = unreal.get_default_object(captain.generated_class())
    cdo.set_editor_property("balance_row_name", "Captain")
    cdo.set_editor_property("is_boss", True)
    unreal.EditorAssetLibrary.save_loaded_asset(captain)
    unreal.log("Configured BP_TPPMinionCaptain as replicated three-phase boss")
else:
    unreal.log_error("BP_TPPMinionCaptain not found")
