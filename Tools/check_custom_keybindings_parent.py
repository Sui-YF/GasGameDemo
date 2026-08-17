import unreal

bp = unreal.load_asset("/Game/CVAD/UI/WBP_CustomKeybindings")
if not bp:
    unreal.log_warning("MISSING WBP_CustomKeybindings")
else:
    cls = bp.generated_class()
    unreal.log_warning("WBP_CustomKeybindings class=%s" % cls.get_name())
