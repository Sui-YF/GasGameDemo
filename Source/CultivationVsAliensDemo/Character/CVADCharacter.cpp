#include "Character/CVADCharacter.h"
#include "AbilitySystemComponent.h"
#include "Player/CVADPlayerState.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/CVADCombatAbility.h"
#include "Inventory/CVADInventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Components/SphereComponent.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "Engine/OverlapResult.h"
#include "Battle/CVADBattleDirector.h"
#include "EngineUtils.h"
#include "AbilitySystem/Effects/CVADDamageEffect.h"
#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVADAbilityInput, Log, All);

ACVADCharacter::ACVADCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->MaxAcceleration = 2200.f;
    GetCharacterMovement()->BrakingDecelerationWalking = 1800.f;
    GetCharacterMovement()->GroundFriction = 8.f;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
    GetCharacterMovement()->bUseSeparateBrakingFriction = true;
    GetCharacterMovement()->BrakingFriction = 8.f;
    GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = true;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 420.f;
    CameraBoom->bUsePawnControlRotation = true;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
    // Keep player equipment/weapon animation efficient when the remote player is outside a client's view.
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquipmentHead"));
    UpperBodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquipmentUpperBody"));
    LowerBodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquipmentLowerBody"));
    FeetMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquipmentFeet"));
    HandsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquipmentHands"));
    for (USkeletalMeshComponent* Component : {HeadMesh, UpperBodyMesh, LowerBodyMesh, FeetMesh, HandsMesh})
    {
        Component->SetupAttachment(GetMesh());
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    SwordMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Sword"));
    SwordMesh->SetupAttachment(GetMesh(), TEXT("Weapon_r"));
    SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SwordMesh->SetIsReplicated(true);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SwordAsset(TEXT("/Game/LanFang/Meshes/Weapons/SK_Sword.SK_Sword"));
    if (SwordAsset.Succeeded()) SwordMesh->SetSkeletalMesh(SwordAsset.Object);

    static ConstructorHelpers::FClassFinder<UAnimInstance> FlyingSwordAnimBP(
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSword"));
    if (FlyingSwordAnimBP.Succeeded()) FlyingSwordAnimClass = FlyingSwordAnimBP.Class;

    FlyingSwordMeshLeft = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FlyingSwordLeft"));
    FlyingSwordMeshRight = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FlyingSwordRight"));
    FlyingSwordMeshLeft->SetupAttachment(GetMesh(), TEXT("Weapon_l"));
    FlyingSwordMeshRight->SetupAttachment(GetMesh(), TEXT("WEAPON_M"));
    for (USkeletalMeshComponent* FlyingSword : {FlyingSwordMeshLeft, FlyingSwordMeshRight})
    {
        FlyingSword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FlyingSword->SetIsReplicated(true);
        if (SwordAsset.Succeeded()) FlyingSword->SetSkeletalMesh(SwordAsset.Object);
        FlyingSword->SetVisibility(false, true);
    }

    SwordCollisionCenter = CreateDefaultSubobject<USphereComponent>(TEXT("SwordCollisionCenter"));
    SwordCollisionLeft = CreateDefaultSubobject<USphereComponent>(TEXT("SwordCollisionLeft"));
    SwordCollisionRight = CreateDefaultSubobject<USphereComponent>(TEXT("SwordCollisionRight"));
    const TArray<TPair<USphereComponent*, USkeletalMeshComponent*>> SwordCollisions = {
        {SwordCollisionCenter, SwordMesh}, {SwordCollisionLeft, FlyingSwordMeshLeft}, {SwordCollisionRight, FlyingSwordMeshRight}};
    for (const auto& Pair : SwordCollisions)
    {
        Pair.Key->SetupAttachment(Pair.Value);
        Pair.Key->SetSphereRadius(38.f);
        Pair.Key->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Pair.Key->SetCollisionResponseToAllChannels(ECR_Ignore);
        Pair.Key->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        Pair.Key->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleFlyingSwordOverlap);
    }
}

void ACVADCharacter::BeginPlay()
{
    Super::BeginPlay();
    NormalAnimClass = GetMesh() ? GetMesh()->GetAnimClass() : nullptr;
}

UAbilitySystemComponent* ACVADCharacter::GetAbilitySystemComponent() const
{
    const ACVADPlayerState* CVADPlayerState = GetPlayerState<ACVADPlayerState>();
    return CVADPlayerState ? CVADPlayerState->GetAbilitySystemComponent() : nullptr;
}

void ACVADCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
    GrantDefaultAbilities();
    BindEquipment();
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->GetGameplayAttributeValueChangeDelegate(UCVADAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandlePlayerHealthChanged);
}

void ACVADCharacter::HandlePlayerHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    if (!HasAuthority() || bPlayerDown || ChangeData.NewValue > 0.f) return;
    bPlayerDown = true;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->AddLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Downed")));
    ApplyDownedState();
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { It->RegisterPlayerDown(); break; }
    UE_LOG(LogCVADAbilityInput, Log, TEXT("Player %s is down"), *GetName());
}

void ACVADCharacter::OnRep_PlayerDown()
{
    ApplyDownedState();
}

void ACVADCharacter::ApplyDownedState()
{
    if (bPlayerDown && bSprinting)
    {
        bSprinting = false;
        GetWorldTimerManager().ClearTimer(SprintDrainTimer);
    }
    ApplySprintSpeed();
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        if (bPlayerDown) Movement->DisableMovement();
        else if (Movement->MovementMode == MOVE_None) Movement->SetMovementMode(MOVE_Walking);
    }
    if (AController* OwningController = GetController())
    {
        OwningController->SetIgnoreMoveInput(bPlayerDown);
        OwningController->SetIgnoreLookInput(false);
    }
    if (bPlayerDown)
    {
        bCombatInputLocked = false;
        BufferedCombatInput = INDEX_NONE;
        PendingActionAnimation = nullptr;
        HandleActionAnimationFinished();
    }
}

void ACVADCharacter::ServerTryReviveNearbyPlayer_Implementation()
{
    ACVADCharacter* Best = nullptr;
    float BestSq = FMath::Square(275.f);
    for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
    {
        if (*It == this || !It->IsPlayerDown()) continue;
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < BestSq) { BestSq = DistanceSq; Best = *It; }
    }
    if (Best) Best->RevivePlayer(0.5f);
}

void ACVADCharacter::RevivePlayer(float HealthPercent)
{
    if (!HasAuthority() || !bPlayerDown) return;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC) return;
    const float MaxHealth = ASC->GetNumericAttribute(UCVADAttributeSet::GetMaxHealthAttribute());
    ASC->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), MaxHealth * FMath::Clamp(HealthPercent, 0.1f, 1.f));
    bPlayerDown = false;
    ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Downed")));
    ApplyDownedState();
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { It->RegisterPlayerRevived(); break; }
    UE_LOG(LogCVADAbilityInput, Log, TEXT("Player %s revived"), *GetName());
}

void ACVADCharacter::BeginTemporaryInvulnerability(float Duration)
{
    if (!HasAuthority()) return;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->AddLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Invulnerable")));
    GetWorldTimerManager().ClearTimer(InvulnerabilityTimer);
    GetWorldTimerManager().SetTimer(InvulnerabilityTimer, this, &ThisClass::EndTemporaryInvulnerability,
        FMath::Max(0.05f, Duration), false);
}

void ACVADCharacter::EndTemporaryInvulnerability()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Invulnerable")));
}

void ACVADCharacter::GrantDefaultAbilities()
{
    if (!HasAuthority() || bDefaultAbilitiesGranted) return;
    if (ACVADPlayerState* CVADPlayerState = GetPlayerState<ACVADPlayerState>())
    {
        CVADPlayerState->InitializeDefaultSkillLoadout();
        bDefaultAbilitiesGranted = true;
        return;
    }
    UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
    if (!AbilitySystem) return;
    for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
    {
        if (AbilityClass) AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
    }
    bDefaultAbilitiesGranted = true;
}

void ACVADCharacter::ActivateCombatInput(ECVADAbilityInput Input)
{
    if (bPlayerDown)
    {
        UE_LOG(LogCVADAbilityInput, Verbose, TEXT("Combat input ignored while downed"));
        return;
    }
    if (bFlyingSwordMode && (Input == ECVADAbilityInput::LightAttack || Input == ECVADAbilityInput::HeavyAttack))
    {
        Input = ECVADAbilityInput::FlyingSword;
    }
    if (bCombatInputLocked)
    {
        if (BufferedCombatInput == INDEX_NONE) BufferedCombatInput = static_cast<int32>(Input);
        UE_LOG(LogCVADAbilityInput, Log, TEXT("Combat input buffered Slot=%d"), static_cast<int32>(Input));
        return;
    }
    UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
    if (!AbilitySystem)
    {
        UE_LOG(LogCVADAbilityInput, Warning, TEXT("Ability input %d ignored: no ASC on %s"), static_cast<int32>(Input), *GetNameSafe(this));
        return;
    }
    for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
    {
        if (Spec.InputID == static_cast<int32>(Input))
        {
            bCombatInputLocked = true;
            const bool bActivated = AbilitySystem->TryActivateAbility(Spec.Handle);
            if (!bActivated) bCombatInputLocked = false;
            UE_LOG(LogCVADAbilityInput, Log, TEXT("Ability input %d -> %s Activated=%s"),
                static_cast<int32>(Input), *GetNameSafe(Spec.Ability), bActivated ? TEXT("true") : TEXT("false"));
            return;
        }
    }
    UE_LOG(LogCVADAbilityInput, Warning, TEXT("Ability input %d has no granted matching ability. GrantedCount=%d"),
        static_cast<int32>(Input), AbilitySystem->GetActivatableAbilities().Num());
    bCombatInputLocked = false;
}

void ACVADCharacter::SetSprinting(bool bNewSprinting)
{
    if (bPlayerDown) bNewSprinting = false;
    bSprinting = bNewSprinting;
    ApplySprintSpeed();
    if (HasAuthority()) ServerSetSprinting_Implementation(bNewSprinting);
    else ServerSetSprinting(bNewSprinting);
}

void ACVADCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    bSprinting = bNewSprinting && !bPlayerDown && ASC &&
        ASC->GetNumericAttribute(UCVADAttributeSet::GetStaminaAttribute()) > 0.f;
    ApplySprintSpeed();
    GetWorldTimerManager().ClearTimer(SprintDrainTimer);
    if (bSprinting) GetWorldTimerManager().SetTimer(SprintDrainTimer, this, &ThisClass::DrainSprintStamina, 0.25f, true);
    ForceNetUpdate();
}

void ACVADCharacter::DrainSprintStamina()
{
    if (!HasAuthority() || !bSprinting) return;
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
    if (!ASC) { ServerSetSprinting_Implementation(false); return; }
    const float Current = ASC->GetNumericAttribute(UCVADAttributeSet::GetStaminaAttribute());
    if (Current <= 0.f) { ServerSetSprinting_Implementation(false); return; }
    ASC->ApplyModToAttribute(UCVADAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, -SprintStaminaPerSecond * 0.25f);
}

void ACVADCharacter::OnRep_Sprinting() { ApplySprintSpeed(); }
void ACVADCharacter::ApplySprintSpeed()
{
    if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
}

void ACVADCharacter::PlayReplicatedActionAnimation(UAnimSequenceBase* Animation)
{
    if (!Animation) return;
    if (HasAuthority()) MulticastPlayActionAnimation(Animation);
}

void ACVADCharacter::QueueAttackDamage(float Damage, float Distance, float Radius, bool bAllowMultipleTargets)
{
    if (!HasAuthority()) return;
    PendingAttackDamage = Damage;
    PendingAttackDistance = Distance;
    PendingAttackRadius = Radius;
    bPendingAttackHitsMultiple = bAllowMultipleTargets;
    bPendingAttackDamage = Damage > 0.f;
}

void ACVADCharacter::HandleAttackHitNotify()
{
    if (!HasAuthority() || !bPendingAttackDamage) return;
    bPendingAttackDamage = false; // Every attack animation may deal damage only once.

    const FVector Center = GetActorLocation() + GetActorForwardVector() * PendingAttackDistance;
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CVADAttackNotify), false, this);
    GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
        FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects),
        FCollisionShape::MakeSphere(PendingAttackRadius), Params);

    TSet<AActor*> DamagedActors;
    for (const FOverlapResult& Result : Overlaps)
    {
        AActor* Target = Result.GetActor();
        if (!Target || Target == this || DamagedActors.Contains(Target)) continue;
        IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(Target);
        UAbilitySystemComponent* TargetASC = AbilityInterface ? AbilityInterface->GetAbilitySystemComponent() : nullptr;
        if (!TargetASC) continue;
        if (TargetASC->HasMatchingGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Invulnerable")))) continue;
        UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
        if (!SourceASC) continue;
        FGameplayEffectSpecHandle DamageSpec = SourceASC->MakeOutgoingSpec(UCVADDamageEffect::StaticClass(), 1.f, SourceASC->MakeEffectContext());
        if (!DamageSpec.IsValid()) continue;
        DamageSpec.Data->SetSetByCallerMagnitude(
            UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage")), -PendingAttackDamage);
        SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
        DamagedActors.Add(Target);
        UE_LOG(LogCVADAbilityInput, Log, TEXT("Attack notify hit %s Damage=%.1f"), *GetNameSafe(Target), PendingAttackDamage);
        if (!bPendingAttackHitsMultiple) break;
    }
}

void ACVADCharacter::MulticastPlayActionAnimation_Implementation(UAnimSequenceBase* Animation)
{
    PlayActionAnimationLocal(Animation);
}

void ACVADCharacter::PlayActionAnimationLocal(UAnimSequenceBase* Animation)
{
    if (!Animation || !GetMesh()) return;
    if (bActionAnimationPlaying)
    {
        if (!PendingActionAnimation)
        {
            PendingActionAnimation = Animation;
            UE_LOG(LogCVADAbilityInput, Log, TEXT("Queued one action animation %s"), *GetNameSafe(Animation));
        }
        return;
    }
    StartActionAnimation(Animation);
}

void ACVADCharacter::StartActionAnimation(UAnimSequenceBase* Animation)
{
    if (!Animation || !GetMesh()) return;
    bActionAnimationPlaying = true;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->AddLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
    GetWorldTimerManager().ClearTimer(ActionAnimationTimer);
    GetMesh()->PlayAnimation(Animation, false);
    if (UAnimInstance* ActionInstance = GetMesh()->GetAnimInstance())
    {
        // Attacks animate the body but CharacterMovement remains authoritative, allowing move/jump during attacks.
        ActionInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    }
    const float Duration = FMath::Max(0.05f, Animation->GetPlayLength());
    if (HasAuthority() && bPendingAttackDamage)
    {
        GetWorldTimerManager().ClearTimer(AttackDamageTimer);
        GetWorldTimerManager().SetTimer(AttackDamageTimer, this, &ThisClass::HandleAttackHitNotify,
            FMath::Clamp(Duration * 0.45f, 0.08f, Duration - 0.02f), false);
    }
    // Notify is authoritative for sequencing. This delayed timer is only a safety fallback.
    GetWorldTimerManager().SetTimer(ActionAnimationTimer, this, &ThisClass::HandleActionAnimationFinished, Duration + 0.15f, false);
    UE_LOG(LogCVADAbilityInput, Log, TEXT("Playing replicated action animation %s Duration=%.2f"), *GetNameSafe(Animation), Duration);
}

void ACVADCharacter::HandleActionAnimationFinished()
{
    if (!bActionAnimationPlaying) return;
    GetWorldTimerManager().ClearTimer(ActionAnimationTimer);
    GetWorldTimerManager().ClearTimer(AttackDamageTimer);
    bActionAnimationPlaying = false;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
    bPendingAttackDamage = false;
    if (PendingActionAnimation)
    {
        UAnimSequenceBase* Next = PendingActionAnimation;
        PendingActionAnimation = nullptr;
        StartActionAnimation(Next);
        return;
    }
    RestoreLocomotionAnimation();
    bCombatInputLocked = false;
    const int32 NextInput = BufferedCombatInput;
    BufferedCombatInput = INDEX_NONE;
    if (NextInput != INDEX_NONE && IsLocallyControlled())
    {
        ActivateCombatInput(static_cast<ECVADAbilityInput>(NextInput));
    }
}

void ACVADCharacter::ToggleFlyingSwordMode()
{
    if (!HasAuthority()) return;
    bFlyingSwordMode = !bFlyingSwordMode;
    ApplySwordVisualMode();
    ForceNetUpdate();
    UE_LOG(LogCVADAbilityInput, Log, TEXT("Flying sword mode=%s"), bFlyingSwordMode ? TEXT("true") : TEXT("false"));
}

void ACVADCharacter::OnRep_FlyingSwordMode() { ApplySwordVisualMode(); }

void ACVADCharacter::ApplySwordVisualMode()
{
    if (!SwordMesh || !FlyingSwordMeshLeft || !FlyingSwordMeshRight || !GetMesh()) return;

    // LanFang's original flying-sword sequences animate these three weapon bones.
    // Keep every mesh snapped to its authored bone and let the source animation position it.
    SwordMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("Weapon_r"));
    FlyingSwordMeshLeft->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("Weapon_l"));
    FlyingSwordMeshRight->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("WEAPON_M"));
    SwordMesh->SetVisibility(true, true);
    FlyingSwordMeshLeft->SetVisibility(bFlyingSwordMode, true);
    FlyingSwordMeshRight->SetVisibility(bFlyingSwordMode, true);
    const ECollisionEnabled::Type CollisionMode = bFlyingSwordMode ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
    SwordCollisionCenter->SetCollisionEnabled(CollisionMode);
    SwordCollisionLeft->SetCollisionEnabled(CollisionMode);
    SwordCollisionRight->SetCollisionEnabled(CollisionMode);

    if (!bActionAnimationPlaying)
    {
        RestoreLocomotionAnimation();
    }
}

void ACVADCharacter::HandleFlyingSwordOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!HasAuthority() || !bFlyingSwordMode || !OtherActor || OtherActor == this) return;
    ACVADEnemyCharacter* Enemy = Cast<ACVADEnemyCharacter>(OtherActor);
    UAbilitySystemComponent* EnemyASC = Enemy ? Enemy->GetAbilitySystemComponent() : nullptr;
    if (!EnemyASC) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (const double* LastHit = FlyingSwordLastHitTimes.Find(OtherActor); LastHit && Now - *LastHit < FlyingSwordHitInterval) return;
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
    if (!SourceASC) return;
    FGameplayEffectSpecHandle DamageSpec = SourceASC->MakeOutgoingSpec(UCVADDamageEffect::StaticClass(), 1.f, SourceASC->MakeEffectContext());
    if (!DamageSpec.IsValid()) return;
    DamageSpec.Data->SetSetByCallerMagnitude(
        UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage")), -FlyingSwordContactDamage);
    SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), EnemyASC);
    FlyingSwordLastHitTimes.Add(OtherActor, Now);
    UE_LOG(LogCVADAbilityInput, Log, TEXT("Flying sword collision hit %s Damage=%.1f"), *GetNameSafe(Enemy), FlyingSwordContactDamage);
}

void ACVADCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACVADCharacter, bFlyingSwordMode);
    DOREPLIFETIME(ACVADCharacter, bPlayerDown);
    DOREPLIFETIME(ACVADCharacter, bSprinting);
}

void ACVADCharacter::RestoreLocomotionAnimation()
{
    if (!GetMesh()) return;
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    const TSubclassOf<UAnimInstance> DesiredClass = bFlyingSwordMode && FlyingSwordAnimClass
        ? FlyingSwordAnimClass : NormalAnimClass;
    if (DesiredClass && GetMesh()->GetAnimClass() != DesiredClass)
    {
        GetMesh()->SetAnimInstanceClass(DesiredClass);
    }
    if (UAnimInstance* LocomotionInstance = GetMesh()->GetAnimInstance())
    {
        LocomotionInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
    }
}

void ACVADCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilityActorInfo();
    BindEquipment();
}

void ACVADCharacter::BindEquipment()
{
    ACVADPlayerState* CVADPlayerState = GetPlayerState<ACVADPlayerState>();
    UCVADInventoryComponent* Inventory = CVADPlayerState ? CVADPlayerState->GetInventoryComponent() : nullptr;
    if (!Inventory) return;

    Inventory->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
    Inventory->OnEquipmentChanged.AddDynamic(this, &ThisClass::HandleEquipmentChanged);
    HandleEquipmentChanged(Inventory->GetEquipmentLoadout());
}

void ACVADCharacter::HandleEquipmentChanged(const FCVADEquipmentLoadout& NewLoadout)
{
    for (USkeletalMeshComponent* Component : {HeadMesh, UpperBodyMesh, LowerBodyMesh, FeetMesh, HandsMesh})
    {
        Component->SetLeaderPoseComponent(GetMesh());
    }
    SetEquipmentMesh(HeadMesh, NewLoadout.Head);
    SetEquipmentMesh(UpperBodyMesh, NewLoadout.UpperBody);
    SetEquipmentMesh(LowerBodyMesh, NewLoadout.LowerBody);
    SetEquipmentMesh(FeetMesh, NewLoadout.Feet);
    SetEquipmentMesh(HandsMesh, NewLoadout.Hands);
}

void ACVADCharacter::SetEquipmentMesh(USkeletalMeshComponent* Component, FName ItemId)
{
    if (!Component) return;
    static const TMap<FName, FString> MeshPaths = {
        {TEXT("Head.BambooHat"), TEXT("/Game/LanFang/Meshes/Characters/Separates/Hats/SK_BambooHat_A.SK_BambooHat_A")},
        {TEXT("Head.Helmet"), TEXT("/Game/LanFang/Meshes/Characters/Separates/Hats/SK_Helmet_A.SK_Helmet_A")},
        {TEXT("Upper.Armor"), TEXT("/Game/LanFang/Meshes/Characters/Separates/TopBodies/SK_TopBody_A.SK_TopBody_A")},
        {TEXT("Upper.Robe"), TEXT("/Game/LanFang/Meshes/Characters/Separates/TopBodies/SK_TopBody_B.SK_TopBody_B")},
        {TEXT("Lower.Default"), TEXT("/Game/LanFang/Meshes/Characters/Separates/BotBodies/SK_BotBody_A.SK_BotBody_A")},
        {TEXT("Lower.Alt"), TEXT("/Game/LanFang/Meshes/Characters/Separates/BotBodies/SK_BotBody_B.SK_BotBody_B")},
        {TEXT("Feet.Boots"), TEXT("/Game/LanFang/Meshes/Characters/Separates/Shoes/SK_Boots_A.SK_Boots_A")},
        {TEXT("Feet.Shoes"), TEXT("/Game/LanFang/Meshes/Characters/Separates/Shoes/SK_Shoes_A.SK_Shoes_A")},
        {TEXT("Hands.Gauntlets"), TEXT("/Game/LanFang/Meshes/Characters/Separates/TopBodies/SK_Gauntlets.SK_Gauntlets")}
    };

    const FString* Path = MeshPaths.Find(ItemId);
    Component->SetSkeletalMesh(Path ? LoadObject<USkeletalMesh>(nullptr, **Path) : nullptr);
}

void ACVADCharacter::InitializeAbilityActorInfo()
{
    if (ACVADPlayerState* CVADPlayerState = GetPlayerState<ACVADPlayerState>())
    {
        CVADPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(CVADPlayerState, this);
    }
}
