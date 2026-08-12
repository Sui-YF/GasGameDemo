import unreal

level = '/Game/CVAD/Maps/L_BattlePrototype'
unreal.EditorLoadingAndSavingUtils.load_map(level)

spawner_class = unreal.load_class(None, '/Game/CVAD/Blueprints/Objectives/BP_MinionSpawner.BP_MinionSpawner_C')
enemy_class = unreal.load_class(None, '/Game/CVAD/Blueprints/Characters/BP_TPPMinion.BP_TPPMinion_C')
boss_class = unreal.load_class(None, '/Game/CVAD/Blueprints/Characters/BP_TPPMinionBoss.BP_TPPMinionBoss_C')

for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_actor_label() == 'BP_MinionSpawner_Demo':
        actor.set_actor_location(unreal.Vector(650, 0, 100), False, False)
        actor.set_editor_property('minion_class', enemy_class)
        actor.set_editor_property('boss_class', boss_class)
        actor.set_editor_property('profile_row_name', 'DemoPlayable')
        actor.set_editor_property('spawn_interval', 1.5)
        actor.set_editor_property('max_alive', 4)
        actor.set_editor_property('kill_quota', 8)
        box = actor.get_component_by_class(unreal.BoxComponent)
        if box:
            box.set_box_extent(unreal.Vector(1100, 900, 400), True)
        unreal.log('CVAD_PLAYABLE_SPAWNER class={} location={}'.format(enemy_class, actor.get_actor_location()))

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
