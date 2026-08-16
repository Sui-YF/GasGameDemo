#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CVADWidgetLayoutBuilder.generated.h"

UCLASS()
class CULTIVATIONVSALIENSDEMOEDITOR_API UCVADEditorAssetBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="CVAD Editor")
    static bool BuildAllWidgetLayouts();

    UFUNCTION(BlueprintCallable, Category="CVAD Editor")
    static bool BuildFlyingSwordAnimationBlueprint();
    UFUNCTION(BlueprintCallable, Category="CVAD Editor") static bool BuildMinionAnimationBlueprint();
    UFUNCTION(BlueprintCallable, Category="CVAD Editor") static bool BuildEnemyAIAssets();
    UFUNCTION(BlueprintCallable, Category="CVAD Editor") static bool BuildAllUIControlSkeletons();
    UFUNCTION(BlueprintCallable, Category="CVAD Editor") static bool UpdateUIBackButtons();
    UFUNCTION(BlueprintCallable, Category="CVAD Editor") static bool UpdateSettingsKeyLabels();
};
