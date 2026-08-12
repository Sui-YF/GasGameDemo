#include "Animation/CVADAnimNotify_AttackHit.h"
#include "Character/CVADCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UCVADAnimNotify_AttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*,
    const FAnimNotifyEventReference&)
{
    if (ACVADCharacter* Character = MeshComp ? Cast<ACVADCharacter>(MeshComp->GetOwner()) : nullptr)
    {
        Character->HandleAttackHitNotify();
    }
}
