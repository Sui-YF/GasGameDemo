import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/CVAD/Maps/L_CastleBattle")
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    cls = actor.get_class().get_name()
    if "PlayerStart" in cls:
        unreal.log_warning("CVAD_FLOW PlayerStart %s loc=%s" % (actor.get_name(), actor.get_actor_location()))
    elif "MinionSpawner" in cls:
        box = actor.get_editor_property("activation_box")
        unreal.log_warning(
            "CVAD_FLOW Spawner %s loc=%s extent=%s minion=%s boss=%s start=%s require=%s"
            % (
                actor.get_name(),
                actor.get_actor_location(),
                box.get_editor_property("box_extent"),
                actor.get_editor_property("minion_class"),
                actor.get_editor_property("boss_class"),
                actor.get_editor_property("start_active"),
                actor.get_editor_property("require_player_inside"),
            )
        )
    elif "BattleDirector" in cls:
        unreal.log_warning("CVAD_FLOW Director %s loc=%s" % (actor.get_name(), actor.get_actor_location()))

print("CVAD_FLOW_DONE")
