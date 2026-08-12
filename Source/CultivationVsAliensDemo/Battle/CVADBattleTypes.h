#pragma once

#include "CoreMinimal.h"
#include "CVADBattleTypes.generated.h"

UENUM(BlueprintType)
enum class ECVADBattlePhase : uint8
{
    Rally,
    Frontline,
    Capture,
    Defense,
    Outposts,
    Boss,
    Results
};
