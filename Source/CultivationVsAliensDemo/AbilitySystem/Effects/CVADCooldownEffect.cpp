#include "AbilitySystem/Effects/CVADCooldownEffect.h"
#include "GameplayTagsManager.h"

UCVADCooldownEffect::UCVADCooldownEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    FSetByCallerFloat CallerDuration;
    CallerDuration.DataTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Cooldown"));
    DurationMagnitude = FGameplayEffectModifierMagnitude(CallerDuration);
}
