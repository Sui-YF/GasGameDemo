#pragma once

#include "Components/ActorComponent.h"
#include "Inventory/CVADInventoryTypes.h"
#include "CVADInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCVADInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCVADEquipmentChanged, const FCVADEquipmentLoadout&, NewLoadout);

UCLASS(ClassGroup=(CVAD), meta=(BlueprintSpawnableComponent))
class CULTIVATIONVSALIENSDEMO_API UCVADInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCVADInventoryComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Inventory")
    bool OwnsItem(FName ItemId) const { return OwnedItemIds.Contains(ItemId); }

    UFUNCTION(BlueprintPure, Category="Inventory")
    const TArray<FName>& GetOwnedItemIds() const { return OwnedItemIds; }

    UFUNCTION(BlueprintPure, Category="Inventory")
    const FCVADEquipmentLoadout& GetEquipmentLoadout() const { return EquipmentLoadout; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory")
    bool AddItem(FName ItemId);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    void RequestEquipItem(ECVADItemType Slot, FName ItemId);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory")
    void RestoreInventory(const TArray<FName>& ItemIds, const FCVADEquipmentLoadout& SavedLoadout);

    UPROPERTY(BlueprintAssignable, Category="Inventory") FCVADInventoryChanged OnInventoryChanged;
    UPROPERTY(BlueprintAssignable, Category="Inventory") FCVADEquipmentChanged OnEquipmentChanged;

protected:
    UPROPERTY(EditDefaultsOnly, Category="Inventory")
    TArray<FName> StarterItemIds;

    UPROPERTY(ReplicatedUsing=OnRep_OwnedItems) TArray<FName> OwnedItemIds;
    UPROPERTY(ReplicatedUsing=OnRep_Equipment) FCVADEquipmentLoadout EquipmentLoadout;

    UFUNCTION(Server, Reliable)
    void ServerEquipItem(ECVADItemType Slot, FName ItemId);

    UFUNCTION() void OnRep_OwnedItems();
    UFUNCTION() void OnRep_Equipment();
};
