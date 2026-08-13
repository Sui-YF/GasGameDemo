#pragma once

#include "UI/CVADUserWidget.h"
#include "Inventory/CVADInventoryTypes.h"
#include "CVADInventoryWidget.generated.h"

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API UCVADInventoryWidget : public UCVADUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    UFUNCTION() void ToggleHead();
    UFUNCTION() void ToggleUpper();
    UFUNCTION() void ToggleLower();
    UFUNCTION() void ToggleFeet();
    UFUNCTION() void ToggleHands();
    UFUNCTION() void CloseInventory();
    UFUNCTION() void RefreshEquipmentLabels(const FCVADEquipmentLoadout& NewLoadout);
    void CycleSlot(ECVADItemType ItemSlot);
    void SetButtonLabel(class UButton* Button, const FString& Prefix, FName ItemId);

    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Head;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Upper;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Lower;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Feet;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Hands;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Close;
};
