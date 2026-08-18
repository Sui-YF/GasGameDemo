#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "Inventory/CVADInventoryTypes.h"
#include "CVADPlayerState.generated.h"

class UAbilitySystemComponent;
class UCVADAttributeSet;
class UCVADInventoryComponent;
enum class ECVADAbilityInput : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCVADSkillLoadoutChanged);

UCLASS()
class CULTIVATIONVSALIENSDEMO_API ACVADPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACVADPlayerState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void BeginPlay() override;
    UCVADAttributeSet* GetAttributeSet() const { return AttributeSet; }
    UFUNCTION(BlueprintPure, Category="Inventory") UCVADInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

    UFUNCTION(BlueprintCallable, Category="Skills") void EquipSkill(ECVADAbilityInput Slot, FName SkillRowName);
    UFUNCTION(BlueprintPure, Category="Skills") FName GetEquippedSkill(ECVADAbilityInput Slot) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Skills") void InitializeDefaultSkillLoadout();
    UPROPERTY(BlueprintAssignable, Category="Skills") FCVADSkillLoadoutChanged OnSkillLoadoutChanged;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_EquippedSkills, Category="Skills") TArray<FName> EquippedSkillRows;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_EquippedSkills, Category="Skills") TArray<FName> UnlockedSkillRows;
    UFUNCTION(BlueprintPure, Category="Skills") bool IsSkillUnlocked(FName SkillRowName) const;
    UFUNCTION(BlueprintPure, Category="Skills") bool CanUnlockSkill(FName SkillRowName, FText& FailureReason) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Progression") void AddExperience(int32 Amount);
    UFUNCTION(BlueprintCallable, Category="Progression") void RequestSpendSkillPoint(FName SkillRowName);
    UFUNCTION(BlueprintPure, Category="Progression") int32 GetExperienceToNextLevel() const { return PlayerLevel * 100; }
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Progression") int32 PlayerLevel = 1;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Progression") int32 Experience = 0;
    /** Demo budget: enough points to test every purchasable skill in one session. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category="Progression") int32 SkillPoints = 99;
    /** Demo mode grants a complete profile so every skill can be equipped directly. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression|Demo") bool bDemoFullProfile = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression|Demo", meta=(ClampMin="1", ClampMax="50"))
    int32 DemoPlayerLevel = 50;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression|Demo", meta=(ClampMin="0"))
    int32 DemoSkillPoints = 999;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LobbyReady, Category="Lobby") bool bLobbyReady = false;
    UFUNCTION(BlueprintCallable, Category="Lobby") void SetLobbyReady(bool bReady);
    void RequestRestoreProfile(int32 InLevel, int32 InExperience, int32 InSkillPoints,
        const TArray<FName>& InUnlockedSkills, const TArray<FName>& InEquippedSkills,
        const TArray<FName>& InUnlockedItems, const FCVADEquipmentLoadout& InEquipment);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UCVADAttributeSet> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
    TObjectPtr<UCVADInventoryComponent> InventoryComponent;

private:
    UFUNCTION(Server, Reliable) void ServerSpendSkillPoint(FName SkillRowName);
    UFUNCTION(Server, Reliable) void ServerSetLobbyReady(bool bReady);
    UFUNCTION() void OnRep_LobbyReady();
    bool UnlockSkillAuthority(FName SkillRowName);
    UFUNCTION(Server, Reliable) void ServerEquipSkill(ECVADAbilityInput Slot, FName SkillRowName);
    UFUNCTION(Server, Reliable) void ServerRestoreProfile(int32 InLevel, int32 InExperience, int32 InSkillPoints,
        const TArray<FName>& InUnlockedSkills, const TArray<FName>& InEquippedSkills,
        const TArray<FName>& InUnlockedItems, const FCVADEquipmentLoadout& InEquipment);
    void RestoreProfileAuthority(int32 InLevel, int32 InExperience, int32 InSkillPoints,
        const TArray<FName>& InUnlockedSkills, const TArray<FName>& InEquippedSkills,
        const TArray<FName>& InUnlockedItems, const FCVADEquipmentLoadout& InEquipment);
    void ApplyDemoFullProfile();
    UFUNCTION() void OnRep_EquippedSkills();
    bool EquipSkillAuthority(ECVADAbilityInput Slot, FName SkillRowName);
    TArray<FGameplayAbilitySpecHandle> EquippedAbilityHandles;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    FTimerHandle ResourceRegenTimer;
    void RegenerateResources();
    void ApplyLevelGrowth();
    void RefreshEquippedAbilityLevels();
};
