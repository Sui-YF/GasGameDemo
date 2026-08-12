import unreal

imc = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Input/IMC_Player")
move = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Input/Actions/IA_Move")
if not imc or not move:
    unreal.log_error("Missing IMC_Player or IA_Move")
else:
    for mapping in imc.get_editor_property("mappings"):
        if mapping.action == move:
            modifiers = [modifier.get_class().get_name() for modifier in mapping.get_editor_property("modifiers")]
            unreal.log("MOVE_MAPPING key={} modifiers={}".format(mapping.key.get_editor_property("key_name"), modifiers))
