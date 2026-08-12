#include "Animation/CVADAnimNotify_ActionFinished.h"
#include "Character/CVADCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UCVADAnimNotify_ActionFinished::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*,
    const FAnimNotifyEventReference&)
{
    if (ACVADCharacter* Character = MeshComp ? Cast<ACVADCharacter>(MeshComp->GetOwner()) : nullptr)
    {
        Character->HandleActionAnimationFinished();
    }
}
