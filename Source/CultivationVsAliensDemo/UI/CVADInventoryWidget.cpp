#include "UI/CVADInventoryWidget.h"
#include "Inventory/CVADInventoryComponent.h"
#include "Components/Button.h"

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
}

void UCVADInventoryWidget::ToggleHead() { if (auto* I = GetInventory()) I->RequestEquipItem(ECVADItemType::Head, I->GetEquipmentLoadout().Head == TEXT("Head.BambooHat") ? TEXT("Head.Helmet") : TEXT("Head.BambooHat")); }
void UCVADInventoryWidget::ToggleUpper() { if (auto* I = GetInventory()) I->RequestEquipItem(ECVADItemType::UpperBody, I->GetEquipmentLoadout().UpperBody == TEXT("Upper.Armor") ? TEXT("Upper.Robe") : TEXT("Upper.Armor")); }
void UCVADInventoryWidget::ToggleLower() { if (auto* I = GetInventory()) I->RequestEquipItem(ECVADItemType::LowerBody, I->GetEquipmentLoadout().LowerBody == TEXT("Lower.Default") ? TEXT("Lower.Alt") : TEXT("Lower.Default")); }
void UCVADInventoryWidget::ToggleFeet() { if (auto* I = GetInventory()) I->RequestEquipItem(ECVADItemType::Feet, I->GetEquipmentLoadout().Feet == TEXT("Feet.Boots") ? TEXT("Feet.Shoes") : TEXT("Feet.Boots")); }
void UCVADInventoryWidget::ToggleHands() { if (auto* I = GetInventory()) I->RequestEquipItem(ECVADItemType::Hands, I->GetEquipmentLoadout().Hands.IsNone() ? FName(TEXT("Hands.Gauntlets")) : FName()); }
void UCVADInventoryWidget::CloseInventory() { CloseScreen(); }
