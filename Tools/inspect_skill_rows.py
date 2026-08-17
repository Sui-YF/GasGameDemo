import unreal

table = unreal.load_asset("/Game/CVAD/Data/DT_Skills")
if not table:
    unreal.log_warning("MISSING DT_Skills")
else:
    names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    unreal.log_warning("DT_Skills rows=%s" % names)
