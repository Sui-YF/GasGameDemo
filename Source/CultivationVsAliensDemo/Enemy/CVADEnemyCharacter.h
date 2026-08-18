#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "CVADEnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UCVADAttributeSet;
class ACVADMinionSpawner;
class UAnimInstance;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API ACVADEnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACVADEnemyCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void BeginPlay() override;
    void SetSpawnSource(ACVADMinionSpawner* InSpawnSource);

    UFUNCTION(BlueprintPure, Category="Boss") int32 GetBossPhase() const { return BossPhase; }
    UFUNCTION(BlueprintPure, Category="Boss") bool IsBoss() const { return bIsBoss; }
    UFUNCTION(BlueprintPure, Category="Boss") int32 GetBossRole() const { return BossRole; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Boss") void SetBossRole(int32 NewRole);
    UFUNCTION(BlueprintPure, Category="Combat") bool IsHitStunned() const { return bHitStunned; }
    UFUNCTION(BlueprintPure, Category="Combat") bool IsAttackTelegraphActive() const { return bAttackTelegraphActive; }
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void BeginAttackTelegraph(const FVector& Center, float Radius, float Duration);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void BeginShapedAttackTelegraph(const FVector& Center, float Radius, float Duration, int32 Shape, const FVector& Direction);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void EndAttackTelegraph();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Minion|Animation") void PlayMinionAttackAnimation();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Minion|Animation") void PlayMinionHitAnimation();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Minion|Animation") void PlayMinionDeathAnimation();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Boss|Animation") void PlayBossAttackAnimation();
    UFUNCTION(BlueprintCallable, Category="Ragdoll") void MakeRagdoll();
    UFUNCTION(BlueprintPure, Category="Ragdoll") bool IsRagdollFrozen() const { return bRagdollFrozen; }
    UFUNCTION(BlueprintPure, Category="Boss") bool IsBossDeathSequenceActive() const { return bBossDeathSequenceActive; }
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnEnemyDamaged(float DamageAmount);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnHitStunChanged(bool bNewHitStunned);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnAttackTelegraphChanged(bool bActive, FVector Center, float Radius, float Duration);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Angel") TObjectPtr<class USkeletalMeshComponent> AngelWingLeft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Angel") TObjectPtr<class USkeletalMeshComponent> AngelWingRight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Angel") TObjectPtr<class USkeletalMeshComponent> AngelSword;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<class UAnimSequenceBase> SwordBossAttack;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<class UAnimSequenceBase> WingBossAttack;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Animation") TObjectPtr<class UAnimSequenceBase> CasterBossAttack;
    /** Skeleton-compatible fallback animation for minions that do not ship with an AnimBP. Configure in the enemy Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minion|Animation") TObjectPtr<class UAnimSequenceBase> MinionIdleAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minion|Animation") TObjectPtr<class UAnimSequenceBase> MinionAttackAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minion|Animation") TObjectPtr<class UAnimSequenceBase> MinionHitAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Minion|Animation") TObjectPtr<class UAnimSequenceBase> MinionDeathAnimation;
    /** Optional body mesh for each boss role (0 Sword, 1 Wing Vanguard, 2 Celestial Caster). Configure in the enemy Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Visual") TArray<TObjectPtr<class USkeletalMesh>> BossRoleBodyMeshes;
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayBossAttack(int32 AttackRole);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayMinionAnimation(int32 AnimationType);
    UFUNCTION(NetMulticast, Unreliable) void MulticastRagdoll();
    UFUNCTION(NetMulticast, Unreliable) void MulticastFreezeRagdoll();
    UFUNCTION(NetMulticast, Unreliable) void MulticastBossDeathSequence();
    void FreezeRagdoll();
    void SpawnBossLoot();
    void BeginBossDeathSlowMotion();
    void EndBossDeathSlowMotion();
    void StartBossDeathCamera();
    void EndBossDeathCamera();
    void RestoreBossAnimationBlueprint();
    void RestoreMinionAnimationBlueprint();
    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> BossAnimationClass;
    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> MinionAnimationClass;
    FTimerHandle BossAnimationRestoreTimer;
    FTimerHandle MinionAnimationRestoreTimer;
    /** Rendering/network distance only. The actor is never hidden or destroyed by this setting. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="0.0"))
    float VisualCullDistance = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="0.0"))
    float NetworkCullDistance = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") float HitReactionImpulse = 420.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Demo") bool bOneHitKillMinion = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0")) float MinionHitStunDuration = 0.28f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0.0")) float BossPhaseHitStunDuration = 0.55f;
    /** Bosses move noticeably slower than regular minions so their telegraphed attacks remain readable. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0.1", ClampMax="1.0"))
    float BossMoveSpeedMultiplier = 0.55f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Movement", meta=(ClampMin="0.1", ClampMax="1.0"))
    float EnemyMoveSpeedMultiplier = 0.65f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Animation", meta=(ClampMin="0.1", ClampMax="2.0"))
    float EnemyAnimPlayRate = 0.75f;
    /** Seconds after a ragdoll starts before physics/collision freeze to keep dense waves stable. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="0.5", ClampMax="5.0"))
    float RagdollFreezeDelaySeconds = 2.5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Death", meta=(ClampMin="0.05", ClampMax="1.0"))
    float BossDeathSlowMotionTimeDilation = 0.3f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Death", meta=(ClampMin="0.2", ClampMax="5.0"))
    float BossDeathSlowMotionDurationSeconds = 1.8f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Death", meta=(ClampMin="0.5", ClampMax="8.0"))
    float BossDeathCameraDurationSeconds = 2.6f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Death", meta=(ClampMin="0"))
    int32 BossLootExperience = 500;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss|Death", meta=(ClampMin="0"))
    int32 BossLootSkillPoints = 2;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HitStunned, Category="Combat") bool bHitStunned = false;
    UFUNCTION() void OnRep_HitStunned();
    void BeginHitStun(float Duration);
    void EndHitStun();
    FTimerHandle HitStunTimer;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AttackTelegraph, Category="Combat") bool bAttackTelegraphActive = false;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Combat") FVector_NetQuantize AttackTelegraphCenter;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Combat") float AttackTelegraphRadius = 0.f;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Combat") float AttackTelegraphDuration = 0.f;
    /** 0 circle, 1 forward cone, 2 line corridor. */
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Combat") int32 AttackTelegraphShape = 0;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Combat") FVector_NetQuantizeNormal AttackTelegraphDirection = FVector::ForwardVector;
    UFUNCTION() void OnRep_AttackTelegraph();
    void DrawAttackTelegraphPlaceholder() const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Balance") FName BalanceRowName = TEXT("Minion");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") bool bIsBoss = false;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BossPhase, Category="Boss") int32 BossPhase = 1;
    /** 0 Sword, 1 Wing Vanguard, 2 Celestial Caster. */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BossRole, Category="Boss") int32 BossRole = 0;
    UFUNCTION() void OnRep_BossRole();
    void ApplyBossRoleVisuals();
    UFUNCTION(BlueprintImplementableEvent, Category="Boss") void OnBossRoleChanged(int32 NewRole);
    UFUNCTION() void OnRep_BossPhase();
    UFUNCTION(BlueprintImplementableEvent, Category="Boss") void OnBossPhaseChanged(int32 NewPhase);
    void EvaluateBossPhase(float CurrentHealth);
    void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
    bool bDeathHandled = false;
    bool bApplyingOneHitKill = false;
    bool bRagdollFrozen = false;
    bool bBossDeathSequenceActive = false;
    TWeakObjectPtr<ACVADMinionSpawner> SpawnSource;
    FTimerHandle RagdollFreezeTimer;
    FTimerHandle BossSlowMotionRestoreTimer;
    FTimerHandle BossCameraRestoreTimer;
    TWeakObjectPtr<class ACameraActor> DeathCameraActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UCVADAttributeSet> AttributeSet;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
