#include "UI/CVADInventoryWidget.h"
#include "Inventory/CVADInventoryComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCVADInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitializeFromOwningPlayer();
    if (Button_Head) Button_Head->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleHead);
    if (Button_Upper) Button_Upper->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleUpper);
    if (Button_Lower) Button_Lower->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleLower);
    if (Button_Feet) Button_Feet->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleFeet);
    if (Button_Hands) Button_Hands->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleHands);
    if (Button_Close) Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseInventory);
    if (UCVADInventoryComponent* Inventory=GetInventory())
    {
        Inventory->OnEquipmentChanged.AddUniqueDynamic(this,&ThisClass::RefreshEquipmentLabels);
        RefreshEquipmentLabels(Inventory->GetEquipmentLoadout());
    }
}

void UCVADInventoryWidget::CycleSlot(ECVADItemType ItemSlot)
{
    UCVADInventoryComponent* Inventory=GetInventory(); if(!Inventory) return;
    FText Failure; if(!Inventory->CanChangeEquipment(Failure)){UE_LOG(LogTemp,Warning,TEXT("Cannot cycle equipment: %s"),*Failure.ToString());return;}
    TArray<FName> Items=Inventory->GetOwnedItemsForSlot(ItemSlot); if(Items.IsEmpty()) return;
    const FName Current=Inventory->GetEquipmentLoadout().GetItem(ItemSlot);
    const int32 CurrentIndex=Items.IndexOfByKey(Current);
    Inventory->RequestEquipItem(ItemSlot,Items[(CurrentIndex+1)%Items.Num()]);
}
void UCVADInventoryWidget::ToggleHead(){CycleSlot(ECVADItemType::Head);} void UCVADInventoryWidget::ToggleUpper(){CycleSlot(ECVADItemType::UpperBody);}
void UCVADInventoryWidget::ToggleLower(){CycleSlot(ECVADItemType::LowerBody);} void UCVADInventoryWidget::ToggleFeet(){CycleSlot(ECVADItemType::Feet);}
void UCVADInventoryWidget::ToggleHands(){CycleSlot(ECVADItemType::Hands);}
void UCVADInventoryWidget::CloseInventory() { CloseScreen(); }

void UCVADInventoryWidget::SetButtonLabel(UButton* Button,const FString& Prefix,FName ItemId)
{
    if(!Button) return; if(UTextBlock* Text=Cast<UTextBlock>(Button->GetContent())) Text->SetText(FText::FromString(Prefix+TEXT("：")+(ItemId.IsNone()?TEXT("未装备"):ItemId.ToString())));
}
void UCVADInventoryWidget::RefreshEquipmentLabels(const FCVADEquipmentLoadout& L)
{
    SetButtonLabel(Button_Head,TEXT("头部"),L.Head); SetButtonLabel(Button_Upper,TEXT("上装"),L.UpperBody);
    SetButtonLabel(Button_Lower,TEXT("下装"),L.LowerBody); SetButtonLabel(Button_Feet,TEXT("脚部"),L.Feet);
    SetButtonLabel(Button_Hands,TEXT("手部"),L.Hands);
}
