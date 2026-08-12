import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level.load_level("/Game/CVAD/Maps/L_BattlePrototype")

minions = sorted(
    [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith("BP_TPPMinion_")],
    key=lambda a: a.get_actor_label())
for index, actor in enumerate(minions):
    row, col = divmod(index, 4)
    actor.set_actor_location(unreal.Vector(450.0 + row * 350.0, -300.0 + col * 200.0, 100.0), False, False)

for actor in actors.get_all_level_actors():
    if actor.get_actor_label() == "BP_MinionSpawner_Demo":
        actor.set_actor_location(unreal.Vector(1000.0, 0.0, 100.0), False, False)

level.save_current_level()
unreal.log("DEMO_MINIONS_REPOSITIONED count={} nearest_x=450 spawner_x=1000".format(len(minions)))
