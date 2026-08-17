#pragma once

#include "Abilities/GameplayAbility.h"
#include "CVADCombatAbility.generated.h"

class UAnimSequenceBase;

UENUM(BlueprintType)
enum class ECVADAbilityResource : uint8
{
    None,
    Stamina,
    Spirit
};

UENUM(BlueprintType)
enum class ECVADAbilityInput : uint8
{
    LightAttack,
    HeavyAttack,
    Dodge,
    FlyingSword,
    SwitchStance
};

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API UCVADCombatAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCVADCombatAbility();
    ECVADAbilityInput GetAbilityInput() const { return AbilityInput; }
    float GetPreviewDamage(int32 Level) const { return Damage * (1.f + DamageGrowthPerLevel * FMath::Max(0, Level - 1)); }
    float GetPreviewRadius(int32 Level) const { return AttackRadius * (1.f + RadiusGrowthPerLevel * FMath::Max(0, Level - 1)); }
    float GetPreviewCooldown(int32 Level) const { return CooldownSeconds * FMath::Max(0.5f, 1.f - CooldownReductionPerLevel * FMath::Max(0, Level - 1)); }
    float GetPreviewResourceCost() const { return ResourceCost; }
    UAnimSequenceBase* GetPreviewAnimation() const { return AttackAnimation; }

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
        FGameplayTagContainer* OptionalRelevantTags=nullptr) const override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") ECVADAbilityInput AbilityInput = ECVADAbilityInput::LightAttack;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") float Damage = 20.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") float AttackDistance = 150.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") float AttackRadius = 120.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") ECVADAbilityResource Resource = ECVADAbilityResource::None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") float ResourceCost = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequenceBase> AttackAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequenceBase> AlternateAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequenceBase>> ComboAnimations;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") bool bHitMultipleTargets = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") bool bAutoTargetNearest = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Flying Sword") bool bSpawnHomingSword = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Flying Sword", meta=(ClampMin="1", ClampMax="3")) int32 HomingSwordCount = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cooldown", meta=(ClampMin="0.0")) float CooldownSeconds = 0.25f;

private:
    bool ConsumeResource(UAbilitySystemComponent* AbilitySystem) const;
    FGameplayTag GetCooldownTag() const;
    void ApplyCooldownEffect(UAbilitySystemComponent* AbilitySystem, int32 AbilityLevel) const;
    void ApplyServerDamage(AActor* AvatarActor, UAbilitySystemComponent* SourceAbilitySystem, bool bAllowMultipleTargets) const;
    int32 ComboIndex = 0;
    UPROPERTY(EditDefaultsOnly, Category="Progression", meta=(ClampMin="0.0")) float DamageGrowthPerLevel = 0.08f;
    UPROPERTY(EditDefaultsOnly, Category="Progression", meta=(ClampMin="0.0")) float RadiusGrowthPerLevel = 0.03f;
    UPROPERTY(EditDefaultsOnly, Category="Progression", meta=(ClampMin="0.0", ClampMax="0.1")) float CooldownReductionPerLevel = 0.02f;
};
