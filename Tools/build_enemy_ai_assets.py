import unreal


if unreal.CVADEditorAssetBuilder.build_enemy_ai_assets():
    unreal.log("Created CVAD enemy Behavior Tree and Blackboard assets.")
else:
    unreal.log_error("Failed to create CVAD enemy AI assets.")
