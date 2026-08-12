#include "AbilitySystem/Abilities/CVADCombatAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Character/CVADCharacter.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "EngineUtils.h"
#include "AbilitySystem/Effects/CVADCooldownEffect.h"
#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVADCombatAbility, Log, All);

UCVADCombatAbility::UCVADCombatAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

FGameplayTag UCVADCombatAbility::GetCooldownTag() const
{
    const TCHAR* Name = TEXT("Cooldown.Attack.Light");
    switch (AbilityInput)
    {
    case ECVADAbilityInput::HeavyAttack: Name = TEXT("Cooldown.Attack.Heavy"); break;
    case ECVADAbilityInput::Dodge: Name = TEXT("Cooldown.Dodge"); break;
    case ECVADAbilityInput::FlyingSword: Name = TEXT("Cooldown.FlyingSword"); break;
    case ECVADAbilityInput::SwitchStance: Name = TEXT("Cooldown.Stance"); break;
    default: break;
    }
    return UGameplayTagsManager::Get().RequestGameplayTag(Name);
}

bool UCVADCombatAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
    return !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid()
        || !ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(GetCooldownTag());
}

void UCVADCombatAbility::ApplyCooldownEffect(UAbilitySystemComponent* AbilitySystem) const
{
    if (!AbilitySystem || CooldownSeconds <= 0.f) return;
    FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UCVADCooldownEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
    if (!Spec.IsValid()) return;
    Spec.Data->SetSetByCallerMagnitude(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Cooldown")), CooldownSeconds);
    Spec.Data->DynamicGrantedTags.AddTag(GetCooldownTag());
    AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UCVADCombatAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !ActorInfo->AvatarActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
    if (!ConsumeResource(AbilitySystem))
    {
        UE_LOG(LogCVADCombatAbility, Warning, TEXT("Ability %s rejected: insufficient resource type=%d cost=%.1f"),
            *GetNameSafe(this), static_cast<int32>(Resource), ResourceCost);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    ApplyCooldownEffect(AbilitySystem);

    if (ActorInfo->IsNetAuthority())
    {
        UAnimSequenceBase* SelectedAnimation = AttackAnimation;
        bool bThisHitMultiple = bHitMultipleTargets;
        if (!ComboAnimations.IsEmpty())
        {
            const int32 SelectedIndex = ComboIndex % ComboAnimations.Num();
            SelectedAnimation = ComboAnimations[SelectedIndex];
            bThisHitMultiple = SelectedIndex == ComboAnimations.Num() - 1;
            ComboIndex = (SelectedIndex + 1) % ComboAnimations.Num();
        }
        if (ACVADCharacter* Character = Cast<ACVADCharacter>(ActorInfo->AvatarActor.Get()))
        {
            if (AbilityInput == ECVADAbilityInput::Dodge) Character->BeginTemporaryInvulnerability(0.35f);
            UAnimSequenceBase* AnimationToPlay = SelectedAnimation;
            if (AbilityInput == ECVADAbilityInput::SwitchStance)
            {
                Character->ToggleFlyingSwordMode();
                if (!Character->IsFlyingSwordMode() && AlternateAnimation) AnimationToPlay = AlternateAnimation;
            }
            if (bAutoTargetNearest)
            {
                AActor* Nearest = nullptr;
                float NearestSq = FMath::Square(FMath::Max(AttackDistance, 1500.f));
                for (TActorIterator<ACVADEnemyCharacter> It(Character->GetWorld()); It; ++It)
                {
                    const float DistanceSq = FVector::DistSquared(Character->GetActorLocation(), It->GetActorLocation());
                    if (DistanceSq < NearestSq) { NearestSq = DistanceSq; Nearest = *It; }
                }
                if (Nearest)
                {
                    const FRotator TargetRotation = (Nearest->GetActorLocation() - Character->GetActorLocation()).Rotation();
                    Character->SetActorRotation(FRotator(0.f, TargetRotation.Yaw, 0.f));
                    UE_LOG(LogCVADCombatAbility, Log, TEXT("Flying sword auto-target=%s"), *GetNameSafe(Nearest));
                }
            }
            if (AbilityInput != ECVADAbilityInput::SwitchStance)
            {
                Character->QueueAttackDamage(Damage, AttackDistance, AttackRadius,
                    AbilityInput == ECVADAbilityInput::FlyingSword ? true : bThisHitMultiple);
            }
            Character->PlayReplicatedActionAnimation(AnimationToPlay);
        }
        UE_LOG(LogCVADCombatAbility, Log, TEXT("Server executing %s Avatar=%s Damage=%.1f Distance=%.1f Radius=%.1f"),
            *GetNameSafe(this), *GetNameSafe(ActorInfo->AvatarActor.Get()), Damage, AttackDistance, AttackRadius);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UCVADCombatAbility::ConsumeResource(UAbilitySystemComponent* AbilitySystem) const
{
    if (!AbilitySystem || Resource == ECVADAbilityResource::None || ResourceCost <= 0.f) return true;

    const FGameplayAttribute Attribute = Resource == ECVADAbilityResource::Stamina
        ? UCVADAttributeSet::GetStaminaAttribute()
        : UCVADAttributeSet::GetSpiritAttribute();
    const float Current = AbilitySystem->GetNumericAttribute(Attribute);
    if (Current < ResourceCost) return false;

    if (AbilitySystem->IsOwnerActorAuthoritative())
    {
        AbilitySystem->ApplyModToAttribute(Attribute, EGameplayModOp::Additive, -ResourceCost);
    }
    return true;
}

void UCVADCombatAbility::ApplyServerDamage(AActor* AvatarActor, UAbilitySystemComponent* SourceAbilitySystem, bool bAllowMultipleTargets) const
{
    if (!AvatarActor || !SourceAbilitySystem || !AvatarActor->HasAuthority()) return;

    const FVector Start = AvatarActor->GetActorLocation();
    const FVector Center = Start + AvatarActor->GetActorForwardVector() * AttackDistance;
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CVADCombatAbility), false, AvatarActor);
    AvatarActor->GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        Center,
        FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
        FCollisionShape::MakeSphere(AttackRadius),
        Params);

    TSet<AActor*> DamagedActors;
    for (const FOverlapResult& Result : Overlaps)
    {
        AActor* TargetActor = Result.GetActor();
        if (!TargetActor || TargetActor == AvatarActor || DamagedActors.Contains(TargetActor)) continue;
        IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(TargetActor);
        UAbilitySystemComponent* TargetAbilitySystem = AbilityInterface ? AbilityInterface->GetAbilitySystemComponent() : nullptr;
        if (!TargetAbilitySystem) continue;

        UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage());
        DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
        FGameplayModifierInfo Modifier;
        Modifier.Attribute = UCVADAttributeSet::GetHealthAttribute();
        Modifier.ModifierOp = EGameplayModOp::Additive;
        Modifier.ModifierMagnitude = FScalableFloat(-Damage);
        DamageEffect->Modifiers.Add(Modifier);
        SourceAbilitySystem->ApplyGameplayEffectToTarget(
            DamageEffect,
            TargetAbilitySystem,
            1.f,
            SourceAbilitySystem->MakeEffectContext(),
            SourceAbilitySystem->GetPredictionKeyForNewAction());

        DamagedActors.Add(TargetActor);
        if (!bAllowMultipleTargets) break;
    }
}
