#pragma once

#include "GameFramework/GameModeBase.h"
#include "CVADGameMode.generated.h"

UCLASS()
class CULTIVATIONVSALIENSDEMO_API ACVADGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACVADGameMode();
    virtual void BeginPlay() override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};
