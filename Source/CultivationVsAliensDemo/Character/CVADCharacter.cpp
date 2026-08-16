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
#include "Animation/AnimMontage.h"
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
#include "Misc/ConfigCacheIni.h"
#include "SkeletalMeshMerge.h"

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
    HairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquipmentHair"));
    HatMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EquipmentHat"));
    for (USkeletalMeshComponent* Component : {HeadMesh, HairMesh, HatMesh, UpperBodyMesh, HandsMesh, LowerBodyMesh, FeetMesh})
    {
        Component->SetupAttachment(GetMesh());
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // The demo uses LanFang's authored combined body. Modular equipment was visually
        // redundant and could remain in reference pose while locomotion AnimBPs changed.
        Component->SetHiddenInGame(true);
        Component->SetVisibility(false, true);
        Component->SetComponentTickEnabled(false);
    }

    SwordMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Sword"));
    SwordMesh->SetupAttachment(GetMesh(), TEXT("Weapon_r"));
    SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SwordMesh->SetIsReplicated(true);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SwordAsset(TEXT("/Game/LanFang/Meshes/Weapons/SK_Sword.SK_Sword"));
    if (SwordAsset.Succeeded()) SwordMesh->SetSkeletalMesh(SwordAsset.Object);

    static ConstructorHelpers::FClassFinder<UAnimInstance> FlyingSwordAnimBP(
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSwordV2"));
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
    static const TArray<TArray<FString>> Choices={
        {TEXT("Heads/SK_Head_A"),TEXT("Heads/SK_Head_B"),TEXT("Heads/SK_Head_C")},
        {TEXT("Hairs/SK_Hair_A"),TEXT("Hairs/SK_Hair_ADyeing_01"),TEXT("Hairs/SK_Hair_ADyeing_02"),TEXT("Hairs/SK_Hair4MaskCloth_A")},
        {TEXT(""),TEXT("Hats/SK_BambooHat_A"),TEXT("Hats/SK_BambooHat_B"),TEXT("Hats/SK_BambooHat_C"),TEXT("Hats/SK_Helmet_A"),TEXT("Hats/SK_Helmet_B")},
        {TEXT("TopBodies/SK_TopBody_A"),TEXT("TopBodies/SK_TopBody_B"),TEXT("TopBodies/SK_TopBody_Base"),TEXT("TopBodies/SK_TopBody_C"),TEXT("TopBodies/SK_TopBody_D")},
        {TEXT("Hands/SK_Hands"),TEXT("Hands/SK_Hand_L"),TEXT("Hands/SK_Hand_R")},
        {TEXT("BotBodies/SK_BotBody_A"),TEXT("BotBodies/SK_BotBody_B"),TEXT("BotBodies/SK_BotBody_Base")},
        {TEXT("Shoes/SK_Boots_A"),TEXT("Shoes/SK_Boots_B"),TEXT("Shoes/SK_Feet"),TEXT("Shoes/SK_Shoes_A"),TEXT("Shoes/SK_Shoes_B"),TEXT("Shoes/SK_Shoes_C"),TEXT("Shoes/SK_Shoes_D")}};
    USkeletalMeshComponent* Parts[]={HeadMesh,HairMesh,HatMesh,UpperBodyMesh,HandsMesh,LowerBodyMesh,FeetMesh};
    TArray<USkeletalMesh*> AppearanceMeshes;
    for(int32 Part=0;Part<7;++Part)
    {
        int32 Index=0;if(GConfig)GConfig->GetInt(TEXT("CVAD.Appearance"),*FString::Printf(TEXT("Part%d"),Part),Index,GGameUserSettingsIni);
        Index=FMath::Clamp(Index,0,Choices[Part].Num()-1); const FString& Relative=Choices[Part][Index];
        USkeletalMesh* PartMesh=nullptr;if(!Relative.IsEmpty()){const FString Asset=FString::Printf(TEXT("/Game/LanFang/Meshes/Characters/Separates/%s.%s"),*Relative,*FPaths::GetBaseFilename(Relative));PartMesh=LoadObject<USkeletalMesh>(nullptr,*Asset);}
        Parts[Part]->SetSkeletalMesh(PartMesh);Parts[Part]->SetLeaderPoseComponent(GetMesh(),true,false);Parts[Part]->SetHiddenInGame(false);Parts[Part]->SetVisibility(true,true);Parts[Part]->SetComponentTickEnabled(false);
        if(PartMesh) AppearanceMeshes.Add(PartMesh);
    }
    if(!MergeAppearanceMeshes(AppearanceMeshes)) GetMesh()->SetVisibility(false,false);
}

UAbilitySystemComponent* ACVADCharacter::GetAbilitySystemComponent() const
{
    const ACVADPlayerState* CVADPlayerState = GetPlayerState<ACVADPlayerState>();
    return CVADPlayerState ? CVADPlayerState->GetAbilitySystemComponent() : nullptr;
}

void ACVADCharacter::ApplyAppearanceSelection(const int32 PartIndices[7])
{
    static const TArray<TArray<FString>> Choices={
        {TEXT("Heads/SK_Head_A"),TEXT("Heads/SK_Head_B"),TEXT("Heads/SK_Head_C")},
        {TEXT("Hairs/SK_Hair_A"),TEXT("Hairs/SK_Hair_ADyeing_01"),TEXT("Hairs/SK_Hair_ADyeing_02"),TEXT("Hairs/SK_Hair4MaskCloth_A")},
        {TEXT(""),TEXT("Hats/SK_BambooHat_A"),TEXT("Hats/SK_BambooHat_B"),TEXT("Hats/SK_BambooHat_C"),TEXT("Hats/SK_Helmet_A"),TEXT("Hats/SK_Helmet_B")},
        {TEXT("TopBodies/SK_TopBody_A"),TEXT("TopBodies/SK_TopBody_B"),TEXT("TopBodies/SK_TopBody_Base"),TEXT("TopBodies/SK_TopBody_C"),TEXT("TopBodies/SK_TopBody_D")},
        {TEXT("Hands/SK_Hands"),TEXT("Hands/SK_Hand_L"),TEXT("Hands/SK_Hand_R")},
        {TEXT("BotBodies/SK_BotBody_A"),TEXT("BotBodies/SK_BotBody_B"),TEXT("BotBodies/SK_BotBody_Base")},
        {TEXT("Shoes/SK_Boots_A"),TEXT("Shoes/SK_Boots_B"),TEXT("Shoes/SK_Feet"),TEXT("Shoes/SK_Shoes_A"),TEXT("Shoes/SK_Shoes_B"),TEXT("Shoes/SK_Shoes_C"),TEXT("Shoes/SK_Shoes_D")}};
    USkeletalMeshComponent* Parts[]={HeadMesh,HairMesh,HatMesh,UpperBodyMesh,HandsMesh,LowerBodyMesh,FeetMesh};
    TArray<USkeletalMesh*> AppearanceMeshes;
    for(int32 Part=0;Part<7;++Part)
    {
        const int32 Index=FMath::Clamp(PartIndices[Part],0,Choices[Part].Num()-1);
        const FString& Relative=Choices[Part][Index];
        USkeletalMesh* PartMesh=nullptr;
        if(!Relative.IsEmpty())
        {
            const FString Asset=FString::Printf(TEXT("/Game/LanFang/Meshes/Characters/Separates/%s.%s"),*Relative,*FPaths::GetBaseFilename(Relative));
            PartMesh=LoadObject<USkeletalMesh>(nullptr,*Asset);
        }
        Parts[Part]->SetSkeletalMesh(PartMesh);
        Parts[Part]->SetLeaderPoseComponent(GetMesh(),true,false);
        Parts[Part]->SetHiddenInGame(false);
        Parts[Part]->SetVisibility(true,true);
        if(PartMesh) AppearanceMeshes.Add(PartMesh);
    }
    if(!MergeAppearanceMeshes(AppearanceMeshes)) GetMesh()->SetVisibility(false,false);
}

bool ACVADCharacter::MergeAppearanceMeshes(const TArray<USkeletalMesh*>& SourceMeshes)
{
    if(!GetMesh() || SourceMeshes.Num()<2) return false;
    USkeletalMesh* Merged=NewObject<USkeletalMesh>(this,NAME_None,RF_Transient);
    const TArray<FSkelMeshMergeSectionMapping> SectionMappings;
    FSkeletalMeshMerge Merger(Merged,SourceMeshes,SectionMappings,0,EMeshBufferAccess::Default,
        static_cast<const FSkelMeshMergeUVTransformMapping*>(nullptr));
    if(!Merger.DoMerge())
    {
        UE_LOG(LogCVADAbilityInput,Warning,TEXT("Runtime appearance merge failed; using leader-pose components"));
        return false;
    }
    // FSkeletalMeshMerge builds render and reference-skeleton data but does not
    // automatically retain the source USkeleton asset. Without it the AnimBP can
    // be instantiated, yet dynamic montages fail their skeleton compatibility test.
    USkeleton* AppearanceSkeleton=nullptr;
    for(USkeletalMesh* SourceMesh : SourceMeshes)
    {
        if(SourceMesh && SourceMesh->GetSkeleton()) { AppearanceSkeleton=SourceMesh->GetSkeleton(); break; }
    }
    if(!AppearanceSkeleton)
    {
        UE_LOG(LogCVADAbilityInput,Error,TEXT("Runtime appearance merge produced no compatible Skeleton"));
        return false;
    }
    Merged->SetSkeleton(AppearanceSkeleton);
    RuntimeMergedAppearanceMesh=Merged;
    GetMesh()->SetSkeletalMesh(RuntimeMergedAppearanceMesh);
    GetMesh()->SetHiddenInGame(false);
    GetMesh()->SetVisibility(true,true);
    for(USkeletalMeshComponent* Part : {HeadMesh,HairMesh,HatMesh,UpperBodyMesh,HandsMesh,LowerBodyMesh,FeetMesh})
    {
        if(!Part) continue;
        Part->SetHiddenInGame(true);
        Part->SetVisibility(false,true);
    }
    RestoreLocomotionAnimation();
    GetMesh()->InitAnim(true);
    UE_LOG(LogCVADAbilityInput,Log,TEXT("Merged %d appearance meshes; AnimClass=%s Skeleton=%s"),
        SourceMeshes.Num(),*GetNameSafe(GetMesh()->GetAnimClass()),*GetNameSafe(RuntimeMergedAppearanceMesh->GetSkeleton()));
    return true;
}

void ACVADCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
    GrantDefaultAbilities();
    // Outfit swapping is intentionally disabled for this demo.
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->GetGameplayAttributeValueChangeDelegate(UCVADAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandlePlayerHealthChanged);
}

void ACVADCharacter::HandlePlayerHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    if (!HasAuthority() || bPlayerDown || ChangeData.NewValue >= ChangeData.OldValue) return;
    if (ChangeData.NewValue > 0.f)
    {
        // Attacks have poise for this musou-style demo: damage still applies, but
        // an enemy poke cannot cancel the current skill or corrupt its combo queue.
        if(!bActionAnimationPlaying) BeginPlayerHitStun(PlayerHitStunDuration);
        BeginTemporaryInvulnerability(PostHitInvulnerabilityDuration);
        return;
    }
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
    if (bPlayerDown)
    {
        bPlayerHitStunned = false;
        GetWorldTimerManager().ClearTimer(PlayerHitStunTimer);
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
            ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.HitStunned")));
    }
    ApplyPlayerControlState();
    if (bPlayerDown)
    {
        bCombatInputLocked = false;
        BufferedCombatInput = INDEX_NONE;
        PendingActionAnimation = nullptr;
        HandleActionAnimationFinished();
    }
}

void ACVADCharacter::BeginPlayerHitStun(float Duration)
{
    if (!HasAuthority() || bPlayerDown || Duration <= 0.f) return;
    bPlayerHitStunned = true;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->AddLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.HitStunned")));
    bSprinting = false;
    GetWorldTimerManager().ClearTimer(SprintDrainTimer);
    BufferedCombatInput = INDEX_NONE;
    PendingActionAnimation = nullptr;
    if (bActionAnimationPlaying) HandleActionAnimationFinished();

    AActor* NearestEnemy = nullptr;
    float BestSq = TNumericLimits<float>::Max();
    for (TActorIterator<ACVADEnemyCharacter> It(GetWorld()); It; ++It)
    {
        const float Sq = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
        if (Sq < BestSq) { BestSq = Sq; NearestEnemy = *It; }
    }
    if (NearestEnemy)
    {
        const FVector Away = (GetActorLocation() - NearestEnemy->GetActorLocation()).GetSafeNormal2D();
        LaunchCharacter(Away * PlayerHitImpulse + FVector(0.f, 0.f, 70.f), true, true);
    }
    ApplyPlayerControlState();
    OnPlayerHitStunChanged(true);
    GetWorldTimerManager().ClearTimer(PlayerHitStunTimer);
    GetWorldTimerManager().SetTimer(PlayerHitStunTimer, this, &ThisClass::EndPlayerHitStun, Duration, false);
    ForceNetUpdate();
}

void ACVADCharacter::EndPlayerHitStun()
{
    if (!HasAuthority()) return;
    bPlayerHitStunned = false;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.HitStunned")));
    ApplyPlayerControlState();
    OnPlayerHitStunChanged(false);
    ForceNetUpdate();
}

void ACVADCharacter::OnRep_PlayerHitStunned()
{
    ApplyPlayerControlState();
    OnPlayerHitStunChanged(bPlayerHitStunned);
}

void ACVADCharacter::ApplyPlayerControlState()
{
    const bool bLocked = bPlayerDown || bPlayerHitStunned;
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        if (bLocked) Movement->DisableMovement();
        else if (Movement->MovementMode == MOVE_None) Movement->SetMovementMode(MOVE_Walking);
    }
    if (AController* OwningController = GetController())
    {
        OwningController->SetIgnoreMoveInput(bLocked);
        OwningController->SetIgnoreLookInput(false);
    }
}

void ACVADCharacter::ServerTryReviveNearbyPlayer_Implementation()
{
    if (bPlayerDown || bPlayerHitStunned) { UE_LOG(LogCVADAbilityInput,Warning,TEXT("Revive rejected: reviver is down or stunned")); return; }
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        if (It->BattlePhase==ECVADBattlePhase::Results) { UE_LOG(LogCVADAbilityInput,Warning,TEXT("Revive rejected: battle already ended")); return; }
    ACVADCharacter* Best = nullptr;
    float BestSq = FMath::Square(275.f);
    for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
    {
        if (*It == this || !It->IsPlayerDown()) continue;
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < BestSq) { BestSq = DistanceSq; Best = *It; }
    }
    if (Best) { UE_LOG(LogCVADAbilityInput,Log,TEXT("Revive requested Reviver=%s Target=%s"),*GetName(),*GetNameSafe(Best)); Best->RevivePlayer(0.5f); }
    else UE_LOG(LogCVADAbilityInput,Log,TEXT("Revive requested but no downed player was within range"));
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
    if (bPlayerDown || bPlayerHitStunned)
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
        if (BufferedCombatInput == INDEX_NONE)
        {
            BufferedCombatInput = static_cast<int32>(Input);
            UE_LOG(LogCVADAbilityInput, Log, TEXT("Combat input buffered Slot=%d Window=%s"),
                static_cast<int32>(Input), bComboInputWindowOpen ? TEXT("open") : TEXT("early"));
        }
        else
        {
            UE_LOG(LogCVADAbilityInput, Verbose, TEXT("Combat input ignored: buffer already occupied Slot=%d"),
                BufferedCombatInput);
        }
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

void ACVADCharacter::PlayReplicatedActionAnimation(UAnimSequenceBase* Animation, bool bUseRootMotion)
{
    if (!Animation) return;
    if (HasAuthority()) MulticastPlayActionAnimation(Animation,bUseRootMotion);
}

void ACVADCharacter::QueueAttackDamage(float Damage, float Distance, float Radius, bool bAllowMultipleTargets)
{
    if (!HasAuthority()) return;
    PendingAttackDamage = Damage;
    PendingAttackDistance = Distance * DemoAttackDistanceMultiplier;
    PendingAttackRadius = Radius * DemoAttackRadiusMultiplier;
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

void ACVADCharacter::MulticastPlayActionAnimation_Implementation(UAnimSequenceBase* Animation, bool bUseRootMotion)
{
    PlayActionAnimationLocal(Animation,bUseRootMotion);
}

void ACVADCharacter::PlayActionAnimationLocal(UAnimSequenceBase* Animation, bool bUseRootMotion)
{
    if (!Animation || !GetMesh()) return;
    if (bActionAnimationPlaying)
    {
        if (!PendingActionAnimation)
        {
            PendingActionAnimation = Animation;
            bPendingActionUsesRootMotion = bUseRootMotion;
            UE_LOG(LogCVADAbilityInput, Log, TEXT("Queued one action animation %s"), *GetNameSafe(Animation));
        }
        return;
    }
    StartActionAnimation(Animation,bUseRootMotion);
}

void ACVADCharacter::StartActionAnimation(UAnimSequenceBase* Animation, bool bUseRootMotion)
{
    if (!Animation || !GetMesh()) return;
    // Slot playback keeps the locomotion AnimBP alive, so CharacterMovement, jumping and
    // stance-specific state machines continue evaluating underneath the action animation.
    RestoreLocomotionAnimation();
    bActionAnimationPlaying = true;
    bCurrentActionUsesRootMotion = bUseRootMotion;
    bComboInputWindowOpen = false;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->AddLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
    GetWorldTimerManager().ClearTimer(ActionAnimationTimer);
    UAnimInstance* ActionInstance = GetMesh()->GetAnimInstance();
    UAnimMontage* DynamicMontage = ActionInstance
        ? ActionInstance->PlaySlotAnimationAsDynamicMontage(
            Animation, CombatAnimationSlot, CombatBlendInTime, CombatBlendOutTime, 1.f, 1)
        : nullptr;
    if (!DynamicMontage)
    {
        UE_LOG(LogCVADAbilityInput, Error,
            TEXT("Could not play %s through slot %s. Check the active AnimBP contains the slot node."),
            *GetNameSafe(Animation), *CombatAnimationSlot.ToString());
        bActionAnimationPlaying = false;
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
            ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
        bPendingAttackDamage = false;
        bCombatInputLocked = false;
        return;
    }
    if (ActionInstance)
    {
        // Attacks animate the body but CharacterMovement remains authoritative, allowing move/jump during attacks.
        ActionInstance->SetRootMotionMode(bUseRootMotion
            ? ERootMotionMode::RootMotionFromMontagesOnly
            : ERootMotionMode::IgnoreRootMotion);
    }
    const float Duration = FMath::Max(0.05f, DynamicMontage->GetPlayLength());
    GetWorldTimerManager().ClearTimer(ComboWindowTimer);
    GetWorldTimerManager().SetTimer(ComboWindowTimer, this, &ThisClass::OpenComboInputWindow,
        FMath::Clamp(Duration * ComboWindowStartNormalized, 0.05f, Duration - 0.01f), false);
    if (HasAuthority() && bPendingAttackDamage)
    {
        GetWorldTimerManager().ClearTimer(AttackDamageTimer);
        GetWorldTimerManager().SetTimer(AttackDamageTimer, this, &ThisClass::HandleAttackHitNotify,
            FMath::Clamp(Duration * 0.45f, 0.08f, Duration - 0.02f), false);
    }
    // Notify is authoritative for sequencing. This delayed timer is only a safety fallback.
    GetWorldTimerManager().SetTimer(ActionAnimationTimer, this, &ThisClass::HandleActionAnimationFinished, Duration + 0.15f, false);
    UE_LOG(LogCVADAbilityInput, Log, TEXT("Playing replicated action montage %s Slot=%s Duration=%.2f"),
        *GetNameSafe(Animation), *CombatAnimationSlot.ToString(), Duration);
}

void ACVADCharacter::OpenComboInputWindow()
{
    if (!bActionAnimationPlaying) return;
    bComboInputWindowOpen = true;
    UE_LOG(LogCVADAbilityInput, Log, TEXT("Combo input window opened BufferedSlot=%d"), BufferedCombatInput);
}

void ACVADCharacter::HandleActionAnimationFinished()
{
    // Anim notifies execute while USkeletalMeshComponent is still inside PostAnimEvaluation.
    // Starting another montage or changing the AnimInstance there recurses into evaluation and
    // triggers SkeletalMeshComponent's !bPostEvaluatingAnimation assertion. Always leave that
    // critical section first, including when this function is reached by the safety timer.
    if (!bActionAnimationPlaying || !GetWorld()) return;
    if (!GetWorldTimerManager().IsTimerActive(DeferredActionFinishTimer))
    {
        DeferredActionFinishTimer = GetWorldTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(this, &ThisClass::FinishActionAnimationDeferred));
    }
}

void ACVADCharacter::FinishActionAnimationDeferred()
{
    DeferredActionFinishTimer.Invalidate();
    if (!bActionAnimationPlaying) return;
    GetWorldTimerManager().ClearTimer(ActionAnimationTimer);
    GetWorldTimerManager().ClearTimer(AttackDamageTimer);
    GetWorldTimerManager().ClearTimer(ComboWindowTimer);
    bComboInputWindowOpen = false;
    bActionAnimationPlaying = false;
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->RemoveLooseGameplayTag(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Attacking")));
    bPendingAttackDamage = false;
    if (PendingActionAnimation)
    {
        UAnimSequenceBase* Next = PendingActionAnimation;
        const bool bNextUsesRootMotion=bPendingActionUsesRootMotion;
        PendingActionAnimation = nullptr;
        bPendingActionUsesRootMotion=false;
        StartActionAnimation(Next,bNextUsesRootMotion);
        return;
    }
    bCurrentActionUsesRootMotion=false;
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
    DOREPLIFETIME(ACVADCharacter, bPlayerHitStunned);
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
}

void ACVADCharacter::BindEquipment()
{
    // Kept as a no-op for save compatibility with profiles created before equipment removal.
}

void ACVADCharacter::HandleEquipmentChanged(const FCVADEquipmentLoadout& NewLoadout)
{
    for (USkeletalMeshComponent* Component : {HeadMesh, UpperBodyMesh, LowerBodyMesh, FeetMesh, HandsMesh})
    {
        if (!Component) continue;
        Component->SetSkeletalMesh(nullptr);
        Component->SetHiddenInGame(true);
        Component->SetVisibility(false, true);
    }
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
