import unreal

unreal.EditorLoadingAndSavingUtils.load_map('/Game/CVAD/Maps/L_CastleBattle')
cls=unreal.load_class(None,'/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion.BP_SkeletonMinion_C')
if not cls: raise RuntimeError('Missing BP_SkeletonMinion class')
actor=unreal.EditorLevelLibrary.spawn_actor_from_class(cls,unreal.Vector(300,11000,1500))
if not actor: raise RuntimeError('Skeleton spawn failed')
unreal.log('CVAD_SKELETON_SPAWN_TEST_OK actor={}'.format(actor.get_name()))
# Do not save: this actor exists only to exercise construction and BeginPlay-compatible defaults.
