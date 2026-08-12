#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CVADAttributeSet.generated.h"

#define CVAD_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class CULTIVATIONVSALIENSDEMO_API UCVADAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UCVADAttributeSet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual bool PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Attributes")
    FGameplayAttributeData Health;
    CVAD_ATTRIBUTE_ACCESSORS(UCVADAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Attributes")
    FGameplayAttributeData MaxHealth;
    CVAD_ATTRIBUTE_ACCESSORS(UCVADAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Stamina, Category="Attributes")
    FGameplayAttributeData Stamina;
    CVAD_ATTRIBUTE_ACCESSORS(UCVADAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxStamina, Category="Attributes")
    FGameplayAttributeData MaxStamina;
    CVAD_ATTRIBUTE_ACCESSORS(UCVADAttributeSet, MaxStamina)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Spirit, Category="Attributes")
    FGameplayAttributeData Spirit;
    CVAD_ATTRIBUTE_ACCESSORS(UCVADAttributeSet, Spirit)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxSpirit, Category="Attributes")
    FGameplayAttributeData MaxSpirit;
    CVAD_ATTRIBUTE_ACCESSORS(UCVADAttributeSet, MaxSpirit)

protected:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_Spirit(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxSpirit(const FGameplayAttributeData& OldValue);
};
