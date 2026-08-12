import unreal

root = "/Game/CVAD/Data"
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = root + "/DT_Skills"
table = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not table:
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", unreal.CVADSkillRow.static_struct())
    table = tools.create_asset("DT_Skills", root, unreal.DataTable, factory)

csv = (
    "Name,DisplayName,Description,SkillSlot,AbilityClass,Icon,ResourceType,ResourceCost,CooldownSeconds,bUnlockedByDefault\n"
    "SwordLight,剑式·流光,快速近战斩击,LightAttack,/Game/CVAD/Abilities/GA_LightAttack.GA_LightAttack_C,,None,0,0.25,true\n"
    "SwordHeavy,剑式·破阵,消耗体力的重斩,HeavyAttack,/Game/CVAD/Abilities/GA_HeavyAttack.GA_HeavyAttack_C,,Stamina,25,0.8,true\n"
    "DodgeRoll,踏风闪,消耗体力向前闪避,Dodge,/Game/CVAD/Abilities/GA_Dodge.GA_Dodge_C,,Stamina,20,0.6,true\n"
    "FlyingSword,御剑诀,消耗灵力发动远程飞剑,FlyingSword,/Game/CVAD/Abilities/GA_FlyingSword.GA_FlyingSword_C,,Spirit,20,0.7,true\n"
    "FlyingSwordStance,御剑姿态,切换手持剑与飞剑姿态,SwitchStance,/Game/CVAD/Abilities/GA_SwitchStance.GA_SwitchStance_C,,None,0,0.5,true\n"
)
if table and unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, csv, unreal.CVADSkillRow.static_struct()):
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    unreal.log("SKILL_TABLE_CREATED rows={}".format(len(unreal.DataTableFunctionLibrary.get_data_table_row_names(table))))
else:
    unreal.log_error("Failed to create DT_Skills")
