#include "Enemy/CVADEnemyCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Battle/CVADBattleDirector.h"
#include "Battle/CVADLootPickup.h"
#include "EngineUtils.h"
#include "Enemy/CVADEnemyAIController.h"
#include "Battle/CVADMinionSpawner.h"
#include "Data/CVADBalanceRows.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/CVADPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Character/CVADCharacter.h"
#include "TimerManager.h"
#include "AIController.h"
#include "GameplayTagsManager.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

ACVADEnemyCharacter::ACVADEnemyCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    AIControllerClass = ACVADEnemyAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    AttributeSet = CreateDefaultSubobject<UCVADAttributeSet>(TEXT("AttributeSet"));
    AngelWingLeft=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AngelWingLeft"));
    AngelWingLeft->SetupAttachment(GetMesh()); AngelWingLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AngelWingRight=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AngelWingRight"));
    AngelWingRight->SetupAttachment(GetMesh()); AngelWingRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AngelSword=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AngelSword"));
    AngelSword->SetupAttachment(GetMesh(),TEXT("Weapon_r")); AngelSword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Demo/debug phase: keep enemies visible regardless of camera distance.
    GetMesh()->SetCullDistance(0.f);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    bAlwaysRelevant = true;
    GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = true;
}

UAbilitySystemComponent* ACVADEnemyCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACVADEnemyCharacter::SetSpawnSource(ACVADMinionSpawner* InSpawnSource)
{
    SpawnSource = InSpawnSource;
}

void ACVADEnemyCharacter::SetBossRole(int32 NewRole)
{
    if(!HasAuthority() || !bIsBoss) return;
    BossRole=FMath::Clamp(NewRole,0,2);
    if(ACVADEnemyAIController* AI=Cast<ACVADEnemyAIController>(GetController())) AI->ConfigureBossRole(BossRole);
    ApplyBossRoleVisuals(); OnBossRoleChanged(BossRole); ForceNetUpdate();
    UE_LOG(LogTemp,Log,TEXT("Angel Boss %s role=%d"),*GetName(),BossRole);
}

void ACVADEnemyCharacter::PlayBossAttackAnimation()
{
    if(HasAuthority() && bIsBoss) MulticastPlayBossAttack(BossRole);
}

void ACVADEnemyCharacter::MulticastPlayBossAttack_Implementation(int32 AttackRole)
{
    UAnimSequenceBase* Sequence = AttackRole == 0
        ? SwordBossAttack
        : (AttackRole == 1 ? WingBossAttack : CasterBossAttack);
    USkeletalMeshComponent* MeshComponent = GetMesh();
    if (!MeshComponent || !Sequence) return;

    BossAnimationClass = MeshComponent->GetAnimClass();
    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
    bool bPlayedRootMotionMontage = false;
    if (AnimInstance)
    {
        AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
        UAnimMontage* BossMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
            Sequence, FAnimSlotGroup::DefaultSlotName, 0.1f, 0.25f, 1.f, 1);
        if (BossMontage)
        {
            bPlayedRootMotionMontage = true;
            UE_LOG(LogTemp, Verbose, TEXT("Boss %s is playing root-motion montage %s"),
                *GetName(), *GetNameSafe(Sequence));
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Boss %s AnimBP has no DefaultSlot; falling back to in-place animation %s"),
                *GetName(), *GetNameSafe(Sequence));
        }
    }

    if (!bPlayedRootMotionMontage)
    {
        // The imported role meshes use different animation blueprints and may not
        // expose DefaultSlot. Keep the old in-place fallback functional.
        MeshComponent->PlayAnimation(Sequence, false);
    }
    GetWorldTimerManager().ClearTimer(BossAnimationRestoreTimer);
    GetWorldTimerManager().SetTimer(BossAnimationRestoreTimer, this,
        &ThisClass::RestoreBossAnimationBlueprint,
        FMath::Max(Sequence->GetPlayLength(), 0.1f), false);
    UE_LOG(LogTemp, Verbose, TEXT("Boss %s is playing %s"),
        *GetName(), *GetNameSafe(Sequence));
}

void ACVADEnemyCharacter::RestoreBossAnimationBlueprint()
{
    if (!GetMesh() || !BossAnimationClass) return;

    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    GetMesh()->SetAnimInstanceClass(BossAnimationClass);
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
    }
}

void ACVADEnemyCharacter::PlayMinionAttackAnimation()
{
    if (HasAuthority() && !bIsBoss) MulticastPlayMinionAnimation(0);
}

void ACVADEnemyCharacter::PlayMinionHitAnimation()
{
    if (HasAuthority() && !bIsBoss) MulticastPlayMinionAnimation(1);
}

void ACVADEnemyCharacter::PlayMinionDeathAnimation()
{
    if (HasAuthority() && !bIsBoss) MulticastPlayMinionAnimation(2);
}

void ACVADEnemyCharacter::MulticastPlayMinionAnimation_Implementation(int32 AnimationType)
{
    if (bIsBoss || !GetMesh()) return;

    UAnimSequenceBase* Sequence = AnimationType == 0
        ? MinionAttackAnimation
        : (AnimationType == 1 ? MinionHitAnimation : MinionDeathAnimation);
    if (!Sequence) return;

    MinionAnimationClass = GetMesh()->GetAnimClass();
    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
        if (AnimInstance->PlaySlotAnimationAsDynamicMontage(
            Sequence, FAnimSlotGroup::DefaultSlotName, 0.05f, 0.12f, 1.f, 1))
        {
            GetWorldTimerManager().ClearTimer(MinionAnimationRestoreTimer);
            GetWorldTimerManager().SetTimer(MinionAnimationRestoreTimer, this,
                &ThisClass::RestoreMinionAnimationBlueprint,
                FMath::Max(Sequence->GetPlayLength(), 0.1f) + 0.1f, false);
            return;
        }
    }

    GetMesh()->PlayAnimation(Sequence, false);
    GetWorldTimerManager().ClearTimer(MinionAnimationRestoreTimer);
    GetWorldTimerManager().SetTimer(MinionAnimationRestoreTimer, this,
        &ThisClass::RestoreMinionAnimationBlueprint,
        FMath::Max(Sequence->GetPlayLength(), 0.1f) + 0.1f, false);
}

void ACVADEnemyCharacter::RestoreMinionAnimationBlueprint()
{
    if (!GetMesh()) return;
    if (MinionAnimationClass)
    {
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        GetMesh()->SetAnimInstanceClass(MinionAnimationClass);
        if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
        {
            AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
        }
    }
    else if (MinionIdleAnimation)
    {
        GetMesh()->PlayAnimation(MinionIdleAnimation, true);
    }
}

void ACVADEnemyCharacter::MakeRagdoll()
{
    if (!GetMesh() || !GetCapsuleComponent() || !GetCharacterMovement()) return;
    if (bRagdollFrozen) return;
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    GetMesh()->SetPhysicsBlendWeight(1.f);
    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetAllBodiesSimulatePhysics(true);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCapsuleComponent()->SetSimulatePhysics(false);
    GetCharacterMovement()->DisableMovement();
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    if (HasAuthority())
    {
        GetWorldTimerManager().ClearTimer(RagdollFreezeTimer);
        GetWorldTimerManager().SetTimer(RagdollFreezeTimer, this,
            &ThisClass::MulticastFreezeRagdoll, RagdollFreezeDelaySeconds, false);
    }
    UE_LOG(LogTemp, Log, TEXT("Enemy %s became a ragdoll"), *GetName());
}

void ACVADEnemyCharacter::MulticastRagdoll_Implementation()
{
    MakeRagdoll();
}

void ACVADEnemyCharacter::MulticastFreezeRagdoll_Implementation()
{
    FreezeRagdoll();
}

void ACVADEnemyCharacter::FreezeRagdoll()
{
    if (!GetMesh() || bRagdollFrozen) return;
    bRagdollFrozen = true;
    // Stop simulation and collision but keep the last simulated pose blended so
    // frozen corpses do not slide into each other or push the player around.
    GetMesh()->SetAllBodiesSimulatePhysics(false);
    GetMesh()->SetSimulatePhysics(false);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionProfileName(TEXT("NoCollision"));
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    UE_LOG(LogTemp, Log, TEXT("Enemy %s ragdoll frozen"), *GetName());
}

void ACVADEnemyCharacter::MulticastBossDeathSequence_Implementation()
{
    if (!bIsBoss || bBossDeathSequenceActive) return;
    bBossDeathSequenceActive = true;
    BeginBossDeathSlowMotion();
    StartBossDeathCamera();
    if (HasAuthority()) SpawnBossLoot();
}

void ACVADEnemyCharacter::SpawnBossLoot()
{
    if (!HasAuthority() || !GetWorld() || !bIsBoss) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACVADLootPickup* Loot = GetWorld()->SpawnActor<ACVADLootPickup>(
        ACVADLootPickup::StaticClass(),
        GetActorLocation() + FVector(0.f, 0.f, 90.f),
        FRotator::ZeroRotator,
        Params);
    if (Loot)
    {
        Loot->InitializeLoot(BossLootExperience, BossLootSkillPoints, TEXT("天穹三使的传承"));
    }
}

void ACVADEnemyCharacter::BeginBossDeathSlowMotion()
{
    if (!GetWorld()) return;
    UGameplayStatics::SetGlobalTimeDilation(this, BossDeathSlowMotionTimeDilation);
    // World timers advance at the dilated rate, so compensate the restore rate by
    // the dilation to fire after the intended real-time duration.
    const float RestoreRate = BossDeathSlowMotionDurationSeconds * BossDeathSlowMotionTimeDilation;
    GetWorldTimerManager().ClearTimer(BossSlowMotionRestoreTimer);
    GetWorldTimerManager().SetTimer(BossSlowMotionRestoreTimer, this,
        &ThisClass::EndBossDeathSlowMotion, RestoreRate, false);
    UE_LOG(LogTemp, Log, TEXT("Boss %s death slow motion Dilation=%.2f RestoreRate=%.2f"),
        *GetName(), BossDeathSlowMotionTimeDilation, RestoreRate);
}

void ACVADEnemyCharacter::EndBossDeathSlowMotion()
{
    if (GetWorld()) UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
}

void ACVADEnemyCharacter::StartBossDeathCamera()
{
    if (!GetWorld() || !bIsBoss) return;

    APlayerController* LocalPC = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->IsLocalController())
        {
            LocalPC = PC;
            break;
        }
    }
    if (!LocalPC) return;

    const FVector BossLocation = GetActorLocation();
    const FVector CameraOffset = -GetActorForwardVector() * 430.f + FVector(0.f, 0.f, 190.f);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    DeathCameraActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
        FTransform(GetActorRotation(), BossLocation + CameraOffset), Params);
    if (!DeathCameraActor.IsValid()) return;

    DeathCameraActor->SetActorRotation((BossLocation - DeathCameraActor->GetActorLocation()).Rotation());
    DeathCameraActor->SetLifeSpan(BossDeathCameraDurationSeconds + 2.f);
    LocalPC->SetViewTargetWithBlend(DeathCameraActor.Get(), 0.8f, VTBlend_Cubic, 0.f);

    const float RestoreRate = BossDeathCameraDurationSeconds
        * UGameplayStatics::GetGlobalTimeDilation(this);
    GetWorldTimerManager().ClearTimer(BossCameraRestoreTimer);
    GetWorldTimerManager().SetTimer(BossCameraRestoreTimer, this,
        &ThisClass::EndBossDeathCamera, RestoreRate, false);
    UE_LOG(LogTemp, Log, TEXT("Boss %s death camera started"), *GetName());
}

void ACVADEnemyCharacter::EndBossDeathCamera()
{
    if (!GetWorld()) return;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->IsLocalController() && PC->GetPawn())
        {
            PC->SetViewTarget(PC->GetPawn());
        }
    }
    UE_LOG(LogTemp, Log, TEXT("Boss %s death camera restored"), *GetName());
}

void ACVADEnemyCharacter::OnRep_BossRole(){ApplyBossRoleVisuals();OnBossRoleChanged(BossRole);}

void ACVADEnemyCharacter::ApplyBossRoleVisuals()
{
    if(!bIsBoss || !GetMesh()) return;
    USkeletalMesh* RoleMesh=BossRoleBodyMeshes.IsValidIndex(BossRole) ? BossRoleBodyMeshes[BossRole] : nullptr;
    if(RoleMesh && GetMesh()->GetSkeletalMeshAsset()!=RoleMesh) GetMesh()->SetSkeletalMesh(RoleMesh);
    if(AngelSword) AngelSword->SetVisibility(BossRole==0,true);
}

void ACVADEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    bIsBoss = bIsBoss || BalanceRowName == TEXT("Boss");
    if(GetMesh())
    {
        GetMesh()->SetHiddenInGame(false);
        GetMesh()->SetVisibility(true,true);
        GetMesh()->SetComponentTickEnabled(true);
        GetMesh()->GlobalAnimRateScale = EnemyAnimPlayRate;
        if(!bIsBoss && MinionIdleAnimation && !GetMesh()->GetAnimClass())
        {
            // Do not evaluate Manny's AnimBP on the SkeletonArmy skeleton. A compatible
            // single-node pose guarantees the spawned mesh is rendered until a dedicated
            // SkeletonArmy locomotion AnimBP is authored.
            GetMesh()->PlayAnimation(MinionIdleAnimation,true);
        }
        UE_LOG(LogTemp,Log,TEXT("Enemy visual Mesh=%s Anim=%s Hidden=%s Visible=%s Location=%s Scale=%s"),
            *GetNameSafe(GetMesh()->GetSkeletalMeshAsset()),*GetNameSafe(MinionIdleAnimation),
            GetMesh()->bHiddenInGame?TEXT("true"):TEXT("false"),GetMesh()->IsVisible()?TEXT("true"):TEXT("false"),
            *GetActorLocation().ToCompactString(),*GetActorScale3D().ToCompactString());
    }
    if(AngelWingLeft) AngelWingLeft->SetVisibility(bIsBoss,true);
    if(AngelWingRight) AngelWingRight->SetVisibility(bIsBoss,true);
    if(AngelSword) AngelSword->SetVisibility(false,true);
    if(GetMesh()) GetMesh()->SetCullDistance(0.f);
    for(USkeletalMeshComponent* Component : {AngelWingLeft,AngelWingRight,AngelSword})
        if(Component) Component->SetCullDistance(0.f);
    bAlwaysRelevant = true;
    if(!AbilitySystemComponent)
    {
        UE_LOG(LogTemp,Error,TEXT("Enemy %s has no AbilitySystemComponent; destroying invalid spawn"),*GetName());
        if(HasAuthority()) Destroy();
        return;
    }
    // Blueprint children created before AttributeSet became a reflected subobject can
    // deserialize the property as null. Recover it from the ASC, or register a fresh set.
    if(!AttributeSet)
    {
        AttributeSet=const_cast<UCVADAttributeSet*>(AbilitySystemComponent->GetSet<UCVADAttributeSet>());
        if(!AttributeSet)
        {
            AttributeSet=NewObject<UCVADAttributeSet>(this,TEXT("RuntimeAttributeSet"));
            AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
        }
        UE_LOG(LogTemp, Log, TEXT("Enemy %s restored its legacy Blueprint AttributeSet"), *GetName());
    }
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCVADAttributeSet::GetHealthAttribute())
        .AddUObject(this, &ThisClass::HandleHealthChanged);

    UDataTable* BalanceTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_EnemyBalance.DT_EnemyBalance"));
    if (const FCVADEnemyBalanceRow* Row = BalanceTable ? BalanceTable->FindRow<FCVADEnemyBalanceRow>(BalanceRowName, TEXT("EnemyBeginPlay")) : nullptr)
    {
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxHealthAttribute(), Row->MaxHealth);
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), Row->MaxHealth);
        GetCharacterMovement()->MaxWalkSpeed = Row->MoveSpeed * EnemyMoveSpeedMultiplier;
        if (bIsBoss)
        {
            GetCharacterMovement()->MaxWalkSpeed *= BossMoveSpeedMultiplier;
        }
        if (ACVADEnemyAIController* EnemyController = Cast<ACVADEnemyAIController>(GetController()))
        {
            EnemyController->ConfigureCombat(Row->AttackDamage, Row->AttackInterval);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("Enemy %s initialized Row=%s Boss=%s Health=%.0f"), *GetName(),
        *BalanceRowName.ToString(), bIsBoss ? TEXT("true") : TEXT("false"), AttributeSet->GetHealth());
    if (HasAuthority() && bIsBoss)
    {
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        { It->RegisterBoss(this); It->UpdateBossHealth(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth()); break; }
    }
}

void ACVADEnemyCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    if(HasAuthority() && !bIsBoss && bOneHitKillMinion && !bDeathHandled && !bApplyingOneHitKill &&
        ChangeData.NewValue < ChangeData.OldValue && ChangeData.NewValue > 0.f)
    {
        // Musou-demo rule: every valid player hit defeats a regular soldier. Keep
        // this on the enemy instead of inflating player damage so bosses retain
        // their intended health and phase balance.
        bApplyingOneHitKill=true;
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(),0.f);
        bApplyingOneHitKill=false;
        UE_LOG(LogTemp,Log,TEXT("Minion %s converted valid hit to one-hit defeat"),*GetName());
        return;
    }
    if (HasAuthority() && bIsBoss)
    {
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        {
            if (!It->IsBossStageReady() && ChangeData.NewValue < ChangeData.OldValue)
            {
                AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), AttributeSet->GetMaxHealth());
                return;
            }
            break;
        }
    }
    if (ChangeData.NewValue < ChangeData.OldValue)
    {
        OnEnemyDamaged(ChangeData.OldValue - ChangeData.NewValue);
        if (HasAuthority() && !bIsBoss)
        {
            if (ChangeData.NewValue > 0.f)
            {
                PlayMinionHitAnimation();
                BeginHitStun(MinionHitStunDuration);
                AActor* NearestPlayer = nullptr;
                float BestSq = TNumericLimits<float>::Max();
                for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
                {
                    const float Sq = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
                    if (Sq < BestSq) { BestSq = Sq; NearestPlayer = *It; }
                }
                if (NearestPlayer)
                {
                    const FVector Away = (GetActorLocation() - NearestPlayer->GetActorLocation()).GetSafeNormal2D();
                    LaunchCharacter(Away * HitReactionImpulse + FVector(0.f, 0.f, 110.f), true, true);
                }
            }
        }
    }
    if (HasAuthority() && bIsBoss && ChangeData.NewValue > 0.f) EvaluateBossPhase(ChangeData.NewValue);
    if (HasAuthority() && bIsBoss)
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        { It->UpdateBossHealth(ChangeData.NewValue, AttributeSet->GetMaxHealth()); break; }
    if (!HasAuthority() || bDeathHandled || ChangeData.NewValue > 0.f) return;
    bDeathHandled = true;
    if (ACVADEnemyAIController* AI = Cast<ACVADEnemyAIController>(GetController())) AI->CancelPendingAttack();
    if (!bIsBoss) PlayMinionDeathAnimation();
    MulticastRagdoll();
    if (bIsBoss) MulticastBossDeathSequence();
    if (bIsBoss) for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { It->CompleteBossBattle(this); break; }
    if (SpawnSource.IsValid()) SpawnSource->NotifySpawnedMinionDefeated(this);
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
    {
        It->RegisterMinionDefeated();
        break;
    }
    const int32 ExperienceReward = bIsBoss ? 500 : (BalanceRowName == TEXT("Captain") ? 60 : 25);
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { It->RegisterExperienceReward(ExperienceReward); break; }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ACVADPlayerState* RewardPlayerState = It->Get() ? It->Get()->GetPlayerState<ACVADPlayerState>() : nullptr)
            RewardPlayerState->AddExperience(ExperienceReward);
    }
    SetLifeSpan(5.f);
}

void ACVADEnemyCharacter::EvaluateBossPhase(float CurrentHealth)
{
    const float MaxHealth = FMath::Max(1.f, AttributeSet->GetMaxHealth());
    const float Ratio = CurrentHealth / MaxHealth;
    const int32 NewPhase = Ratio <= 0.35f ? 3 : (Ratio <= 0.70f ? 2 : 1);
    if (NewPhase == BossPhase) return;
    BossPhase = NewPhase;
    BeginHitStun(BossPhaseHitStunDuration);
    if (ACVADEnemyAIController* AI = Cast<ACVADEnemyAIController>(GetController())) AI->ApplyBossPhase(BossPhase);
    OnBossPhaseChanged(BossPhase);
    ForceNetUpdate();
}

void ACVADEnemyCharacter::OnRep_BossPhase() { OnBossPhaseChanged(BossPhase); }

void ACVADEnemyCharacter::BeginHitStun(float Duration)
{
    if (!HasAuthority() || Duration <= 0.f || bDeathHandled) return;
    if (ACVADEnemyAIController* AI = Cast<ACVADEnemyAIController>(GetController())) AI->CancelPendingAttack();
    bHitStunned = true;
    AbilitySystemComponent->AddLooseGameplayTag(
        UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.HitStunned")));
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    GetWorldTimerManager().ClearTimer(HitStunTimer);
    GetWorldTimerManager().SetTimer(HitStunTimer, this, &ThisClass::EndHitStun, Duration, false);
    OnHitStunChanged(true);
    ForceNetUpdate();
    UE_LOG(LogTemp, Log, TEXT("Enemy %s hit-stunned Duration=%.2f Boss=%s"), *GetName(), Duration,
        bIsBoss ? TEXT("true") : TEXT("false"));
}

void ACVADEnemyCharacter::EndHitStun()
{
    if (!HasAuthority()) return;
    bHitStunned = false;
    AbilitySystemComponent->RemoveLooseGameplayTag(
        UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.HitStunned")));
    OnHitStunChanged(false);
    ForceNetUpdate();
}

void ACVADEnemyCharacter::OnRep_HitStunned()
{
    OnHitStunChanged(bHitStunned);
}

void ACVADEnemyCharacter::BeginAttackTelegraph(const FVector& Center, float Radius, float Duration)
{
    BeginShapedAttackTelegraph(Center, Radius, Duration, 0, GetActorForwardVector());
}

void ACVADEnemyCharacter::BeginShapedAttackTelegraph(const FVector& Center, float Radius, float Duration, int32 Shape, const FVector& Direction)
{
    if (!HasAuthority() || bDeathHandled) return;
    bAttackTelegraphActive = true;
    AttackTelegraphCenter = Center;
    AttackTelegraphRadius = FMath::Max(0.f, Radius);
    AttackTelegraphDuration = FMath::Max(0.f, Duration);
    AttackTelegraphShape = FMath::Clamp(Shape, 0, 2);
    AttackTelegraphDirection = Direction.GetSafeNormal2D();
    OnAttackTelegraphChanged(true, AttackTelegraphCenter, AttackTelegraphRadius, AttackTelegraphDuration);
    DrawAttackTelegraphPlaceholder();
    ForceNetUpdate();
}

void ACVADEnemyCharacter::EndAttackTelegraph()
{
    if (!HasAuthority() || !bAttackTelegraphActive) return;
    bAttackTelegraphActive = false;
    OnAttackTelegraphChanged(false, AttackTelegraphCenter, AttackTelegraphRadius, 0.f);
    ForceNetUpdate();
}

void ACVADEnemyCharacter::OnRep_AttackTelegraph()
{
    OnAttackTelegraphChanged(bAttackTelegraphActive, AttackTelegraphCenter, AttackTelegraphRadius,
        bAttackTelegraphActive ? AttackTelegraphDuration : 0.f);
    if (bAttackTelegraphActive) DrawAttackTelegraphPlaceholder();
}

void ACVADEnemyCharacter::DrawAttackTelegraphPlaceholder() const
{
    if (!GetWorld() || !bAttackTelegraphActive || AttackTelegraphRadius <= 0.f) return;
    const FVector Center = FVector(AttackTelegraphCenter) + FVector(0.f, 0.f, 8.f);
    const FVector Forward=FVector(AttackTelegraphDirection).GetSafeNormal2D();
    const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
    if(AttackTelegraphShape==1)
    {
        const FVector Origin=GetActorLocation()+FVector(0,0,8); const float HalfAngle=45.f;
        DrawDebugLine(GetWorld(),Origin,Origin+Forward.RotateAngleAxis(-HalfAngle,FVector::UpVector)*AttackTelegraphRadius,FColor::Red,false,AttackTelegraphDuration,0,7.f);
        DrawDebugLine(GetWorld(),Origin,Origin+Forward.RotateAngleAxis(HalfAngle,FVector::UpVector)*AttackTelegraphRadius,FColor::Red,false,AttackTelegraphDuration,0,7.f);
        DrawDebugCircle(GetWorld(),Origin,AttackTelegraphRadius,24,FColor::Orange,false,AttackTelegraphDuration,0,3.f,Right,Forward,false);
    }
    else if(AttackTelegraphShape==2)
    {
        const float HalfWidth=AttackTelegraphRadius*0.28f; const FVector Start=Center-Forward*AttackTelegraphRadius*0.5f; const FVector End=Center+Forward*AttackTelegraphRadius*0.5f;
        DrawDebugLine(GetWorld(),Start+Right*HalfWidth,End+Right*HalfWidth,FColor::Yellow,false,AttackTelegraphDuration,0,7.f);
        DrawDebugLine(GetWorld(),Start-Right*HalfWidth,End-Right*HalfWidth,FColor::Yellow,false,AttackTelegraphDuration,0,7.f);
        DrawDebugLine(GetWorld(),End-Right*HalfWidth,End+Right*HalfWidth,FColor::Orange,false,AttackTelegraphDuration,0,5.f);
    }
    else DrawDebugCircle(GetWorld(),Center,AttackTelegraphRadius,48,FColor::Purple,false,AttackTelegraphDuration,0,7.f,FVector::RightVector,FVector::ForwardVector,false);
}

void ACVADEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACVADEnemyCharacter, BossPhase);
    DOREPLIFETIME(ACVADEnemyCharacter, BossRole);
    DOREPLIFETIME(ACVADEnemyCharacter, bHitStunned);
    DOREPLIFETIME(ACVADEnemyCharacter, bAttackTelegraphActive);
    DOREPLIFETIME(ACVADEnemyCharacter, AttackTelegraphCenter);
    DOREPLIFETIME(ACVADEnemyCharacter, AttackTelegraphRadius);
    DOREPLIFETIME(ACVADEnemyCharacter, AttackTelegraphDuration);
    DOREPLIFETIME(ACVADEnemyCharacter, AttackTelegraphShape);
    DOREPLIFETIME(ACVADEnemyCharacter, AttackTelegraphDirection);
}
