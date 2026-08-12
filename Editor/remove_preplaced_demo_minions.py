import unreal

unreal.EditorLoadingAndSavingUtils.load_map('/Game/CVAD/Maps/L_BattlePrototype')
removed = 0
for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
    if actor.get_class().get_name() in ('BP_TPPMinion_C', 'BP_TPPMinionCaptain_C', 'BP_TPPMinionBoss_C'):
        unreal.EditorLevelLibrary.destroy_actor(actor)
        removed += 1
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log('CVAD_PREPLACED_MINIONS_REMOVED={}'.format(removed))
