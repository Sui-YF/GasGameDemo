#pragma once

#include "UI/CVADUserWidget.h"
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

    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Head;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Upper;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Lower;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Feet;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Hands;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UButton> Button_Close;
};
