import unreal

imc = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Input/IMC_Player")
move = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Input/Actions/IA_Move")
if not imc or not move:
    unreal.log_error("Missing IMC_Player or IA_Move")
else:
    mappings = list(imc.get_editor_property("mappings"))
    for index, mapping in enumerate(mappings):
        if mapping.action != move:
            continue
        key_name = str(mapping.key.get_editor_property("key_name"))
        modifiers = []
        if key_name in ("W", "S"):
            swizzle = unreal.new_object(unreal.InputModifierSwizzleAxis, imc)
            swizzle.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
            modifiers.append(swizzle)
        if key_name in ("A", "S"):
            modifiers.append(unreal.new_object(unreal.InputModifierNegate, imc))
        mapping.set_editor_property("modifiers", modifiers)
        mappings[index] = mapping
    imc.set_editor_property("mappings", mappings)
    unreal.EditorAssetLibrary.save_loaded_asset(imc, only_if_is_dirty=False)
    for mapping in imc.get_editor_property("mappings"):
        if mapping.action == move:
            names = [m.get_class().get_name() for m in mapping.get_editor_property("modifiers")]
            unreal.log("MOVE_FIXED key={} modifiers={}".format(mapping.key.get_editor_property("key_name"), names))
