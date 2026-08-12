#pragma once

#include "GameplayEffect.h"
#include "CVADDamageEffect.generated.h"

/** Reusable instant damage effect. Magnitude is supplied as a negative Data.Damage SetByCaller value. */
UCLASS()
class CULTIVATIONVSALIENSDEMO_API UCVADDamageEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UCVADDamageEffect();
};
