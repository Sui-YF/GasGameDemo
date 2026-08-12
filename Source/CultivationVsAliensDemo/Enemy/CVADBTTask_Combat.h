#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "CVADBTTask_Combat.generated.h"

/** Mandatory Behavior Tree task that owns enemy target acquisition, movement and attacks. */
UCLASS()
class CULTIVATIONVSALIENSDEMO_API UCVADBTTask_Combat : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UCVADBTTask_Combat();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
