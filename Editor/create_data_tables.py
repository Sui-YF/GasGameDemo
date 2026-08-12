import unreal

ROOT = "/Game/CVAD/Data"
if not unreal.EditorAssetLibrary.does_directory_exist(ROOT):
    unreal.EditorAssetLibrary.make_directory(ROOT)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def create_or_update_table(name, row_struct, csv_text):
    path = f"{ROOT}/{name}"
    table = unreal.EditorAssetLibrary.load_asset(path)
    if not table:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", row_struct)
        table = asset_tools.create_asset(name, ROOT, unreal.DataTable, factory)
    if not table:
        unreal.log_error(f"Could not create DataTable {name}")
        return None
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, csv_text, row_struct):
        unreal.log_error(f"Could not import CSV into {name}")
        return None
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    unreal.log(f"Created/updated {path}")
    return table


create_or_update_table(
    "DT_EnemyBalance",
    unreal.CVADEnemyBalanceRow.static_struct(),
    "Name,MaxHealth,AttackDamage,MoveSpeed,AttackInterval\n"
    "Minion,100,8,420,1.5\n"
    "Captain,350,18,360,2.0\n"
    "Boss,1200,16,340,1.8\n",
)

create_or_update_table(
    "DT_SpawnerProfiles",
    unreal.CVADSpawnerProfileRow.static_struct(),
    "Name,SpawnInterval,MaxAlive,KillQuota,bRequirePlayerInside\n"
    "DemoPlayable,1.5,4,8,true\n"
    "Frontline,1.25,12,30,true\n"
    "Defense,0.85,18,60,true\n"
    "Infinite,1.5,10,0,true\n",
)

create_or_update_table(
    "DT_PlayerBalance",
    unreal.CVADPlayerBalanceRow.static_struct(),
    "Name,MaxHealth,MaxStamina,MaxSpirit,MoveSpeed\n"
    "Default,100,100,100,650\n",
)

captain_bp = unreal.EditorAssetLibrary.load_asset("/Game/CVAD/Blueprints/Characters/BP_TPPMinionCaptain")
if captain_bp:
    unreal.get_default_object(captain_bp.generated_class()).set_editor_property("balance_row_name", "Captain")
    unreal.EditorAssetLibrary.save_loaded_asset(captain_bp)

unreal.log("CVAD DataTables created successfully.")
