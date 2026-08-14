import unreal

ROOT = "/Game/CVAD"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


for folder in (
    ROOT,
    f"{ROOT}/Blueprints",
    f"{ROOT}/Blueprints/Characters",
    f"{ROOT}/Blueprints/Game",
    f"{ROOT}/Blueprints/Objectives",
    f"{ROOT}/Abilities",
    f"{ROOT}/Input",
    f"{ROOT}/Input/Actions",
    f"{ROOT}/UI",
    f"{ROOT}/Items",
):
    ensure_dir(folder)


def create_blueprint(name, path, parent_path):
    asset_path = f"{path}/{name}"
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        return existing
    parent = unreal.load_class(None, parent_path)
    if not parent:
        unreal.log_error(f"Missing parent class: {parent_path}")
        return None
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = asset_tools.create_asset(name, path, unreal.Blueprint, factory)
    if bp:
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


def get_cdo(bp):
    return unreal.get_default_object(bp.generated_class()) if bp else None


player_bp = create_blueprint(
    "BP_LanfangCharacter",
    f"{ROOT}/Blueprints/Characters",
    "/Script/CultivationVsAliensDemo.CVADCharacter",
)
if player_bp:
    cdo = get_cdo(player_bp)
    mesh_comp = cdo.get_editor_property("mesh")
    lanfang_mesh = unreal.load_asset("/Game/LanFang/Meshes/Characters/Combines/SK_LanFang_Base")
    lanfang_anim = unreal.load_class(None, "/Game/LanFang/Animations/In-Place/MoveBasic/Female_AnimBP.Female_AnimBP_C")
    if lanfang_mesh:
        mesh_comp.set_skeletal_mesh_asset(lanfang_mesh)
    if lanfang_anim:
        mesh_comp.set_editor_property("anim_class", lanfang_anim)
    mesh_comp.set_relative_location(unreal.Vector(0.0, 0.0, -90.0), False, False)
    mesh_comp.set_relative_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), False, False)
    unreal.EditorAssetLibrary.save_loaded_asset(player_bp)


enemy_bp = create_blueprint(
    "BP_TPPMinion",
    f"{ROOT}/Blueprints/Characters",
    "/Script/CultivationVsAliensDemo.CVADEnemyCharacter",
)
if enemy_bp:
    cdo = get_cdo(enemy_bp)
    mesh_comp = cdo.get_editor_property("mesh")
    manny_mesh = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny")
    manny_anim = unreal.load_class(None, "/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C")
    if manny_mesh:
        mesh_comp.set_skeletal_mesh_asset(manny_mesh)
    if manny_anim:
        mesh_comp.set_editor_property("anim_class", manny_anim)
    mesh_comp.set_relative_location(unreal.Vector(0.0, 0.0, -90.0), False, False)
    mesh_comp.set_relative_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0), False, False)
    unreal.EditorAssetLibrary.save_loaded_asset(enemy_bp)


captain_bp = create_blueprint(
    "BP_TPPMinionCaptain",
    f"{ROOT}/Blueprints/Characters",
    "/Game/CVAD/Blueprints/Characters/BP_TPPMinion.BP_TPPMinion_C",
)
if captain_bp:
    captain_cdo = get_cdo(captain_bp)
    captain_cdo.set_editor_property("balance_row_name", "Captain")
    captain_cdo.set_editor_property("is_boss", True)
    unreal.EditorAssetLibrary.save_loaded_asset(captain_bp)


def create_combat_ability(name, input_type, damage, distance, radius, resource, cost, animation_path=None):
    bp = create_blueprint(name, f"{ROOT}/Abilities", "/Script/CultivationVsAliensDemo.CVADCombatAbility")
    if not bp:
        return None
    cdo = get_cdo(bp)
    cdo.set_editor_property("ability_input", input_type)
    cdo.set_editor_property("damage", damage)
    cdo.set_editor_property("attack_distance", distance)
    cdo.set_editor_property("attack_radius", radius)
    cdo.set_editor_property("resource", resource)
    cdo.set_editor_property("resource_cost", cost)
    if animation_path:
        cdo.set_editor_property("attack_animation", unreal.load_asset(animation_path))
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


ga_light = create_combat_ability(
    "GA_LightAttack", unreal.CVADAbilityInput.LIGHT_ATTACK, 35.0, 130.0, 170.0,
    unreal.CVADAbilityResource.NONE, 0.0,
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack1")
ga_heavy = create_combat_ability(
    "GA_HeavyAttack", unreal.CVADAbilityInput.HEAVY_ATTACK, 70.0, 160.0, 210.0,
    unreal.CVADAbilityResource.STAMINA, 25.0,
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack4")
ga_dodge = create_combat_ability(
    "GA_Dodge", unreal.CVADAbilityInput.DODGE, 0.0, 0.0, 0.0,
    unreal.CVADAbilityResource.STAMINA, 20.0,
    "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Roll_Fwd")
ga_flying = create_combat_ability(
    "GA_FlyingSword", unreal.CVADAbilityInput.FLYING_SWORD, 55.0, 700.0, 180.0,
    unreal.CVADAbilityResource.SPIRIT, 20.0,
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_01")
ga_switch = create_combat_ability(
    "GA_SwitchStance", unreal.CVADAbilityInput.SWITCH_STANCE, 0.0, 0.0, 0.0,
    unreal.CVADAbilityResource.NONE, 0.0,
    "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Equip")

if player_bp:
    ability_classes = [bp.generated_class() for bp in (ga_light, ga_heavy, ga_dodge, ga_flying, ga_switch) if bp]
    get_cdo(player_bp).set_editor_property("default_abilities", ability_classes)
    unreal.EditorAssetLibrary.save_loaded_asset(player_bp)

battle_director_bp = create_blueprint("BP_BattleDirector", f"{ROOT}/Blueprints/Game", "/Script/CultivationVsAliensDemo.CVADBattleDirector")
game_mode_bp = create_blueprint("BP_CVADGameMode", f"{ROOT}/Blueprints/Game", "/Script/CultivationVsAliensDemo.CVADGameMode")
player_controller_bp = create_blueprint("BP_CVADPlayerController", f"{ROOT}/Blueprints/Game", "/Script/CultivationVsAliensDemo.CVADPlayerController")


for name in ("BP_MinionSpawner", "BP_CapturePoint", "BP_DefenseCore", "BP_AlienBeacon"):
    create_blueprint(name, f"{ROOT}/Blueprints/Objectives", "/Script/Engine.Actor")

spawner_bp = unreal.EditorAssetLibrary.load_asset(f"{ROOT}/Blueprints/Objectives/BP_MinionSpawner")
spawner_parent = unreal.load_class(None, "/Script/CultivationVsAliensDemo.CVADMinionSpawner")
if spawner_bp and spawner_parent:
    unreal.BlueprintEditorLibrary.reparent_blueprint(spawner_bp, spawner_parent)
    if enemy_bp:
        spawner_cdo = get_cdo(spawner_bp)
        spawner_cdo.set_editor_property("minion_class", enemy_bp.generated_class())
        spawner_cdo.set_editor_property("max_alive", 12)
        spawner_cdo.set_editor_property("kill_quota", 30)
        spawner_cdo.set_editor_property("spawn_interval", 1.25)
    unreal.EditorAssetLibrary.save_loaded_asset(spawner_bp)


def create_widget_blueprint(name):
    path = f"{ROOT}/UI"
    existing = unreal.EditorAssetLibrary.load_asset(f"{path}/{name}")
    if existing:
        return existing
    parent = unreal.load_class(None, "/Script/CultivationVsAliensDemo.CVADUserWidget")
    try:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        widget_bp = asset_tools.create_asset(name, path, unreal.WidgetBlueprint, factory)
    except Exception as exc:
        unreal.log_warning(f"WidgetBlueprintFactory unavailable for {name}: {exc}")
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        widget_bp = asset_tools.create_asset(name, path, unreal.Blueprint, factory)
    if widget_bp:
        unreal.EditorAssetLibrary.save_loaded_asset(widget_bp)
    return widget_bp


for widget_name in (
    "WBP_MainMenu",
    "WBP_Lobby",
    "WBP_HUD",
    "WBP_Inventory",
    "WBP_Pause",
    "WBP_Settings",
    "WBP_Result",
    "WBP_NameEntry",
):
    create_widget_blueprint(widget_name)

hud_widget_bp = unreal.EditorAssetLibrary.load_asset(f"{ROOT}/UI/WBP_HUD")
inventory_widget_bp = unreal.EditorAssetLibrary.load_asset(f"{ROOT}/UI/WBP_Inventory")
hud_parent = unreal.load_class(None, "/Script/CultivationVsAliensDemo.CVADHUDWidget")
inventory_parent = unreal.load_class(None, "/Script/CultivationVsAliensDemo.CVADInventoryWidget")
if hud_widget_bp and hud_parent:
    unreal.BlueprintEditorLibrary.reparent_blueprint(hud_widget_bp, hud_parent)
    unreal.EditorAssetLibrary.save_loaded_asset(hud_widget_bp)
if inventory_widget_bp and inventory_parent:
    unreal.BlueprintEditorLibrary.reparent_blueprint(inventory_widget_bp, inventory_parent)
    unreal.EditorAssetLibrary.save_loaded_asset(inventory_widget_bp)

if hasattr(unreal, "CVADEditorAssetBuilder"):
    if not unreal.CVADEditorAssetBuilder.build_all_widget_layouts():
        unreal.log_error("Failed to build one or more CVAD Widget Blueprint layouts.")


def create_item(name, item_id, display_name, item_type, mesh_path):
    path = f"{ROOT}/Items"
    existing = unreal.EditorAssetLibrary.load_asset(f"{path}/{name}")
    if existing:
        return existing
    factory = unreal.DataAssetFactory()
    factory.set_editor_property(
        "data_asset_class",
        unreal.load_class(None, "/Script/CultivationVsAliensDemo.CVADItemDefinition"),
    )
    item = asset_tools.create_asset(name, path, unreal.PrimaryDataAsset, factory)
    if item:
        item.set_editor_property("item_id", item_id)
        item.set_editor_property("display_name", display_name)
        item.set_editor_property("item_type", item_type)
        item.set_editor_property("appearance_mesh", unreal.load_asset(mesh_path.split(".")[0]))
        unreal.EditorAssetLibrary.save_loaded_asset(item)
    return item


create_item("DA_Head_BambooHat", "Head.BambooHat", "竹笠", unreal.CVADItemType.HEAD,
            "/Game/LanFang/Meshes/Characters/Separates/Hats/SK_BambooHat_A.SK_BambooHat_A")
create_item("DA_Upper_Armor", "Upper.Armor", "宗门护甲", unreal.CVADItemType.UPPER_BODY,
            "/Game/LanFang/Meshes/Characters/Separates/TopBodies/SK_TopBody_A.SK_TopBody_A")
create_item("DA_Lower_Default", "Lower.Default", "宗门下装", unreal.CVADItemType.LOWER_BODY,
            "/Game/LanFang/Meshes/Characters/Separates/BotBodies/SK_BotBody_A.SK_BotBody_A")
create_item("DA_Feet_Boots", "Feet.Boots", "轻云靴", unreal.CVADItemType.FEET,
            "/Game/LanFang/Meshes/Characters/Separates/Shoes/SK_Boots_A.SK_Boots_A")
create_item("DA_Hands_Gauntlets", "Hands.Gauntlets", "护腕", unreal.CVADItemType.HANDS,
            "/Game/LanFang/Meshes/Characters/Separates/TopBodies/SK_Gauntlets.SK_Gauntlets")
create_item("DA_Head_Helmet", "Head.Helmet", "宗门头盔", unreal.CVADItemType.HEAD,
            "/Game/LanFang/Meshes/Characters/Separates/Hats/SK_Helmet_A.SK_Helmet_A")
create_item("DA_Upper_Robe", "Upper.Robe", "轻云上衣", unreal.CVADItemType.UPPER_BODY,
            "/Game/LanFang/Meshes/Characters/Separates/TopBodies/SK_TopBody_B.SK_TopBody_B")
create_item("DA_Lower_Alt", "Lower.Alt", "轻云下装", unreal.CVADItemType.LOWER_BODY,
            "/Game/LanFang/Meshes/Characters/Separates/BotBodies/SK_BotBody_B.SK_BotBody_B")
create_item("DA_Feet_Shoes", "Feet.Shoes", "布鞋", unreal.CVADItemType.FEET,
            "/Game/LanFang/Meshes/Characters/Separates/Shoes/SK_Shoes_A.SK_Shoes_A")


def create_input_action(name, value_type):
    path = f"{ROOT}/Input/Actions"
    existing = unreal.EditorAssetLibrary.load_asset(f"{path}/{name}")
    if existing:
        return existing
    action = asset_tools.create_asset(name, path, unreal.InputAction, None)
    if action:
        action.set_editor_property("value_type", value_type)
        unreal.EditorAssetLibrary.save_loaded_asset(action)
    return action


axis2d = unreal.InputActionValueType.AXIS2D
boolean = unreal.InputActionValueType.BOOLEAN
actions = {
    "IA_Move": create_input_action("IA_Move", axis2d),
    "IA_Look": create_input_action("IA_Look", axis2d),
    "IA_Jump": create_input_action("IA_Jump", boolean),
    "IA_LightAttack": create_input_action("IA_LightAttack", boolean),
    "IA_HeavyAttack": create_input_action("IA_HeavyAttack", boolean),
    "IA_Dodge": create_input_action("IA_Dodge", boolean),
    "IA_FlyingSword": create_input_action("IA_FlyingSword", boolean),
    "IA_SwitchStance": create_input_action("IA_SwitchStance", boolean),
    "IA_Inventory": create_input_action("IA_Inventory", boolean),
    "IA_Pause": create_input_action("IA_Pause", boolean),
}

mapping_path = f"{ROOT}/Input/IMC_Player"
mapping = unreal.EditorAssetLibrary.load_asset(mapping_path)
if not mapping:
    mapping = asset_tools.create_asset("IMC_Player", f"{ROOT}/Input", unreal.InputMappingContext, None)

if mapping and len(mapping.get_editor_property("mappings")) < 13:
    mapping.unmap_all()
    key_map = {
        "IA_Jump": "SpaceBar",
        "IA_LightAttack": "LeftMouseButton",
        "IA_HeavyAttack": "RightMouseButton",
        "IA_Dodge": "LeftShift",
        "IA_FlyingSword": "Q",
        "IA_SwitchStance": "Tab",
        "IA_Inventory": "B",
        "IA_Pause": "P",
    }
    for action_name, key_name in key_map.items():
        if actions[action_name]:
            key = unreal.Key()
            key.set_editor_property("key_name", key_name)
            mapping.map_key(actions[action_name], key)

    def make_key(key_name):
        result = unreal.Key()
        result.set_editor_property("key_name", key_name)
        return result

    # 2D movement: D uses +X, A uses -X, W/S swizzle X into Y.
    mapping.map_key(actions["IA_Move"], make_key("D"))
    map_a = mapping.map_key(actions["IA_Move"], make_key("A"))
    negate_a = unreal.new_object(unreal.InputModifierNegate, mapping)
    map_a.set_editor_property("modifiers", [negate_a])

    map_w = mapping.map_key(actions["IA_Move"], make_key("W"))
    swizzle_w = unreal.new_object(unreal.InputModifierSwizzleAxis, mapping)
    swizzle_w.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    map_w.set_editor_property("modifiers", [swizzle_w])

    map_s = mapping.map_key(actions["IA_Move"], make_key("S"))
    swizzle_s = unreal.new_object(unreal.InputModifierSwizzleAxis, mapping)
    swizzle_s.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    negate_s = unreal.new_object(unreal.InputModifierNegate, mapping)
    map_s.set_editor_property("modifiers", [swizzle_s, negate_s])

    mapping.map_key(actions["IA_Look"], make_key("Mouse2D"))
    unreal.EditorAssetLibrary.save_loaded_asset(mapping)


if player_controller_bp:
    pc_cdo = get_cdo(player_controller_bp)
    pc_cdo.set_editor_property("player_mapping_context", mapping)
    pc_cdo.set_editor_property("move_action", actions["IA_Move"])
    pc_cdo.set_editor_property("look_action", actions["IA_Look"])
    pc_cdo.set_editor_property("jump_action", actions["IA_Jump"])
    pc_cdo.set_editor_property("light_attack_action", actions["IA_LightAttack"])
    pc_cdo.set_editor_property("heavy_attack_action", actions["IA_HeavyAttack"])
    pc_cdo.set_editor_property("dodge_action", actions["IA_Dodge"])
    pc_cdo.set_editor_property("flying_sword_action", actions["IA_FlyingSword"])
    pc_cdo.set_editor_property("switch_stance_action", actions["IA_SwitchStance"])
    pc_cdo.set_editor_property("inventory_action", actions["IA_Inventory"])
    pc_cdo.set_editor_property("pause_action", actions["IA_Pause"])
    if hud_widget_bp:
        pc_cdo.set_editor_property("hud_widget_class", hud_widget_bp.generated_class())
    if inventory_widget_bp:
        pc_cdo.set_editor_property("inventory_widget_class", inventory_widget_bp.generated_class())
    unreal.EditorAssetLibrary.save_loaded_asset(player_controller_bp)

if game_mode_bp and player_bp and player_controller_bp:
    gm_cdo = get_cdo(game_mode_bp)
    gm_cdo.set_editor_property("default_pawn_class", player_bp.generated_class())
    gm_cdo.set_editor_property("player_controller_class", player_controller_bp.generated_class())
    unreal.EditorAssetLibrary.save_loaded_asset(game_mode_bp)


unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
unreal.log("CVAD project assets created successfully.")
