import unreal

skeleton = unreal.EditorAssetLibrary.load_asset("/Game/LanFang/Meshes/Characters/Combines/SK_FemaleBase_Skeleton")
if not skeleton:
    unreal.log_error("LanFang skeleton not found")
else:
    interesting = [name for name in dir(skeleton) if any(token in name.lower() for token in ("rig", "preview", "retarget"))]
    unreal.log("LANFANG_SKELETON_PROPERTIES " + ",".join(interesting))
    saved = unreal.EditorAssetLibrary.save_loaded_asset(skeleton, only_if_is_dirty=False)
    unreal.log("LANFANG_SKELETON_RESAVE {}".format(saved))
