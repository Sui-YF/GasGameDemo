#pragma once

#include "AIController.h"
#include "CVADEnemyAIController.generated.h"

UCLASS()
class CULTIVATIONVSALIENSDEMO_API ACVADEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    ACVADEnemyAIController();
    virtual void OnPossess(APawn* InPawn) override;
    void ConfigureCombat(float InDamage, float InAttackInterval);
    void CancelPendingAttack();
    /** Called only by BT tasks; returns true while the target remains actionable. */
    bool ExecuteCombatDecision(float DeltaSeconds);
    void ApplyBossPhase(int32 Phase);
    void ConfigureBossRole(int32 NewBossRole);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI") TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;
    UPROPERTY(EditDefaultsOnly, Category="Combat") float AttackRange = 170.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat") float AttackDamage = 8.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat") float AttackInterval = 1.5f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.05")) float MinionAttackWindup = 0.45f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.05")) float BossAttackWindup = 0.85f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="25.0")) float MinionTelegraphRadius = 130.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="25.0")) float BossTelegraphRadius = 450.f;

private:
    TWeakObjectPtr<AActor> CurrentTarget;
    float AttackCooldown = 0.f;
    int32 ActiveBossPhase = 1;
    int32 BossRole = 0;
    float BaseAttackRange = 170.f;
    float BaseAttackDamage = 8.f;
    float BaseAttackInterval = 1.5f;
    bool bAttackWindingUp = false;
    FVector PendingAttackCenter = FVector::ZeroVector;
    FVector PendingAttackDirection = FVector::ForwardVector;
    int32 PendingAttackShape = 0;
    float PendingAttackRadius = 0.f;
    TWeakObjectPtr<AActor> PendingAttackTarget;
    FTimerHandle AttackWindupTimer;
    AActor* FindNearestPlayer() const;
    void AttackTarget(AActor* TargetActor);
    void ResolveTelegraphedAttack();
    void ApplyBossAreaAttack();
    UBehaviorTree* CreateFallbackBehaviorTree();
};
