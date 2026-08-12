#include "AbilitySystem/CVADAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "GameplayTagsManager.h"

UCVADAttributeSet::UCVADAttributeSet()
{
    InitMaxHealth(100.f);
    InitHealth(100.f);
    InitMaxStamina(100.f);
    InitStamina(100.f);
    InitMaxSpirit(100.f);
    InitSpirit(100.f);
}

bool UCVADAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
    if (!Super::PreGameplayEffectExecute(Data)) return false;
    if (Data.EvaluatedData.Attribute == GetHealthAttribute() && Data.EvaluatedData.Magnitude < 0.f)
    {
        const UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
        if (TargetASC && TargetASC->HasMatchingGameplayTag(
            UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Invulnerable"))))
        {
            return false;
        }
    }
    return true;
}

void UCVADAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    else if (Attribute == GetStaminaAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
    else if (Attribute == GetSpiritAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxSpirit());
}

void UCVADAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UCVADAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UCVADAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UCVADAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UCVADAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UCVADAttributeSet, Spirit, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UCVADAttributeSet, MaxSpirit, COND_None, REPNOTIFY_Always);
}

void UCVADAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCVADAttributeSet, Health, OldValue); }
void UCVADAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCVADAttributeSet, MaxHealth, OldValue); }
void UCVADAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCVADAttributeSet, Stamina, OldValue); }
void UCVADAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCVADAttributeSet, MaxStamina, OldValue); }
void UCVADAttributeSet::OnRep_Spirit(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCVADAttributeSet, Spirit, OldValue); }
void UCVADAttributeSet::OnRep_MaxSpirit(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCVADAttributeSet, MaxSpirit, OldValue); }
