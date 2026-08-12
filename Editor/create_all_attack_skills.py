import unreal

ROOT = "/Game/CVAD"
ABILITY_PATH = ROOT + "/Abilities/Attacks"
DATA_PATH = ROOT + "/Data"
tools = unreal.AssetToolsHelpers.get_asset_tools()
if not unreal.EditorAssetLibrary.does_directory_exist(ABILITY_PATH):
    unreal.EditorAssetLibrary.make_directory(ABILITY_PATH)


def ability(name, slot, animation_path, damage, distance, radius, resource, cost, cooldown=0.25):
    path = ABILITY_PATH + "/" + name
    bp = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/CultivationVsAliensDemo.CVADCombatAbility"))
        bp = tools.create_asset(name, ABILITY_PATH, unreal.Blueprint, factory)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("ability_input", slot)
    cdo.set_editor_property("attack_animation", unreal.EditorAssetLibrary.load_asset(animation_path))
    cdo.set_editor_property("damage", damage)
    cdo.set_editor_property("attack_distance", distance)
    cdo.set_editor_property("attack_radius", radius)
    cdo.set_editor_property("resource", resource)
    cdo.set_editor_property("resource_cost", cost)
    cdo.set_editor_property("cooldown_seconds", cooldown)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return bp


entries = [
    ("SwordAttack1", "剑技·流光", "可装配主动剑技", unreal.CVADAbilityInput.HEAVY_ATTACK, "HeavyAttack", "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack1", 65, 150, 190, unreal.CVADAbilityResource.STAMINA, "Stamina", 20, 0.65),
    ("SwordAttack2", "剑式·追风", "连续追击斩", unreal.CVADAbilityInput.LIGHT_ATTACK, "LightAttack", "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack2", 42, 140, 175, unreal.CVADAbilityResource.NONE, "None", 0, 0.30),
    ("SwordAttack3", "剑式·回锋", "回身范围斩", unreal.CVADAbilityInput.LIGHT_ATTACK, "LightAttack", "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack3", 48, 145, 200, unreal.CVADAbilityResource.STAMINA, "Stamina", 10, 0.40),
    ("SwordAttack4", "剑式·破阵", "蓄力重斩", unreal.CVADAbilityInput.HEAVY_ATTACK, "HeavyAttack", "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack4", 70, 160, 210, unreal.CVADAbilityResource.STAMINA, "Stamina", 25, 0.80),
    ("SwordAttack5", "剑式·断岳", "高伤终结斩", unreal.CVADAbilityInput.HEAVY_ATTACK, "HeavyAttack", "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack5", 90, 175, 230, unreal.CVADAbilityResource.STAMINA, "Stamina", 35, 1.10),
    ("FlyingSword1", "御剑·穿云", "单次飞剑突袭", unreal.CVADAbilityInput.FLYING_SWORD, "FlyingSword", "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_01", 55, 700, 180, unreal.CVADAbilityResource.SPIRIT, "Spirit", 20, 0.70),
    ("FlyingSword2", "御剑·双星", "快速飞剑连击", unreal.CVADAbilityInput.FLYING_SWORD, "FlyingSword", "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_02", 70, 750, 200, unreal.CVADAbilityResource.SPIRIT, "Spirit", 30, 0.95),
    ("FlyingSword3", "御剑·天河", "大范围御剑攻击", unreal.CVADAbilityInput.FLYING_SWORD, "FlyingSword", "/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Attack_03", 100, 800, 260, unreal.CVADAbilityResource.SPIRIT, "Spirit", 45, 1.30),
]

csv_rows = ["Name,DisplayName,Description,SkillSlot,AbilityClass,Icon,ActionAnimation,ResourceType,ResourceCost,CooldownSeconds,bIsAreaOfEffect,AreaRadius,bUnlockedByDefault,RequiredLevel,SkillPointCost,PrerequisiteSkill"]
for row_name, display, desc, slot_enum, slot_csv, anim_path, damage, distance, radius, resource_enum, resource_csv, cost, cooldown in entries:
    bp = ability("GA_" + row_name, slot_enum, anim_path, damage, distance, radius, resource_enum, cost, cooldown)
    is_aoe = row_name in ("SwordAttack3", "SwordAttack4", "SwordAttack5", "FlyingSword2", "FlyingSword3")
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("hit_multiple_targets", is_aoe)
    if row_name == "FlyingSword2":
        cdo.set_editor_property("auto_target_nearest", True)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    if row_name in ("SwordAttack2", "SwordAttack3", "SwordAttack4"):
        continue
    anim_object_path = "AnimSequence'{}.{}'".format(anim_path, anim_path.rsplit("/", 1)[-1])
    default_unlock = row_name in ("SwordAttack1", "FlyingSword1")
    required_level = {"SwordAttack5": 4, "FlyingSword2": 2, "FlyingSword3": 5}.get(row_name, 1)
    prerequisite = {"SwordAttack5": "SwordAttack1", "FlyingSword2": "FlyingSword1", "FlyingSword3": "FlyingSword2"}.get(row_name, "")
    csv_rows.append("{},{},{},{},{},,{},{},{},{},{},{},{},{},1,{}".format(
        row_name, display, desc, slot_csv, bp.generated_class().get_path_name(), anim_object_path, resource_csv, cost, cooldown,
        str(is_aoe).lower(), radius if is_aoe else 0, str(default_unlock).lower(), required_level, prerequisite))

combo = ability("GA_SwordNormalCombo", unreal.CVADAbilityInput.LIGHT_ATTACK,
                "/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack2", 40, 145, 195,
                unreal.CVADAbilityResource.NONE, 0)
combo_cdo = unreal.get_default_object(combo.generated_class())
combo_cdo.set_editor_property("combo_animations", [
    unreal.EditorAssetLibrary.load_asset("/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack2"),
    unreal.EditorAssetLibrary.load_asset("/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack3"),
    unreal.EditorAssetLibrary.load_asset("/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack4"),
])
unreal.EditorAssetLibrary.save_loaded_asset(combo)
csv_rows.append("SwordNormalCombo,基础剑式三连,Attack2-3-4普通攻击连段,LightAttack,{},,AnimSequence'/Game/LanFang/Animations/RootMotion/Attacks/Anim_Attack2.Anim_Attack2',None,0,0.25,false,0,true".format(combo.generated_class().get_path_name()))

# Keep utility skills in the same table.
csv_rows += [
    "DodgeRoll,踏风闪,向前闪避,Dodge,/Game/CVAD/Abilities/GA_Dodge.GA_Dodge_C,,AnimSequence'/Game/LanFang/Animations/RootMotion/Attacks/Anim_Roll_Fwd.Anim_Roll_Fwd',Stamina,20,0.6,false,0,true",
    "FlyingSwordStance,御剑姿态,切换剑与飞剑姿态,SwitchStance,/Game/CVAD/Abilities/GA_SwitchStance.GA_SwitchStance_C,,AnimSequence'/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Equip.Anim_FS_Equip',None,0,0.5,false,0,true",
]

# Utility rows above use the legacy column count; append progression defaults consistently.
for index in range(1, len(csv_rows)):
    columns = csv_rows[index].split(',')
    if len(columns) < 16:
        csv_rows[index] += ",1,0,"

table = unreal.EditorAssetLibrary.load_asset(DATA_PATH + "/DT_Skills")
if table and unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, "\n".join(csv_rows) + "\n", unreal.CVADSkillRow.static_struct()):
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    unreal.log("ALL_ATTACK_SKILLS_CREATED rows={}".format(len(unreal.DataTableFunctionLibrary.get_data_table_row_names(table))))
else:
    unreal.log_error("Failed to update DT_Skills with all attacks")
