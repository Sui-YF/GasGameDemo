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
    /** Called only by BT tasks; returns true while the target remains actionable. */
    bool ExecuteCombatDecision(float DeltaSeconds);
    void ApplyBossPhase(int32 Phase);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI") TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;
    UPROPERTY(EditDefaultsOnly, Category="Combat") float AttackRange = 170.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat") float AttackDamage = 8.f;
    UPROPERTY(EditDefaultsOnly, Category="Combat") float AttackInterval = 1.5f;

private:
    TWeakObjectPtr<AActor> CurrentTarget;
    float AttackCooldown = 0.f;
    int32 ActiveBossPhase = 1;
    AActor* FindNearestPlayer() const;
    void AttackTarget(AActor* TargetActor);
    void ApplyBossAreaAttack();
    UBehaviorTree* CreateFallbackBehaviorTree();
};
