import unreal

SOURCE='/Game/Castle/Maps/Castle'
FOLDER='/Game/CVAD/Maps'
NAME='L_CastleBattle'
TARGET=FOLDER+'/'+NAME

unreal.EditorLoadingAndSavingUtils.load_map(TARGET)

def spawn(cls_path,loc,label,rot=unreal.Rotator()):
    cls=unreal.load_class(None,cls_path) if isinstance(cls_path,str) else cls_path
    if not cls: raise RuntimeError('Missing class '+str(cls_path))
    actor=unreal.EditorLevelLibrary.spawn_actor_from_class(cls,loc,rot)
    actor.set_actor_label(label)
    return actor

# Remove only CVAD setup actors when rebuilding; imported scenery remains untouched.
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_actor_label().startswith('CVAD_'):
        unreal.EditorLevelLibrary.destroy_actor(actor)

start=spawn(unreal.PlayerStart,unreal.Vector(300,9000,1500),'CVAD_PlayerStart',unreal.Rotator(0,90,0))
nav=spawn(unreal.NavMeshBoundsVolume,unreal.Vector(300,10900,1550),'CVAD_NavMesh')
nav.set_actor_scale3d(unreal.Vector(9,24,5))

director=spawn('/Script/CultivationVsAliensDemo.CVADBattleDirector',unreal.Vector(300,10500,1500),'CVAD_BattleDirector')
spawner=spawn('/Game/CVAD/Blueprints/Objectives/BP_MinionSpawner.BP_MinionSpawner_C',unreal.Vector(300,11000,1500),'CVAD_CastleSpawner')
spawner.set_editor_property('minion_class',unreal.load_class(None,'/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion.BP_SkeletonMinion_C'))
spawner.set_editor_property('boss_class',unreal.load_class(None,'/Game/CVAD/Blueprints/Characters/BP_AngelBoss.BP_AngelBoss_C'))
spawner.set_editor_property('profile_row_name','DemoPlayable')
spawner.set_editor_property('spawn_interval',1.25)
spawner.set_editor_property('max_alive',6)
spawner.set_editor_property('kill_quota',15)
spawner.set_editor_property('boss_spawn_offset',unreal.Vector(0,1450,80))
box=spawner.get_component_by_class(unreal.BoxComponent)
if box: box.set_box_extent(unreal.Vector(850,1900,500),True)

world=unreal.EditorLevelLibrary.get_editor_world()
game_mode=unreal.load_class(None,'/Game/CVAD/Blueprints/Game/BP_CVADGameMode.BP_CVADGameMode_C')
if game_mode: world.get_world_settings().set_editor_property('default_game_mode',game_mode)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
unreal.log('CVAD_CASTLE_BATTLE_READY map={} gamemode={}'.format(TARGET,game_mode))
