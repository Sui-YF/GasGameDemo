#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Inventory/CVADInventoryTypes.h"
#include "CVADCharacter.generated.h"

class UAbilitySystemComponent;
class USpringArmComponent;
class UCameraComponent;
class UGameplayAbility;
class USkeletalMeshComponent;
class UAnimSequenceBase;
class USphereComponent;
struct FOnAttributeChangeData;
enum class ECVADAbilityInput : uint8;

UCLASS()
class CULTIVATIONVSALIENSDEMO_API ACVADCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACVADCharacter();

    virtual void BeginPlay() override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    void ActivateCombatInput(ECVADAbilityInput Input);

    /** Server-authoritative action animation, replicated to listen-server clients. */
    void PlayReplicatedActionAnimation(UAnimSequenceBase* Animation);
    void HandleActionAnimationFinished();
    void QueueAttackDamage(float Damage, float Distance, float Radius, bool bAllowMultipleTargets);
    void HandleAttackHitNotify();
    UFUNCTION(BlueprintPure, Category="Combat|Stance") bool IsFlyingSwordMode() const { return bFlyingSwordMode; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Stance") void ToggleFlyingSwordMode();
    UFUNCTION(BlueprintPure, Category="Revive") bool IsPlayerDown() const { return bPlayerDown; }
    UFUNCTION(BlueprintPure, Category="Combat") bool IsPlayerHitStunned() const { return bPlayerHitStunned; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Revive") void RevivePlayer(float HealthPercent = 0.5f);
    UFUNCTION(Server, Reliable) void ServerTryReviveNearbyPlayer();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void BeginTemporaryInvulnerability(float Duration);
    UFUNCTION(BlueprintCallable, Category="Movement") void SetSprinting(bool bNewSprinting);
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const { return bSprinting; }

protected:
    void InitializeAbilityActorInfo();
    void GrantDefaultAbilities();
    void BindEquipment();

    UFUNCTION() void HandleEquipmentChanged(const FCVADEquipmentLoadout& NewLoadout);
    void SetEquipmentMesh(USkeletalMeshComponent* Component, FName ItemId);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayActionAnimation(UAnimSequenceBase* Animation);
    void PlayActionAnimationLocal(UAnimSequenceBase* Animation);
    void RestoreLocomotionAnimation();
    void StartActionAnimation(UAnimSequenceBase* Animation);
    void FinishActionAnimationDeferred();
    void OpenComboInputWindow();
    void HandlePlayerHealthChanged(const FOnAttributeChangeData& ChangeData);
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PlayerDown, Category="Revive") bool bPlayerDown = false;
    UFUNCTION() void OnRep_PlayerDown();
    void ApplyDownedState();
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PlayerHitStunned, Category="Combat") bool bPlayerHitStunned = false;
    UFUNCTION() void OnRep_PlayerHitStunned();
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnPlayerHitStunChanged(bool bNewHitStunned);
    void BeginPlayerHitStun(float Duration);
    void EndPlayerHitStun();
    void ApplyPlayerControlState();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0")) float PlayerHitStunDuration = 0.22f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0")) float PlayerHitImpulse = 260.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0")) float PostHitInvulnerabilityDuration = 0.18f;
    FTimerHandle PlayerHitStunTimer;
    UFUNCTION(Server, Reliable) void ServerSetSprinting(bool bNewSprinting);
    UFUNCTION() void OnRep_Sprinting();
    void ApplySprintSpeed();
    void DrainSprintStamina();
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Sprinting, Category="Movement") bool bSprinting = false;
    UPROPERTY(EditDefaultsOnly, Category="Movement") float WalkSpeed = 650.f;
    UPROPERTY(EditDefaultsOnly, Category="Movement") float SprintSpeed = 900.f;
    UPROPERTY(EditDefaultsOnly, Category="Movement") float SprintStaminaPerSecond = 20.f;
    FTimerHandle SprintDrainTimer;

    FTimerHandle ActionAnimationTimer;
    FTimerHandle DeferredActionFinishTimer;
    FTimerHandle AttackDamageTimer;
    FTimerHandle ComboWindowTimer;
    FTimerHandle InvulnerabilityTimer;
    void EndTemporaryInvulnerability();
    UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> PendingActionAnimation;
    bool bActionAnimationPlaying = false;
    bool bComboInputWindowOpen = false;
    bool bCombatInputLocked = false;
    int32 BufferedCombatInput = INDEX_NONE;
    float PendingAttackDamage = 0.f;
    float PendingAttackDistance = 0.f;
    float PendingAttackRadius = 0.f;
    bool bPendingAttackHitsMultiple = false;
    bool bPendingAttackDamage = false;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FlyingSwordMode, Category="Combat|Stance") bool bFlyingSwordMode = false;
    UFUNCTION() void OnRep_FlyingSwordMode();
    void ApplySwordVisualMode();
    UFUNCTION() void HandleFlyingSwordOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    bool bDefaultAbilitiesGranted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> FollowCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment") TObjectPtr<USkeletalMeshComponent> HeadMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment") TObjectPtr<USkeletalMeshComponent> UpperBodyMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment") TObjectPtr<USkeletalMeshComponent> LowerBodyMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment") TObjectPtr<USkeletalMeshComponent> FeetMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment") TObjectPtr<USkeletalMeshComponent> HandsMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USkeletalMeshComponent> SwordMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USkeletalMeshComponent> FlyingSwordMeshLeft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USkeletalMeshComponent> FlyingSwordMeshRight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USphereComponent> SwordCollisionCenter;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USphereComponent> SwordCollisionLeft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<USphereComponent> SwordCollisionRight;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon") float FlyingSwordContactDamage = 20.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.05")) float FlyingSwordHitInterval = 0.5f;
    TMap<TWeakObjectPtr<AActor>, double> FlyingSwordLastHitTimes;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Flying Sword") TSubclassOf<UAnimInstance> FlyingSwordAnimClass;
    UPROPERTY(Transient) TSubclassOf<UAnimInstance> NormalAnimClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Combat") FName CombatAnimationSlot = TEXT("UpperBody");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Combat", meta=(ClampMin="0.0")) float CombatBlendInTime = 0.08f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Combat", meta=(ClampMin="0.0")) float CombatBlendOutTime = 0.12f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Animation|Combat", meta=(ClampMin="0.1", ClampMax="0.95")) float ComboWindowStartNormalized = 0.55f;
};
