import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/UI/WBP_HUD")
unreal.log(f"WIDGET_BP_TYPE={type(bp)}")
unreal.log(f"WIDGET_BP_DIR={[name for name in dir(bp) if 'widget' in name.lower() or 'tree' in name.lower()]}")
try:
    tree = bp.get_editor_property("widget_tree")
    unreal.log(f"TREE_TYPE={type(tree)}")
    unreal.log(f"TREE_DIR={[name for name in dir(tree) if not name.startswith('_')]}")
except Exception as exc:
    unreal.log_error(f"TREE_ERROR={exc}")
