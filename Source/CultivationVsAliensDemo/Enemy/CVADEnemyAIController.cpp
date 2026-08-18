#include "Enemy/CVADEnemyAIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Character/CVADCharacter.h"
#include "EngineUtils.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "Enemy/CVADBTTask_Combat.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "AbilitySystem/Effects/CVADDamageEffect.h"
#include "GameplayTagsManager.h"
#include "Battle/CVADBattleDirector.h"
#include "TimerManager.h"

ACVADEnemyAIController::ACVADEnemyAIController()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ACVADEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (InPawn)
    {
        HomeLocation = InPawn->GetActorLocation();
        bHomeLocationSet = true;
        FlankDirection = FMath::FRand() < 0.5f ? -1 : 1;
    }

    UBehaviorTree* Tree = BehaviorTreeAsset.Get();
    if (!Tree)
    {
        Tree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/CVAD/AI/BT_EnemyCombat.BT_EnemyCombat"));
    }
    if (!Tree)
    {
        Tree = CreateFallbackBehaviorTree();
    }

    if (!Tree || !RunBehaviorTree(Tree))
    {
        UE_LOG(LogTemp, Error, TEXT("AI %s could not start its behavior tree"), *GetName());
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
    BaseAttackDamage=FMath::Max(0.f,InDamage); BaseAttackInterval=FMath::Max(0.1f,InAttackInterval); BaseAttackRange=AttackRange;
    ConfigureBossRole(BossRole);
}

void ACVADEnemyAIController::ConfigureBossRole(int32 NewBossRole)
{
    BossRole=FMath::Clamp(NewBossRole,0,2);
    AttackDamage=BaseAttackDamage; AttackInterval=BaseAttackInterval; AttackRange=BaseAttackRange;
    if(BossRole==0) { AttackDamage*=1.05f; AttackRange=185.f; AttackInterval*=1.0f; }
    else if(BossRole==1) { AttackDamage*=0.8f; AttackRange=270.f; AttackInterval*=0.85f; }
    else { AttackDamage*=0.7f; AttackRange=620.f; AttackInterval*=1.35f; BossTelegraphRadius=520.f; }
    AttackCooldown=0.35f+BossRole*0.65f;
    UE_LOG(LogTemp,Log,TEXT("Boss AI role configured Role=%d Damage=%.1f Range=%.0f Interval=%.2f"),BossRole,AttackDamage,AttackRange,AttackInterval);
}

void ACVADEnemyAIController::CancelPendingAttack()
{
    if (!bAttackWindingUp) return;
    bAttackWindingUp = false;
    GetWorldTimerManager().ClearTimer(AttackWindupTimer);
    PendingAttackTarget.Reset();
    if (ACVADEnemyCharacter* Enemy = Cast<ACVADEnemyCharacter>(GetPawn()))
    {
        Enemy->EndAttackTelegraph();
        if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
            ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
    }
    UE_LOG(LogTemp, Log, TEXT("Enemy %s pending attack cancelled"), *GetNameSafe(GetPawn()));
}

bool ACVADEnemyAIController::ExecuteCombatDecision(float DeltaSeconds)
{
    if (!HasAuthority() || !GetPawn()) return false;

    const ACVADEnemyCharacter* ControlledEnemy = Cast<ACVADEnemyCharacter>(GetPawn());
    const FVector PawnLocation = GetPawn()->GetActorLocation();
    if (!bHomeLocationSet)
    {
        HomeLocation = PawnLocation;
        bHomeLocationSet = true;
    }
    if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
    {
        BlackboardComponent->SetValueAsBool(TEXT("IsBoss"), ControlledEnemy && ControlledEnemy->IsBoss());
        BlackboardComponent->SetValueAsInt(TEXT("BossPhase"), ActiveBossPhase);
    }

    if (ControlledEnemy && ControlledEnemy->IsHitStunned())
    {
        StopMovement();
        return false;
    }
    if (bAttackWindingUp)
    {
        StopMovement();
        return true;
    }

    if (ControlledEnemy && ControlledEnemy->IsBoss())
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
    if (const ACVADCharacter* ExistingTarget=Cast<ACVADCharacter>(CurrentTarget.Get());
        ExistingTarget && ExistingTarget->IsPlayerDown())
    {
        CurrentTarget.Reset();
        StopMovement();
    }
    if (!CurrentTarget.IsValid()) CurrentTarget = FindNearestPlayer();
    AActor* Target = CurrentTarget.Get();

    // Leash: an enemy that wanders too far from its post drops the target and
    // walks back home, so waves do not chase players across the whole map.
    if (Target)
    {
        const float TargetHomeDistSq = FVector::DistSquared(Target->GetActorLocation(), HomeLocation);
        if (TargetHomeDistSq > FMath::Square(AggroLeashDistance))
        {
            CurrentTarget.Reset();
            Target = nullptr;
            if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
                BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), nullptr);
        }
    }
    if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
    {
        BlackboardComponent->SetValueAsObject(TEXT("TargetActor"), Target);
    }
    if (!Target)
    {
        // Return to the home position and wait; the behavior tree keeps ticking
        // so a player entering aggro range is picked up again immediately.
        const float HomeDistanceSq = FVector::DistSquared(PawnLocation, HomeLocation);
        if (HomeDistanceSq > FMath::Square(120.f))
        {
            MoveToLocation(HomeLocation, 60.f, true, true, true);
            bReturningHome = true;
        }
        else
        {
            StopMovement();
            bReturningHome = false;
        }
        return true;
    }
    bReturningHome = false;

    const float Distance = FVector::Dist2D(PawnLocation, Target->GetActorLocation());
    if (Distance > AttackRange)
    {
        if (ControlledEnemy && !ControlledEnemy->IsBoss() && Distance <= FlankStartDistance)
        {
            // Circle to the side while closing in so minions do not all stack
            // in a straight line toward the player.
            const FVector ToTarget = (Target->GetActorLocation() - PawnLocation).GetSafeNormal2D();
            const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal2D();
            const FVector FlankGoal = Target->GetActorLocation() + Right * FlankDirection * FlankOffsetDistance;
            MoveToLocation(FlankGoal, AttackRange * 0.7f, true, true, true);
            if (FMath::FRand() < 0.04f) FlankDirection = -FlankDirection;
        }
        else
        {
            MoveToActor(Target, AttackRange * 0.75f, true, true, true);
        }
    }
    else
    {
        if (ControlledEnemy && !ControlledEnemy->IsBoss() && AttackCooldown > 0.35f)
        {
            // Strafe sideways while the attack cooldown recharges, then commit
            // when the next attack is ready.
            const FVector ToTarget = (Target->GetActorLocation() - PawnLocation).GetSafeNormal2D();
            const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal2D();
            const FVector StrafeGoal = PawnLocation + Right * FlankDirection * FlankOffsetDistance * 0.6f;
            MoveToLocation(StrafeGoal, 60.f, true, true, true);
            if (FMath::FRand() < 0.05f) FlankDirection = -FlankDirection;
        }
        else
        {
            StopMovement();
            GetPawn()->SetActorRotation((Target->GetActorLocation() - PawnLocation).Rotation());
            if (AttackCooldown <= 0.f)
            {
                AttackTarget(Target);
                AttackCooldown = AttackInterval;
            }
        }
    }
    return true;
}

void ACVADEnemyAIController::ApplyBossPhase(int32 Phase)
{
    ActiveBossPhase = Phase;
    if (Phase == 2) { AttackInterval *= 0.85f; AttackDamage *= 1.15f; }
    else if (Phase >= 3) { AttackInterval *= 0.75f; AttackDamage *= 1.2f; AttackRange *= 1.15f; }
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
    ACVADEnemyCharacter* Enemy = Cast<ACVADEnemyCharacter>(GetPawn());
    if (!Enemy || !TargetActor || bAttackWindingUp) return;
    const bool bBossArea = Enemy->IsBoss() && (ActiveBossPhase >= 3 || BossRole==2);
    const float Windup = Enemy->IsBoss() ? BossAttackWindup : MinionAttackWindup;
    PendingAttackDirection=(TargetActor->GetActorLocation()-Enemy->GetActorLocation()).GetSafeNormal2D();
    PendingAttackShape=Enemy->IsBoss()?BossRole:0;
    if(Enemy->IsBoss() && BossRole==0) { PendingAttackCenter=Enemy->GetActorLocation(); PendingAttackRadius=260.f; }
    else if(Enemy->IsBoss() && BossRole==1) { PendingAttackRadius=620.f; PendingAttackCenter=Enemy->GetActorLocation()+PendingAttackDirection*PendingAttackRadius*0.5f; }
    else { PendingAttackCenter=bBossArea?TargetActor->GetActorLocation():TargetActor->GetActorLocation(); PendingAttackRadius=bBossArea?BossTelegraphRadius:MinionTelegraphRadius; }
    PendingAttackTarget = TargetActor;
    bAttackWindingUp = true;
    Enemy->GetAbilitySystemComponent()->AddLooseGameplayTag(
        UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
    Enemy->BeginShapedAttackTelegraph(PendingAttackCenter, PendingAttackRadius, Windup, PendingAttackShape, PendingAttackDirection);
    if (Enemy->IsBoss())
    {
        Enemy->PlayBossAttackAnimation();
    }
    else
    {
        Enemy->PlayMinionAttackAnimation();
    }
    GetWorldTimerManager().SetTimer(AttackWindupTimer, this, &ThisClass::ResolveTelegraphedAttack, Windup, false);
    UE_LOG(LogTemp, Log, TEXT("Enemy %s attack telegraph Center=%s Radius=%.0f Windup=%.2f"),
        *GetNameSafe(Enemy), *PendingAttackCenter.ToCompactString(), PendingAttackRadius, Windup);
}

void ACVADEnemyAIController::ResolveTelegraphedAttack()
{
    bAttackWindingUp = false;
    ACVADEnemyCharacter* Enemy = Cast<ACVADEnemyCharacter>(GetPawn());
    if (!Enemy) return;
    Enemy->EndAttackTelegraph();
    Enemy->GetAbilitySystemComponent()->RemoveLooseGameplayTag(
        UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
    if (Enemy->IsHitStunned()) return;
    if (!PendingAttackTarget.IsValid() || PendingAttackTarget->IsActorBeingDestroyed()) return;
    if (const ACVADCharacter* PendingPlayer=Cast<ACVADCharacter>(PendingAttackTarget.Get());
        PendingPlayer && PendingPlayer->IsPlayerDown()) { PendingAttackTarget.Reset(); return; }
    UAbilitySystemComponent* SourceASC = Enemy->GetAbilitySystemComponent();
    if (!SourceASC) return;

    int32 HitCount = 0;
    for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
    {
        ACVADCharacter* Target = *It;
        if (!Target || Target->IsPlayerDown()) continue;
        const FVector ToTarget=Target->GetActorLocation()-Enemy->GetActorLocation();
        bool bInside=false;
        if(PendingAttackShape==1)
            bInside=ToTarget.SizeSquared2D()<=FMath::Square(PendingAttackRadius) && FVector::DotProduct(ToTarget.GetSafeNormal2D(),PendingAttackDirection)>=0.707f;
        else if(PendingAttackShape==2)
        {
            const FVector Relative=Target->GetActorLocation()-PendingAttackCenter;
            bInside=FMath::Abs(FVector::DotProduct(Relative,PendingAttackDirection))<=PendingAttackRadius*0.5f &&
                FMath::Abs(FVector::DotProduct(Relative,FVector::CrossProduct(FVector::UpVector,PendingAttackDirection)))<=PendingAttackRadius*0.28f;
        }
        else bInside=FVector::DistSquared2D(Target->GetActorLocation(),PendingAttackCenter)<=FMath::Square(PendingAttackRadius);
        if(!bInside) continue;
        UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
        if (!TargetASC || TargetASC->HasMatchingGameplayTag(
            UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Invulnerable")))) continue;

        FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCVADDamageEffect::StaticClass(), 1.f, SourceASC->MakeEffectContext());
        if (!Spec.IsValid()) continue;
        Spec.Data->SetSetByCallerMagnitude(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage")), -AttackDamage);
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
        ++HitCount;
        if (!Enemy->IsBoss()) break;
    }
    UE_LOG(LogTemp, Log, TEXT("Enemy %s resolved telegraphed attack Phase=%d Hits=%d Damage=%.1f"),
        *GetNameSafe(Enemy), ActiveBossPhase, HitCount, AttackDamage);
    PendingAttackTarget.Reset();
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
