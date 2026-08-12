#include "AbilitySystem/Effects/CVADDamageEffect.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "GameplayTagsManager.h"

UCVADDamageEffect::UCVADDamageEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UCVADAttributeSet::GetHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat Magnitude;
    Magnitude.DataTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage"));
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
    Modifiers.Add(Modifier);
}
