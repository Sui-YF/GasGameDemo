import unreal

bp = unreal.load_asset("/Game/CVAD/Blueprints/Objectives/BP_MinionSpawner")
if bp:
    cdo = unreal.get_default_object(bp.generated_class())
    unreal.log_warning("CVAD_SPAWNER_DEFAULT minion=%s boss=%s start=%s require_inside=%s" % (
        cdo.get_editor_property("minion_class"), cdo.get_editor_property("boss_class"),
        cdo.get_editor_property("start_active"), cdo.get_editor_property("require_player_inside")))

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/CVAD/Maps/L_CastleBattle")
actors = unreal.EditorLevelLibrary.get_all_level_actors()
count = 0
for actor in actors:
    if "PlayerStart" in actor.get_class().get_name():
        unreal.log_warning("CVAD_PLAYER_START %s location=%s" % (actor.get_name(), actor.get_actor_location()))
    if "MinionSpawner" not in actor.get_class().get_name():
        continue
    count += 1
    box = actor.get_editor_property("activation_box")
    unreal.log_warning("CVAD_SPAWNER_ACTOR %s location=%s extent=%s minion=%s start=%s require_inside=%s" % (
        actor.get_name(), actor.get_actor_location(), box.get_editor_property("box_extent"),
        actor.get_editor_property("minion_class"), actor.get_editor_property("start_active"),
        actor.get_editor_property("require_player_inside")))
unreal.log_warning("CVAD_SPAWNER_COUNT %d" % count)
