import unreal

MAP_PATH = "/Game/CVAD/Maps/L_BattlePrototype"
asset_lib = unreal.EditorAssetLibrary

if not asset_lib.does_directory_exist("/Game/CVAD/Maps"):
    asset_lib.make_directory("/Game/CVAD/Maps")

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if asset_lib.does_asset_exist(MAP_PATH):
    level_subsystem.load_level(MAP_PATH)
else:
    level_subsystem.new_level(MAP_PATH)

existing = {actor.get_actor_label(): actor for actor in actor_subsystem.get_all_level_actors()}


def spawn_if_missing(actor_class, label, location, rotation=unreal.Rotator()):
    if label in existing:
        return existing[label]
    actor = actor_subsystem.spawn_actor_from_class(actor_class, location, rotation)
    if actor:
        actor.set_actor_label(label)
        existing[label] = actor
    return actor


floor = spawn_if_missing(unreal.StaticMeshActor, "SM_GrayboxBattlefield", unreal.Vector(0, 0, -100))
if floor:
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    floor.static_mesh_component.set_static_mesh(cube)
    floor.set_actor_scale3d(unreal.Vector(100, 100, 1))

spawn_if_missing(unreal.PlayerStart, "PlayerStart_01", unreal.Vector(-300, -150, 100))
spawn_if_missing(unreal.PlayerStart, "PlayerStart_02", unreal.Vector(-300, 150, 100))

sun = spawn_if_missing(unreal.DirectionalLight, "DirectionalLight", unreal.Vector(0, 0, 1000), unreal.Rotator(-45, -35, 0))
if sun:
    sun.light_component.set_editor_property("intensity", 5.0)
spawn_if_missing(unreal.SkyLight, "SkyLight", unreal.Vector(0, 0, 500))
nav_bounds = spawn_if_missing(unreal.NavMeshBoundsVolume, "NavMeshBounds", unreal.Vector(3500, 0, 200))
if nav_bounds:
    nav_bounds.set_actor_scale3d(unreal.Vector(40, 40, 8))


def load_bp_class(path):
    return unreal.load_class(None, path)


battle_director_class = load_bp_class("/Game/CVAD/Blueprints/Game/BP_BattleDirector.BP_BattleDirector_C")
capture_class = load_bp_class("/Game/CVAD/Blueprints/Objectives/BP_CapturePoint.BP_CapturePoint_C")
defense_class = load_bp_class("/Game/CVAD/Blueprints/Objectives/BP_DefenseCore.BP_DefenseCore_C")
beacon_class = load_bp_class("/Game/CVAD/Blueprints/Objectives/BP_AlienBeacon.BP_AlienBeacon_C")
minion_class = load_bp_class("/Game/CVAD/Blueprints/Characters/BP_TPPMinion.BP_TPPMinion_C")
spawner_class = load_bp_class("/Game/CVAD/Blueprints/Objectives/BP_MinionSpawner.BP_MinionSpawner_C")

if battle_director_class:
    spawn_if_missing(battle_director_class, "BP_BattleDirector", unreal.Vector(0, 0, 50))
if spawner_class:
    spawn_if_missing(spawner_class, "BP_MinionSpawner_Demo", unreal.Vector(1800, 0, 100))
if capture_class:
    spawn_if_missing(capture_class, "BP_CapturePoint", unreal.Vector(2500, 0, 50))
if defense_class:
    spawn_if_missing(defense_class, "BP_DefenseCore", unreal.Vector(4500, 0, 50))
if beacon_class:
    spawn_if_missing(beacon_class, "BP_AlienBeacon_A", unreal.Vector(6500, -1200, 50))
    spawn_if_missing(beacon_class, "BP_AlienBeacon_B", unreal.Vector(6500, 1200, 50))

if minion_class:
    for index in range(12):
        row = index // 4
        col = index % 4
        spawn_if_missing(
            minion_class,
            f"BP_TPPMinion_{index:02d}",
            unreal.Vector(1200 + row * 800, -900 + col * 600, 100),
        )

level_subsystem.save_current_level()
unreal.log("CVAD graybox battle map created successfully.")
