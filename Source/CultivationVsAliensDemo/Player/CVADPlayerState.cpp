#include "Player/CVADPlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Inventory/CVADInventoryComponent.h"
#include "Data/CVADBalanceRows.h"
#include "Engine/DataTable.h"
#include "Data/CVADSkillRows.h"
#include "Abilities/GameplayAbility.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Character/CVADCharacter.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVADSkills, Log, All);

ACVADPlayerState::ACVADPlayerState()
{
    NetUpdateFrequency = 100.f;
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    AttributeSet = CreateDefaultSubobject<UCVADAttributeSet>(TEXT("AttributeSet"));
    InventoryComponent = CreateDefaultSubobject<UCVADInventoryComponent>(TEXT("InventoryComponent"));
    EquippedSkillRows.SetNum(5);
    EquippedAbilityHandles.SetNum(5);
}

UAbilitySystemComponent* ACVADPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACVADPlayerState::BeginPlay()
{
    Super::BeginPlay();
    UDataTable* BalanceTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_PlayerBalance.DT_PlayerBalance"));
    const FCVADPlayerBalanceRow* Row = BalanceTable ? BalanceTable->FindRow<FCVADPlayerBalanceRow>(TEXT("Default"), TEXT("PlayerStateBeginPlay")) : nullptr;
    if (Row && AbilitySystemComponent)
    {
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxHealthAttribute(), Row->MaxHealth);
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), Row->MaxHealth);
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxStaminaAttribute(), Row->MaxStamina);
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetStaminaAttribute(), Row->MaxStamina);
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxSpiritAttribute(), Row->MaxSpirit);
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetSpiritAttribute(), Row->MaxSpirit);
    }
    if (HasAuthority()) InitializeDefaultSkillLoadout();
    if (HasAuthority()) GetWorldTimerManager().SetTimer(ResourceRegenTimer, this, &ThisClass::RegenerateResources, 0.25f, true);
}

void ACVADPlayerState::RegenerateResources()
{
    if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet) return;
    const float Step = 0.25f;
    const ACVADCharacter* Character = Cast<ACVADCharacter>(AbilitySystemComponent->GetAvatarActor());
    if (!Character || !Character->IsSprinting())
        AbilitySystemComponent->ApplyModToAttribute(UCVADAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, 14.f * Step);
    AbilitySystemComponent->ApplyModToAttribute(UCVADAttributeSet::GetSpiritAttribute(), EGameplayModOp::Additive, 7.f * Step);
}

void ACVADPlayerState::AddExperience(int32 Amount)
{
    if (!HasAuthority() || Amount <= 0) return;
    Experience += Amount;
    while (Experience >= GetExperienceToNextLevel())
    {
        Experience -= GetExperienceToNextLevel();
        ++PlayerLevel;
        ++SkillPoints;
        ApplyLevelGrowth();
        RefreshEquippedAbilityLevels();
    }
    ForceNetUpdate();
}

void ACVADPlayerState::RequestSpendSkillPoint(FName SkillRowName)
{
    if (HasAuthority()) ServerSpendSkillPoint_Implementation(SkillRowName);
    else ServerSpendSkillPoint(SkillRowName);
}

void ACVADPlayerState::SetLobbyReady(bool bReady)
{
    if (HasAuthority()) ServerSetLobbyReady_Implementation(bReady); else ServerSetLobbyReady(bReady);
}
void ACVADPlayerState::ServerSetLobbyReady_Implementation(bool bReady)
{
    bLobbyReady=bReady; ForceNetUpdate(); OnSkillLoadoutChanged.Broadcast();
    UE_LOG(LogCVADSkills,Log,TEXT("Lobby ready Player=%s Ready=%s"),*GetPlayerName(),bReady?TEXT("true"):TEXT("false"));
}
void ACVADPlayerState::OnRep_LobbyReady(){OnSkillLoadoutChanged.Broadcast();}

void ACVADPlayerState::ServerSpendSkillPoint_Implementation(FName SkillRowName)
{
    UnlockSkillAuthority(SkillRowName);
}

bool ACVADPlayerState::IsSkillUnlocked(FName SkillRowName) const
{
    return UnlockedSkillRows.Contains(SkillRowName);
}

bool ACVADPlayerState::CanUnlockSkill(FName SkillRowName, FText& FailureReason) const
{
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    const FCVADSkillRow* Row = Table ? Table->FindRow<FCVADSkillRow>(SkillRowName, TEXT("CanUnlockSkill")) : nullptr;
    if (!Row) { FailureReason = NSLOCTEXT("CVAD", "SkillMissing", "技能数据不存在"); return false; }
    if (IsSkillUnlocked(SkillRowName)) { FailureReason = NSLOCTEXT("CVAD", "AlreadyUnlocked", "技能已经解锁"); return false; }
    if (PlayerLevel < Row->RequiredLevel) { FailureReason = FText::Format(NSLOCTEXT("CVAD", "NeedLevel", "需要等级 {0}"), Row->RequiredLevel); return false; }
    if (SkillPoints < Row->SkillPointCost) { FailureReason = FText::Format(NSLOCTEXT("CVAD", "NeedPoints", "需要技能点 {0}"), Row->SkillPointCost); return false; }
    if (!Row->PrerequisiteSkill.IsNone() && !IsSkillUnlocked(Row->PrerequisiteSkill))
    { FailureReason = FText::Format(NSLOCTEXT("CVAD", "NeedPrereq", "需要前置技能 {0}"), FText::FromName(Row->PrerequisiteSkill)); return false; }
    FailureReason = FText::GetEmpty(); return true;
}

bool ACVADPlayerState::UnlockSkillAuthority(FName SkillRowName)
{
    if (!HasAuthority()) return false;
    FText Failure;
    if (!CanUnlockSkill(SkillRowName, Failure)) { UE_LOG(LogCVADSkills, Warning, TEXT("Unlock %s rejected: %s"), *SkillRowName.ToString(), *Failure.ToString()); return false; }
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    const FCVADSkillRow* Row = Table->FindRow<FCVADSkillRow>(SkillRowName, TEXT("UnlockSkill"));
    SkillPoints -= Row->SkillPointCost;
    UnlockedSkillRows.AddUnique(SkillRowName);
    ForceNetUpdate(); OnSkillLoadoutChanged.Broadcast();
    UE_LOG(LogCVADSkills, Log, TEXT("Unlocked skill %s RemainingPoints=%d"), *SkillRowName.ToString(), SkillPoints);
    return true;
}

void ACVADPlayerState::ApplyLevelGrowth()
{
    const float NewMaxHealth = AttributeSet->GetMaxHealth() + 15.f;
    const float NewMaxStamina = AttributeSet->GetMaxStamina() + 10.f;
    const float NewMaxSpirit = AttributeSet->GetMaxSpirit() + 12.f;
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxHealthAttribute(), NewMaxHealth);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxStaminaAttribute(), NewMaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxSpiritAttribute(), NewMaxSpirit);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), NewMaxHealth);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetStaminaAttribute(), NewMaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetSpiritAttribute(), NewMaxSpirit);
    UE_LOG(LogCVADSkills, Log, TEXT("Player leveled up Level=%d HP=%.0f Stamina=%.0f Spirit=%.0f"),
        PlayerLevel, NewMaxHealth, NewMaxStamina, NewMaxSpirit);
}

void ACVADPlayerState::InitializeDefaultSkillLoadout()
{
    if (!HasAuthority()) return;
    UDataTable* SkillTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    if (SkillTable)
    {
        for (const FName RowName : SkillTable->GetRowNames())
            if (const FCVADSkillRow* Row = SkillTable->FindRow<FCVADSkillRow>(RowName, TEXT("DefaultUnlock")); Row && Row->bUnlockedByDefault)
                UnlockedSkillRows.AddUnique(RowName);
    }
    static const FName Defaults[] = {TEXT("SwordNormalCombo"), TEXT("SwordAttack1"), TEXT("DodgeRoll"), TEXT("FlyingSword1"), TEXT("FlyingSwordStance")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Defaults); ++Index)
    {
        if (!EquippedSkillRows.IsValidIndex(Index) || !EquippedSkillRows[Index].IsNone()) continue;
        FName Desired=Defaults[Index];
        if(GConfig)
        {
            FString Saved; const FString Key=FString::Printf(TEXT("EquippedSkill%d"),Index);
            if(GConfig->GetString(TEXT("CVAD.SkillLoadout"),*Key,Saved,GGameUserSettingsIni)&&!Saved.IsEmpty()) Desired=FName(*Saved);
        }
        if(!EquipSkillAuthority(static_cast<ECVADAbilityInput>(Index),Desired))
            EquipSkillAuthority(static_cast<ECVADAbilityInput>(Index),Defaults[Index]);
    }
}

void ACVADPlayerState::EquipSkill(ECVADAbilityInput Slot, FName SkillRowName)
{
    if (HasAuthority()) EquipSkillAuthority(Slot, SkillRowName);
    else ServerEquipSkill(Slot, SkillRowName);
}

void ACVADPlayerState::ServerEquipSkill_Implementation(ECVADAbilityInput Slot, FName SkillRowName)
{
    EquipSkillAuthority(Slot, SkillRowName);
}

void ACVADPlayerState::RequestRestoreProfile(int32 InLevel, int32 InExperience, int32 InSkillPoints,
    const TArray<FName>& InUnlockedSkills, const TArray<FName>& InEquippedSkills,
    const TArray<FName>& InUnlockedItems, const FCVADEquipmentLoadout& InEquipment)
{
    if (HasAuthority()) RestoreProfileAuthority(InLevel, InExperience, InSkillPoints, InUnlockedSkills, InEquippedSkills, InUnlockedItems, InEquipment);
    else ServerRestoreProfile(InLevel, InExperience, InSkillPoints, InUnlockedSkills, InEquippedSkills, InUnlockedItems, InEquipment);
}

void ACVADPlayerState::ServerRestoreProfile_Implementation(int32 InLevel, int32 InExperience, int32 InSkillPoints,
    const TArray<FName>& InUnlockedSkills, const TArray<FName>& InEquippedSkills,
    const TArray<FName>& InUnlockedItems, const FCVADEquipmentLoadout& InEquipment)
{
    RestoreProfileAuthority(InLevel, InExperience, InSkillPoints, InUnlockedSkills, InEquippedSkills, InUnlockedItems, InEquipment);
}

void ACVADPlayerState::RestoreProfileAuthority(int32 InLevel, int32 InExperience, int32 InSkillPoints,
    const TArray<FName>& InUnlockedSkills, const TArray<FName>& InEquippedSkills,
    const TArray<FName>& InUnlockedItems, const FCVADEquipmentLoadout& InEquipment)
{
    if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet) return;
    PlayerLevel = FMath::Clamp(InLevel, 1, 50);
    Experience = FMath::Clamp(InExperience, 0, GetExperienceToNextLevel() - 1);
    SkillPoints = FMath::Clamp(InSkillPoints, 0, 100);
    UDataTable* SkillTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    UnlockedSkillRows.Reset();
    if (SkillTable)
    {
        for (const FName RowName : InUnlockedSkills)
        {
            if (UnlockedSkillRows.Num() >= 64) break;
            if (const FCVADSkillRow* Row = SkillTable->FindRow<FCVADSkillRow>(RowName, TEXT("RestoreProfile"));
                Row && PlayerLevel >= Row->RequiredLevel) UnlockedSkillRows.AddUnique(RowName);
        }
        for (const FName RowName : SkillTable->GetRowNames())
            if (const FCVADSkillRow* Row = SkillTable->FindRow<FCVADSkillRow>(RowName, TEXT("RestoreDefaults")); Row && Row->bUnlockedByDefault)
                UnlockedSkillRows.AddUnique(RowName);
    }
    UDataTable* BalanceTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_PlayerBalance.DT_PlayerBalance"));
    const FCVADPlayerBalanceRow* Balance = BalanceTable ? BalanceTable->FindRow<FCVADPlayerBalanceRow>(TEXT("Default"), TEXT("RestoreProfile")) : nullptr;
    const float MaxHealth = (Balance ? Balance->MaxHealth : 100.f) + (PlayerLevel - 1) * 15.f;
    const float MaxStamina = (Balance ? Balance->MaxStamina : 100.f) + (PlayerLevel - 1) * 10.f;
    const float MaxSpirit = (Balance ? Balance->MaxSpirit : 100.f) + (PlayerLevel - 1) * 12.f;
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxHealthAttribute(), MaxHealth);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxSpiritAttribute(), MaxSpirit);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), MaxHealth);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetSpiritAttribute(), MaxSpirit);
    for (int32 Index = 0; Index < FMath::Min(InEquippedSkills.Num(), EquippedSkillRows.Num()); ++Index)
        EquipSkillAuthority(static_cast<ECVADAbilityInput>(Index), InEquippedSkills[Index]);
    InitializeDefaultSkillLoadout();
    if (InventoryComponent) InventoryComponent->RestoreInventory(InUnlockedItems, InEquipment);
    ForceNetUpdate(); OnSkillLoadoutChanged.Broadcast();
    UE_LOG(LogCVADSkills, Log, TEXT("Restored profile Level=%d XP=%d Skills=%d Items=%d"), PlayerLevel, Experience, UnlockedSkillRows.Num(), InUnlockedItems.Num());
}

bool ACVADPlayerState::EquipSkillAuthority(ECVADAbilityInput Slot, FName SkillRowName)
{
    if (!HasAuthority() || !AbilitySystemComponent) return false;
    const int32 Index = static_cast<int32>(Slot);
    if (!EquippedSkillRows.IsValidIndex(Index)) return false;
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    const FCVADSkillRow* Row = Table ? Table->FindRow<FCVADSkillRow>(SkillRowName, TEXT("EquipSkill")) : nullptr;
    if (!Row || Row->SkillSlot != Slot)
    {
        UE_LOG(LogCVADSkills, Warning, TEXT("Rejected skill row %s for slot %d"), *SkillRowName.ToString(), Index);
        return false;
    }
    if (!Row->bUnlockedByDefault && !IsSkillUnlocked(SkillRowName))
    {
        UE_LOG(LogCVADSkills, Warning, TEXT("Rejected locked skill %s"), *SkillRowName.ToString());
        return false;
    }
    TSubclassOf<UGameplayAbility> AbilityClass = Row->AbilityClass.LoadSynchronous();
    if (!AbilityClass) return false;
    if (EquippedAbilityHandles[Index].IsValid()) AbilitySystemComponent->ClearAbility(EquippedAbilityHandles[Index]);
    FGameplayAbilitySpec Spec(AbilityClass, PlayerLevel, Index, this);
    EquippedAbilityHandles[Index] = AbilitySystemComponent->GiveAbility(Spec);
    EquippedSkillRows[Index] = SkillRowName;
    ForceNetUpdate();
    OnSkillLoadoutChanged.Broadcast();
    UE_LOG(LogCVADSkills, Log, TEXT("Equipped skill %s in slot %d Ability=%s"), *SkillRowName.ToString(), Index, *GetNameSafe(AbilityClass));
    return true;
}

void ACVADPlayerState::RefreshEquippedAbilityLevels()
{
    if (!HasAuthority() || !AbilitySystemComponent) return;
    for (const FGameplayAbilitySpecHandle& Handle : EquippedAbilityHandles)
    {
        if (FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
        {
            Spec->Level = PlayerLevel;
            AbilitySystemComponent->MarkAbilitySpecDirty(*Spec);
        }
    }
    UE_LOG(LogCVADSkills, Log, TEXT("Refreshed equipped GAS ability levels to %d"), PlayerLevel);
}

FName ACVADPlayerState::GetEquippedSkill(ECVADAbilityInput Slot) const
{
    const int32 Index = static_cast<int32>(Slot);
    return EquippedSkillRows.IsValidIndex(Index) ? EquippedSkillRows[Index] : NAME_None;
}

void ACVADPlayerState::OnRep_EquippedSkills() { OnSkillLoadoutChanged.Broadcast(); }

void ACVADPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACVADPlayerState, EquippedSkillRows);
    DOREPLIFETIME(ACVADPlayerState, UnlockedSkillRows);
    DOREPLIFETIME(ACVADPlayerState, PlayerLevel);
    DOREPLIFETIME(ACVADPlayerState, Experience);
    DOREPLIFETIME(ACVADPlayerState, SkillPoints);
    DOREPLIFETIME(ACVADPlayerState, bLobbyReady);
}
