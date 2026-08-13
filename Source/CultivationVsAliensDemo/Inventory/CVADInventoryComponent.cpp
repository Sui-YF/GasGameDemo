#include "Inventory/CVADInventoryComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagsManager.h"
#include "Character/CVADCharacter.h"
#include "GameFramework/PlayerState.h"

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

TArray<FName> UCVADInventoryComponent::GetOwnedItemsForSlot(ECVADItemType Slot) const
{
    const TCHAR* Prefix = TEXT("");
    switch (Slot)
    {
    case ECVADItemType::Head: Prefix=TEXT("Head."); break;
    case ECVADItemType::UpperBody: Prefix=TEXT("Upper."); break;
    case ECVADItemType::LowerBody: Prefix=TEXT("Lower."); break;
    case ECVADItemType::Feet: Prefix=TEXT("Feet."); break;
    case ECVADItemType::Hands: Prefix=TEXT("Hands."); break;
    default: return {};
    }
    TArray<FName> Result;
    for (const FName ItemId : OwnedItemIds) if (ItemId.ToString().StartsWith(Prefix)) Result.Add(ItemId);
    Result.Sort(FNameLexicalLess());
    return Result;
}

bool UCVADInventoryComponent::CanChangeEquipment(FText& FailureReason) const
{
    const ACVADCharacter* Character = GetOwner() ? Cast<ACVADCharacter>(Cast<APawn>(GetOwner()->GetInstigator())) : nullptr;
    if (!Character)
    {
        if (const APlayerState* PS = Cast<APlayerState>(GetOwner())) Character = Cast<ACVADCharacter>(PS->GetPawn());
    }
    if (Character && (Character->IsPlayerDown() || Character->IsPlayerHitStunned() || Character->IsSprinting()))
    { FailureReason=NSLOCTEXT("CVAD","EquipMovementBlocked","倒地、受击或冲刺时不能换装"); return false; }
    const IAbilitySystemInterface* AbilityOwner=Cast<IAbilitySystemInterface>(GetOwner());
    const UAbilitySystemComponent* ASC=AbilityOwner?AbilityOwner->GetAbilitySystemComponent():nullptr;
    if (ASC && ASC->HasMatchingGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking"))))
    { FailureReason=NSLOCTEXT("CVAD","EquipCombatBlocked","攻击期间不能换装"); return false; }
    FailureReason=FText::GetEmpty(); return true;
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
    FText FailureReason;
    if (!CanChangeEquipment(FailureReason)) { UE_LOG(LogTemp,Warning,TEXT("Equip rejected: %s"),*FailureReason.ToString()); return; }
    if (Slot == ECVADItemType::Consumable) return;
    if (!ItemId.IsNone() && !OwnedItemIds.Contains(ItemId)) return;

    EquipmentLoadout.SetItem(Slot, ItemId);
    OnEquipmentChanged.Broadcast(EquipmentLoadout);
    GetOwner()->ForceNetUpdate();
    UE_LOG(LogTemp,Log,TEXT("Equipped Item=%s Slot=%d Owner=%s"),*ItemId.ToString(),static_cast<int32>(Slot),*GetNameSafe(GetOwner()));
}

void UCVADInventoryComponent::OnRep_OwnedItems()
{
    OnInventoryChanged.Broadcast();
}

void UCVADInventoryComponent::OnRep_Equipment()
{
    OnEquipmentChanged.Broadcast(EquipmentLoadout);
}
