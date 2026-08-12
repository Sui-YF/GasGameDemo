#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CVADAnimNotify_ActionFinished.generated.h"

UCLASS(meta=(DisplayName="CVAD Action Finished"))
class CULTIVATIONVSALIENSDEMO_API UCVADAnimNotify_ActionFinished : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
    virtual FString GetNotifyName_Implementation() const override { return TEXT("Action Finished"); }
};
