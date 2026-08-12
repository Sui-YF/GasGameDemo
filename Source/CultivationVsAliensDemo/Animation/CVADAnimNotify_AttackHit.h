#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CVADAnimNotify_AttackHit.generated.h"

UCLASS(meta=(DisplayName="CVAD Attack Hit"))
class CULTIVATIONVSALIENSDEMO_API UCVADAnimNotify_AttackHit : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
    virtual FString GetNotifyName_Implementation() const override { return TEXT("Attack Hit"); }
};
