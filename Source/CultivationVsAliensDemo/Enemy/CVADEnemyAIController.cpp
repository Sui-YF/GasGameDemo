#include "Enemy/CVADEnemyAIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Character/CVADCharacter.h"
#include "EngineUtils.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "Enemy/CVADBTTask_Combat.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "AbilitySystem/Effects/CVADDamageEffect.h"
#include "GameplayTagsManager.h"
#include "Battle/CVADBattleDirector.h"

ACVADEnemyAIController::ACVADEnemyAIController()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ACVADEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    UBehaviorTree* Tree = BehaviorTreeAsset ? BehaviorTreeAsset.Get() : CreateFallbackBehaviorTree();
    if (!Tree || !RunBehaviorTree(Tree))
    {
        UE_LOG(LogTemp, Error, TEXT("AI %s failed to start mandatory Behavior Tree"), *GetName());
    }
}

UBehaviorTree* ACVADEnemyAIController::CreateFallbackBehaviorTree()
{
    UBehaviorTree* Tree = NewObject<UBehaviorTree>(this, TEXT("BT_RuntimeCombatFallback"));
    UBTComposite_Sequence* Root = NewObject<UBTComposite_Sequence>(Tree, TEXT("CombatSequence"));
    UCVADBTTask_Combat* CombatTask = NewObject<UCVADBTTask_Combat>(Root, TEXT("AcquireMoveAttack"));
    Root->Children.AddDefaulted();
    Root->Children[0].ChildTask = CombatTask;
    Tree->RootNode = Root;
    return Tree;
}

void ACVADEnemyAIController::ConfigureCombat(float InDamage, float InAttackInterval)
{
    AttackDamage = FMath::Max(0.f, InDamage);
    AttackInterval = FMath::Max(0.1f, InAttackInterval);
}

bool ACVADEnemyAIController::ExecuteCombatDecision(float DeltaSeconds)
{
    if (!HasAuthority() || !GetPawn()) return false;

    if (const ACVADEnemyCharacter* Enemy = Cast<ACVADEnemyCharacter>(GetPawn()); Enemy && Enemy->IsBoss())
    {
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        {
            if (!It->IsBossStageReady())
            {
                StopMovement();
                CurrentTarget.Reset();
                return false;
            }
            break;
        }
    }

    AttackCooldown = FMath::Max(0.f, AttackCooldown - DeltaSeconds);
    if (!CurrentTarget.IsValid()) CurrentTarget = FindNearestPlayer();
    AActor* Target = CurrentTarget.Get();
    if (!Target) return false;

    const float Distance = FVector::Dist2D(GetPawn()->GetActorLocation(), Target->GetActorLocation());
    if (Distance > AttackRange)
    {
        MoveToActor(Target, AttackRange * 0.75f, true, true, true);
    }
    else
    {
        StopMovement();
        GetPawn()->SetActorRotation((Target->GetActorLocation() - GetPawn()->GetActorLocation()).Rotation());
        if (AttackCooldown <= 0.f)
        {
            AttackTarget(Target);
            AttackCooldown = AttackInterval;
        }
    }
    return true;
}

void ACVADEnemyAIController::ApplyBossPhase(int32 Phase)
{
    ActiveBossPhase = Phase;
    if (Phase == 2) { AttackInterval *= 0.75f; AttackDamage *= 1.25f; }
    else if (Phase >= 3) { AttackInterval *= 0.65f; AttackDamage *= 1.35f; AttackRange *= 1.25f; }
}

AActor* ACVADEnemyAIController::FindNearestPlayer() const
{
    AActor* BestTarget = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
    {
        if (It->IsPlayerDown()) continue;
        const float DistanceSq = FVector::DistSquared(GetPawn()->GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            BestTarget = *It;
        }
    }
    return BestTarget;
}

void ACVADEnemyAIController::AttackTarget(AActor* TargetActor)
{
    IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(TargetActor);
    UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
    if (!TargetASC) return;

    if (TargetASC->HasMatchingGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Invulnerable")))) return;
    UAbilitySystemComponent* SourceASC = Cast<IAbilitySystemInterface>(GetPawn())
        ? Cast<IAbilitySystemInterface>(GetPawn())->GetAbilitySystemComponent() : nullptr;
    if (!SourceASC) return;
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCVADDamageEffect::StaticClass(), 1.f, SourceASC->MakeEffectContext());
    if (!Spec.IsValid()) return;
    Spec.Data->SetSetByCallerMagnitude(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage")), -AttackDamage);
    SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
    if (ActiveBossPhase >= 3) ApplyBossAreaAttack();
    UE_LOG(LogTemp, Log, TEXT("Boss/Enemy %s attacked %s Phase=%d Damage=%.1f"),
        *GetNameSafe(GetPawn()), *GetNameSafe(TargetActor), ActiveBossPhase, AttackDamage);
}

void ACVADEnemyAIController::ApplyBossAreaAttack()
{
    const ACVADEnemyCharacter* Enemy = Cast<ACVADEnemyCharacter>(GetPawn());
    if (!Enemy || !Enemy->IsBoss()) return;
    constexpr float PulseRadius = 450.f;
    for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
    {
        if (FVector::DistSquared2D(It->GetActorLocation(), Enemy->GetActorLocation()) > FMath::Square(PulseRadius)) continue;
        UAbilitySystemComponent* TargetASC = It->GetAbilitySystemComponent();
        if (!TargetASC) continue;
        FGameplayEffectSpecHandle Spec = Enemy->GetAbilitySystemComponent()->MakeOutgoingSpec(
            UCVADDamageEffect::StaticClass(), 1.f, Enemy->GetAbilitySystemComponent()->MakeEffectContext());
        if (Spec.IsValid())
        {
            Spec.Data->SetSetByCallerMagnitude(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage")), -AttackDamage * 0.5f);
            Enemy->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("Boss %s phase-3 area pulse Radius=%.0f Damage=%.1f"),
        *GetNameSafe(Enemy), PulseRadius, AttackDamage * 0.5f);
}
