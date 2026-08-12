#include "Inventory/CVADInventoryComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UCVADInventoryComponent::UCVADInventoryComponent()
{
    SetIsReplicatedByDefault(true);
    StarterItemIds = {
        TEXT("Head.BambooHat"),
        TEXT("Head.Helmet"),
        TEXT("Upper.Armor"),
        TEXT("Upper.Robe"),
        TEXT("Lower.Default"),
        TEXT("Lower.Alt"),
        TEXT("Feet.Boots"),
        TEXT("Feet.Shoes"),
        TEXT("Hands.Gauntlets")
    };
}

void UCVADInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    for (const FName ItemId : StarterItemIds)
    {
        AddItem(ItemId);
    }

    EquipmentLoadout.Head = TEXT("Head.BambooHat");
    EquipmentLoadout.UpperBody = TEXT("Upper.Armor");
    EquipmentLoadout.LowerBody = TEXT("Lower.Default");
    EquipmentLoadout.Feet = TEXT("Feet.Boots");
    EquipmentLoadout.Hands = TEXT("Hands.Gauntlets");
    OnEquipmentChanged.Broadcast(EquipmentLoadout);
}

void UCVADInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UCVADInventoryComponent, OwnedItemIds, COND_OwnerOnly);
    DOREPLIFETIME(UCVADInventoryComponent, EquipmentLoadout);
}

bool UCVADInventoryComponent::AddItem(FName ItemId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone() || OwnedItemIds.Contains(ItemId)) return false;
    OwnedItemIds.Add(ItemId);
    OnInventoryChanged.Broadcast();
    return true;
}

void UCVADInventoryComponent::RequestEquipItem(ECVADItemType Slot, FName ItemId)
{
    if (!GetOwner()) return;
    if (GetOwner()->HasAuthority()) ServerEquipItem_Implementation(Slot, ItemId);
    else ServerEquipItem(Slot, ItemId);
}

void UCVADInventoryComponent::RestoreInventory(const TArray<FName>& ItemIds, const FCVADEquipmentLoadout& SavedLoadout)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    for (const FName ItemId : ItemIds)
    {
        if (OwnedItemIds.Num() >= 100) break;
        if (!ItemId.IsNone()) OwnedItemIds.AddUnique(ItemId);
    }
    const ECVADItemType Slots[] = {ECVADItemType::Head, ECVADItemType::UpperBody, ECVADItemType::LowerBody,
        ECVADItemType::Feet, ECVADItemType::Hands};
    for (const ECVADItemType Slot : Slots)
    {
        const FName ItemId = SavedLoadout.GetItem(Slot);
        if (ItemId.IsNone() || OwnedItemIds.Contains(ItemId)) EquipmentLoadout.SetItem(Slot, ItemId);
    }
    OnInventoryChanged.Broadcast();
    OnEquipmentChanged.Broadcast(EquipmentLoadout);
    GetOwner()->ForceNetUpdate();
}

void UCVADInventoryComponent::ServerEquipItem_Implementation(ECVADItemType Slot, FName ItemId)
{
    if (Slot == ECVADItemType::Consumable) return;
    if (!ItemId.IsNone() && !OwnedItemIds.Contains(ItemId)) return;

    EquipmentLoadout.SetItem(Slot, ItemId);
    OnEquipmentChanged.Broadcast(EquipmentLoadout);
}

void UCVADInventoryComponent::OnRep_OwnedItems()
{
    OnInventoryChanged.Broadcast();
}

void UCVADInventoryComponent::OnRep_Equipment()
{
    OnEquipmentChanged.Broadcast(EquipmentLoadout);
}
