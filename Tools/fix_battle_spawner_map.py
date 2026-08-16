import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/CVAD/Maps/L_CastleBattle")
boss_class = unreal.load_asset("/Game/CVAD/Blueprints/Characters/BP_AngelBoss").generated_class()
changed = 0
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if "MinionSpawner" not in actor.get_class().get_name():
        continue
    box = actor.get_editor_property("activation_box")
    box.set_editor_property("box_extent", unreal.Vector(1000.0, 2300.0, 600.0))
    actor.set_editor_property("boss_class", boss_class)
    actor.set_editor_property("require_player_inside", True)
    actor.set_editor_property("start_active", False)
    actor.modify()
    changed += 1
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log_warning("CVAD_FIXED_SPAWNERS %d" % changed)
