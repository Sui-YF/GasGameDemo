#include "Enemy/CVADBTTask_Combat.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Enemy/CVADEnemyAIController.h"

UCVADBTTask_Combat::UCVADBTTask_Combat()
{
    NodeName = TEXT("CVAD Acquire / Move / Attack");
    bNotifyTick = true;
}

EBTNodeResult::Type UCVADBTTask_Combat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
    return Cast<ACVADEnemyAIController>(OwnerComp.GetAIOwner()) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UCVADBTTask_Combat::TickTask(UBehaviorTreeComponent& OwnerComp, uint8*, float DeltaSeconds)
{
    ACVADEnemyAIController* AI = Cast<ACVADEnemyAIController>(OwnerComp.GetAIOwner());
    if (!AI)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }
    // No target, hit stun and a locked boss phase are idle states, not BT
    // failures. Keep this mandatory task alive so it resumes immediately when
    // the replicated battle state permits combat.
    AI->ExecuteCombatDecision(DeltaSeconds);
}
